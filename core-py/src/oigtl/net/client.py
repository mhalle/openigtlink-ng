"""Async OpenIGTLink client.

Mirrors the *capability* of ``oigtl::Client`` in
``core-cpp/include/oigtl/client.hpp`` — typed send/receive, a
dispatch loop for callback-style code, async-context-manager entry,
and opt-in resilience (auto-reconnect, offline buffer, TCP
keepalive, lifecycle callbacks).

Researcher-first happy path::

    async with await Client.connect("tracker.lab", 18944) as c:
        await c.send(Transform(matrix=[...]))
        env = await c.receive(Status)
        print(env.body.status_message)

Resilient configuration for flaky networks::

    opt = ClientOptions(
        auto_reconnect=True,
        tcp_keepalive=True,
        offline_buffer_capacity=100,
        offline_overflow_policy=OfflineOverflow.DROP_OLDEST,
    )
    c = await Client.connect("tracker.lab", 18944, opt)

    c.on_disconnected(lambda exc: metrics.increment("disconnects"))
    c.on_connected(lambda: metrics.increment("reconnects"))
"""

from __future__ import annotations

import asyncio
import contextlib
import random
from datetime import timedelta
from typing import (
    Any,
    AsyncIterator,
    Awaitable,
    Callable,
    TypeVar,
)

from pydantic import BaseModel

from oigtl.codec import RawBody, pack_envelope, unpack_message
from oigtl.net._options import (
    ClientOptions,
    Envelope,
    RawMessage,
    as_timedelta,
)
from oigtl.net._resilience import (
    OfflineBuffer,
    compute_backoff,
    configure_keepalive,
)
from oigtl.net.errors import (
    ConnectionClosedError,
    FramingError,
    TimeoutError as NetTimeoutError,
)
from oigtl.runtime.exceptions import ProtocolError
from oigtl.runtime.header import HEADER_SIZE, pack_header, unpack_header

__all__ = ["Client"]

M = TypeVar("M", bound=BaseModel)
Handler = Callable[[Envelope[Any]], Awaitable[None]]

ConnectedCallback = Callable[[], Awaitable[None] | None]
DisconnectedCallback = Callable[
    [BaseException | None], Awaitable[None] | None,
]
ReconnectFailedCallback = Callable[
    [int, timedelta], Awaitable[None] | None,
]


class Client:
    """Async OpenIGTLink client with opt-in resilience.

    Construct via :meth:`connect` (classmethod, awaits TCP dial).
    Close via :meth:`close` or the ``async with`` context manager.
    Every I/O method is a coroutine; the blocking surface lives in
    :class:`~oigtl.net.SyncClient`.

    Concurrency: ``send()`` / ``receive()`` are safe from multiple
    tasks — a send lock serialises writes, and reads are naturally
    serialised by the stream. When :attr:`ClientOptions.auto_reconnect`
    is True, a background reconnect task watches the connection
    and re-dials on drops; during the outage, ``send()`` may buffer
    (per :attr:`~ClientOptions.offline_overflow_policy`) and
    ``receive()`` blocks until the new connection is up.
    """

    # --------------------------------------------------------------
    # Construction / teardown
    # --------------------------------------------------------------

    def __init__(
        self,
        *,
        host: str,
        port: int,
        reader: asyncio.StreamReader,
        writer: asyncio.StreamWriter,
        options: ClientOptions,
    ) -> None:
        self._host = host
        self._port = port
        self._reader = reader
        self._writer = writer
        self._options = options

        self._send_lock = asyncio.Lock()
        self._closed = asyncio.Event()
        self._run_stop = asyncio.Event()

        # Resilience state.
        self._connected = asyncio.Event()
        self._connected.set()
        self._terminal = False
        self._terminal_reason: BaseException | None = None
        self._reconnect_task: asyncio.Task | None = None
        self._reconnect_attempt = 0
        self._monitor_task: asyncio.Task | None = None
        # Buffer of received envelopes that the watchdog reads
        # speculatively. Populated by the monitor so drops are
        # detected promptly even without an active receive() call;
        # drained by _receive_one() when the caller asks for a
        # message. Only used under auto_reconnect.
        self._incoming: asyncio.Queue[Envelope[BaseModel]] | None = None
        self._offline_buffer = (
            OfflineBuffer(options) if options.auto_reconnect else None
        )
        self._rng = random.Random()

        # Dispatch handlers (type_id → coroutine).
        self._handlers: dict[str, Handler] = {}
        self._unknown_handler: Handler | None = None
        self._error_handler: Callable[[BaseException], Awaitable[None]] | None = None

        # Lifecycle callbacks.
        self._on_connected: ConnectedCallback | None = None
        self._on_disconnected: DisconnectedCallback | None = None
        self._on_reconnect_failed: ReconnectFailedCallback | None = None

        if options.auto_reconnect:
            self._incoming = asyncio.Queue()

        # Apply keepalive now that we have a socket.
        self._apply_keepalive()

        # Under auto_reconnect, spawn a watchdog reader so drops are
        # detected even when no caller is actively receiving.
        if options.auto_reconnect:
            self._start_monitor()

    @classmethod
    async def connect(
        cls,
        host: str,
        port: int,
        options: ClientOptions | None = None,
    ) -> "Client":
        """Open a TCP connection and return a ready :class:`Client`."""
        opt = options or ClientOptions()
        reader, writer = await cls._dial(host, port, opt)
        return cls(
            host=host, port=port,
            reader=reader, writer=writer,
            options=opt,
        )

    @classmethod
    async def _dial(
        cls,
        host: str,
        port: int,
        opt: ClientOptions,
    ) -> tuple[asyncio.StreamReader, asyncio.StreamWriter]:
        """Open a fresh TCP connection. Caller decides what to do on failure."""
        try:
            coro = asyncio.open_connection(host, port)
            if opt.connect_timeout is None:
                return await coro
            return await asyncio.wait_for(
                coro, timeout=opt.connect_timeout.total_seconds(),
            )
        except asyncio.TimeoutError as e:
            raise NetTimeoutError(
                f"connect to {host}:{port} timed out after "
                f"{opt.connect_timeout}"
            ) from e
        except OSError as e:
            raise ConnectionClosedError(
                f"connect to {host}:{port} failed: {e}"
            ) from e

    @classmethod
    def connect_sync(
        cls,
        host: str,
        port: int,
        options: ClientOptions | None = None,
    ) -> "oigtl.net.sync_client.SyncClient":  # noqa: F821
        """Synchronous counterpart to :meth:`connect`.

        Returns a :class:`~oigtl.net.sync_client.SyncClient` — the
        blocking façade whose methods don't need ``await``.
        """
        from oigtl.net.sync_client import SyncClient
        return SyncClient.connect(host, port, options)

    # --------------------------------------------------------------
    # Introspection
    # --------------------------------------------------------------

    @property
    def options(self) -> ClientOptions:
        return self._options

    @property
    def peer(self) -> tuple[str, int] | None:
        info = self._writer.get_extra_info("peername")
        if info is None:
            return None
        return info[0], info[1]

    @property
    def is_connected(self) -> bool:
        """True iff a working transport is currently live.

        False during reconnect outages even when :attr:`close` hasn't
        been called.
        """
        return self._connected.is_set() and not self._closed.is_set()

    def _apply_keepalive(self) -> None:
        """Set SO_KEEPALIVE and friends on the current socket."""
        if not self._options.tcp_keepalive:
            return
        sock = self._writer.get_extra_info("socket")
        if sock is None:
            return
        try:
            configure_keepalive(
                sock,
                idle=self._options.tcp_keepalive_idle,
                interval=self._options.tcp_keepalive_interval,
                count=self._options.tcp_keepalive_count,
            )
        except OSError:
            # Unsupported on this platform/socket — keepalive is a
            # best-effort hint, not a correctness requirement.
            pass

    # --------------------------------------------------------------
    # Close / context manager
    # --------------------------------------------------------------

    async def close(self) -> None:
        """Close the underlying TCP connection.

        Safe to call repeatedly. Also cancels any in-flight reconnect
        task and wakes a running :meth:`run` loop.
        """
        if self._closed.is_set():
            return
        self._closed.set()
        self._run_stop.set()
        self._connected.clear()

        if self._monitor_task is not None:
            self._monitor_task.cancel()
            with contextlib.suppress(asyncio.CancelledError, Exception):
                await self._monitor_task
            self._monitor_task = None

        if self._reconnect_task is not None:
            self._reconnect_task.cancel()
            with contextlib.suppress(asyncio.CancelledError, Exception):
                await self._reconnect_task

        try:
            self._writer.close()
            with contextlib.suppress(Exception):
                await self._writer.wait_closed()
        except Exception:
            pass

        # Drop any buffered messages; a closed client doesn't send.
        if self._offline_buffer is not None:
            self._offline_buffer.clear()

    async def __aenter__(self) -> "Client":
        return self

    async def __aexit__(self, *_: object) -> None:
        await self.close()

    # --------------------------------------------------------------
    # Send
    # --------------------------------------------------------------

    async def send(
        self,
        message: BaseModel,
        *,
        device_name: str | None = None,
        timestamp: int = 0,
    ) -> None:
        """Frame and transmit *message*.

        Behaviour under resilience:

        - Connected: framed and sent normally.
        - Disconnected + ``auto_reconnect=True``: framed bytes go
          into the offline buffer per the configured overflow policy.
          Drained on reconnect before any live sends.
        - Disconnected + ``auto_reconnect=False``: raises
          :class:`ConnectionClosedError`.
        - Terminal (reconnect attempts exhausted): raises
          :class:`ConnectionClosedError`.
        """
        if self._terminal or self._closed.is_set():
            raise ConnectionClosedError(
                "client is closed or reconnect attempts exhausted"
            )

        type_id = getattr(type(message), "TYPE_ID", None)
        if not isinstance(type_id, str):
            raise TypeError(
                f"{type(message).__name__} has no TYPE_ID; not a "
                f"generated OpenIGTLink message class?"
            )

        body = message.pack()
        # We emit v1 framing: the 58-byte header followed directly
        # by the message body, no extended-header region. Declaring
        # version=2 with a bare v1-style body is spec-inconsistent
        # (v2 prescribes a 12-byte extended header ahead of the
        # content) — a strict v2 parser reads the first body bytes
        # as ext_header_size and fails. When this Client grows v2
        # emission (for metadata / message_id correlation), it will
        # need to prepend the extended header + append the metadata
        # region alongside bumping the declared version.
        header = pack_header(
            version=1,
            type_id=type_id,
            device_name=device_name or self._options.default_device,
            timestamp=timestamp,
            body=body,
        )
        wire = header + body

        await self._send_wire(wire)

    async def send_raw(self, msg: RawMessage) -> None:
        """Send already-framed wire bytes.

        Escape hatch for gateways and middleware: moves a
        :class:`RawMessage` from one transport to another without a
        decode / re-encode round trip. The caller is responsible
        for producing valid OIGTL framing — typically this
        ``RawMessage`` came from another endpoint's
        :meth:`raw_messages` iterator.

        Same resilience semantics as :meth:`send` (buffers during
        outages when ``auto_reconnect`` is on, raises otherwise).
        """
        if self._terminal or self._closed.is_set():
            raise ConnectionClosedError(
                "client is closed or reconnect attempts exhausted"
            )
        await self._send_wire(msg.wire)

    async def _send_wire(self, wire: bytes) -> None:
        """Send already-framed bytes, honouring resilience policy.

        Factored out so the offline-buffer drain path can reuse it
        without re-packing the message.
        """
        # Fast path: connection is live. Try to send under the lock.
        if self.is_connected:
            async with self._send_lock:
                if self.is_connected:
                    try:
                        self._writer.write(wire)
                        await self._writer.drain()
                        return
                    except (ConnectionError, BrokenPipeError, OSError) as e:
                        # Send failed — fall through to the buffered
                        # path if resilience is on. Mark the
                        # connection lost first.
                        await self._handle_drop(cause=e)

        # Disconnected path.
        if self._offline_buffer is None:
            # auto_reconnect off — the caller asked not to buffer.
            raise ConnectionClosedError(
                "client is disconnected and auto_reconnect is off"
            )

        # Enqueue and let the reconnect worker drain.
        await self._offline_buffer.push(wire)

    async def _handle_drop(self, *, cause: BaseException | None) -> None:
        """Transition to the disconnected state and schedule reconnect.

        Idempotent: re-entering while already disconnected is a no-op.
        """
        if not self._connected.is_set():
            return
        self._connected.clear()

        # Fire on_disconnected. Supports both sync and async callbacks.
        if self._on_disconnected is not None:
            result = self._on_disconnected(cause)
            if asyncio.iscoroutine(result):
                await result

        if (self._options.auto_reconnect
                and not self._closed.is_set()
                and self._reconnect_task is None):
            self._reconnect_task = asyncio.create_task(
                self._reconnect_loop(),
                name=f"oigtl-reconnect-{self._host}:{self._port}",
            )

    async def _reconnect_loop(self) -> None:
        """Background task: redial with exponential backoff.

        Exits on success (reinstalls reader/writer, fires
        on_connected, drains buffer), on terminal (max_attempts
        exhausted, marks client terminal), or on cancellation (Client
        being closed).
        """
        self._reconnect_attempt = 0
        opt = self._options
        try:
            while not self._closed.is_set():
                self._reconnect_attempt += 1

                # Wait for backoff (except first attempt which
                # proceeds after the initial drop immediately via
                # reconnect_initial_backoff itself).
                delay = compute_backoff(
                    self._reconnect_attempt, opt, rng=self._rng,
                )
                await asyncio.sleep(delay.total_seconds())

                if self._closed.is_set():
                    return

                try:
                    reader, writer = await self._dial(
                        self._host, self._port, opt,
                    )
                except (ConnectionClosedError, NetTimeoutError) as e:
                    # Failed attempt — fire callback, check limit.
                    if self._on_reconnect_failed is not None:
                        result = self._on_reconnect_failed(
                            self._reconnect_attempt, delay,
                        )
                        if asyncio.iscoroutine(result):
                            await result

                    if (opt.reconnect_max_attempts > 0
                            and self._reconnect_attempt
                            >= opt.reconnect_max_attempts):
                        self._terminal = True
                        self._terminal_reason = e
                        if self._offline_buffer is not None:
                            self._offline_buffer.clear()
                            # Wake any blocked BLOCK-policy senders.
                            await self._offline_buffer.notify_space()
                        return
                    continue

                # Success — cancel the old monitor (bound to the
                # defunct reader), install the new stream, drain,
                # announce, restart the monitor.
                if self._monitor_task is not None:
                    self._monitor_task.cancel()
                    with contextlib.suppress(Exception):
                        await self._monitor_task
                    self._monitor_task = None

                async with self._send_lock:
                    self._reader = reader
                    self._writer = writer
                    self._apply_keepalive()

                # Drain buffered messages before releasing live sends.
                if self._offline_buffer is not None:
                    try:
                        await self._offline_buffer.drain(
                            self._drain_one,
                        )
                    except (ConnectionError, BrokenPipeError, OSError):
                        # Drain broke the connection. Loop back around
                        # and retry.
                        self._connected.clear()
                        continue

                self._reconnect_attempt = 0
                self._connected.set()
                # Restart the monitor on the new reader.
                self._start_monitor()

                if self._on_connected is not None:
                    result = self._on_connected()
                    if asyncio.iscoroutine(result):
                        await result
                return
        finally:
            self._reconnect_task = None

    async def _drain_one(self, wire: bytes) -> None:
        """Write one buffered wire message during reconnect drain."""
        async with self._send_lock:
            self._writer.write(wire)
            await self._writer.drain()

    # --------------------------------------------------------------
    # Monitor — proactive drop detection under auto_reconnect.
    # --------------------------------------------------------------

    def _start_monitor(self) -> None:
        """Spawn a background reader that detects drops even when no
        caller is actively awaiting receive(). Populates ``_incoming``
        so the next receive() returns a pre-read envelope instead of
        racing the monitor for the same bytes."""
        if self._monitor_task is not None and not self._monitor_task.done():
            return
        self._monitor_task = asyncio.create_task(
            self._monitor_loop(),
            name=f"oigtl-monitor-{self._host}:{self._port}",
        )

    async def _monitor_loop(self) -> None:
        """Read messages into the shared queue; react to EOF by
        triggering the reconnect flow.

        Exits when the client is closed. On drop, handles it and
        waits for the reconnect event before resuming reads on the
        new stream.
        """
        assert self._incoming is not None
        while not self._closed.is_set():
            # Wait for a live connection before reading.
            if not self._connected.is_set():
                try:
                    await asyncio.wait_for(
                        self._connected.wait(), timeout=None,
                    )
                except asyncio.CancelledError:
                    return
                continue

            try:
                env = await self._receive_one_from_stream()
            except ConnectionClosedError as e:
                await self._handle_drop(cause=e)
                continue
            except BaseException:
                # CRC, framing, decode errors — let the queued caller
                # see them by funnelling the exception through the
                # queue as a special sentinel. Simplest: just propagate
                # by marking drop; the C++ side has the same rule that
                # bad bytes close the connection.
                await self._handle_drop(cause=None)
                continue
            try:
                await self._incoming.put(env)
            except asyncio.CancelledError:
                return

    # --------------------------------------------------------------
    # Receive
    # --------------------------------------------------------------

    async def receive_any(
        self,
        *,
        timeout: timedelta | float | int | None = None,
    ) -> Envelope[BaseModel]:
        """Receive the next message of any registered type."""
        budget = as_timedelta(timeout) or self._options.receive_timeout
        coro = self._receive_with_reconnect()
        if budget is None:
            return await coro
        try:
            return await asyncio.wait_for(
                coro, timeout=budget.total_seconds(),
            )
        except asyncio.TimeoutError as e:
            raise NetTimeoutError(
                f"receive_any timed out after {budget}"
            ) from e

    async def receive(
        self,
        message_type: type[M],
        *,
        timeout: timedelta | float | int | None = None,
    ) -> Envelope[M]:
        """Receive until a message of *message_type* arrives."""
        expected_type_id = getattr(message_type, "TYPE_ID", None)
        if not isinstance(expected_type_id, str):
            raise TypeError(
                f"{message_type.__name__} has no TYPE_ID; not a "
                f"generated OpenIGTLink message class?"
            )

        budget = as_timedelta(timeout) or self._options.receive_timeout
        loop = asyncio.get_running_loop()
        deadline = (
            loop.time() + budget.total_seconds()
            if budget is not None else None
        )

        while True:
            if deadline is not None:
                remaining = deadline - loop.time()
                if remaining <= 0:
                    raise NetTimeoutError(
                        f"receive({message_type.__name__}) timed out "
                        f"after {budget}"
                    )
                try:
                    env = await asyncio.wait_for(
                        self._receive_with_reconnect(),
                        timeout=remaining,
                    )
                except asyncio.TimeoutError as e:
                    raise NetTimeoutError(
                        f"receive({message_type.__name__}) timed out "
                        f"after {budget}"
                    ) from e
            else:
                env = await self._receive_with_reconnect()

            if env.header.type_id == expected_type_id:
                return env     # type: ignore[return-value]
            await self._dispatch(env)

    async def messages(self) -> AsyncIterator[Envelope[BaseModel]]:
        """Async iterator yielding every received message, in order."""
        try:
            while not self._closed.is_set():
                yield await self._receive_with_reconnect()
        except ConnectionClosedError:
            return

    async def raw_messages(self) -> AsyncIterator[RawMessage]:
        """Async iterator yielding raw (header + wire bytes) messages.

        Gateway-friendly counterpart to :meth:`messages`. Skips body
        decoding entirely — the iterator yields :class:`RawMessage`
        instances whose ``wire`` bytes can be pushed straight into
        another :meth:`send_raw` call on any transport. Useful when
        the intermediary doesn't care about message types.
        """
        try:
            while not self._closed.is_set():
                env = await self._receive_with_reconnect()
                # Rebuild the wire bytes from the header + body.
                # The monitor already decoded both; we re-pack the
                # header with the original timestamp/device_name/
                # type_id and the body bytes the peer sent.
                yield self._envelope_to_raw(env)
        except ConnectionClosedError:
            return

    def _envelope_to_raw(self, env: Envelope[BaseModel]) -> RawMessage:
        """Reconstruct the wire bytes from a decoded envelope.

        The receive path decodes the body for typed dispatch; to
        offer a byte-pipe iterator we have to reconstruct the wire.
        Alternative: keep raw bytes alongside the decoded envelope
        in the queue. That's the right optimisation if raw_messages
        ever becomes the hot path, but the simple approach is fine
        for now — gateway users rarely care about latency.
        """
        wire = pack_envelope(env)
        return RawMessage(header=env.header, wire=wire)

    async def _receive_with_reconnect(self) -> Envelope[BaseModel]:
        """Wrap ``_receive_one`` with outage-aware retry.

        If we're mid-reconnect and not terminal, wait for the new
        connection before reading. If we're terminal, raise.
        """
        while True:
            if self._terminal:
                raise ConnectionClosedError(
                    "reconnect attempts exhausted"
                ) from self._terminal_reason
            if self._closed.is_set():
                raise ConnectionClosedError("client is closed")

            if not self._connected.is_set():
                # Wait for reconnect. If auto_reconnect is off, this
                # event will never be re-set — the caller's
                # outer timeout is the exit.
                await self._connected.wait()
                continue

            try:
                return await self._receive_one()
            except ConnectionClosedError as e:
                if not self._options.auto_reconnect:
                    raise
                await self._handle_drop(cause=e)
                # Loop: wait for _connected.

    async def _receive_one(self) -> Envelope[BaseModel]:
        """Return the next message.

        Under auto_reconnect a background monitor has already read
        messages into the queue; we just pop the next one and let
        the monitor keep handling drops. Without auto_reconnect we
        read directly from the stream.
        """
        if self._incoming is not None:
            return await self._incoming.get()
        return await self._receive_one_from_stream()

    async def _receive_one_from_stream(self) -> Envelope[BaseModel]:
        """Read exactly one framed message directly off the stream."""
        try:
            header_bytes = await self._reader.readexactly(HEADER_SIZE)
        except asyncio.IncompleteReadError as e:
            raise ConnectionClosedError(
                f"peer closed after {len(e.partial)} of {HEADER_SIZE} "
                f"header bytes"
            ) from e
        except (ConnectionError, OSError) as e:
            raise ConnectionClosedError(f"recv failed: {e}") from e

        try:
            header = unpack_header(header_bytes)
        except ValueError as e:
            raise ProtocolError(str(e)) from e

        if (self._options.max_message_size
                and header.body_size > self._options.max_message_size):
            raise FramingError(
                f"body_size {header.body_size} exceeds "
                f"max_message_size {self._options.max_message_size}"
            )

        try:
            body = await self._reader.readexactly(header.body_size)
        except asyncio.IncompleteReadError as e:
            raise ConnectionClosedError(
                f"peer closed mid-body: got {len(e.partial)} of "
                f"{header.body_size}"
            ) from e

        # Hand off to the pure codec: CRC check + type dispatch +
        # Envelope construction in one call. loose=True so unknown
        # type_ids surface as RawBody rather than raising — the
        # client's receive loop should be resilient to forward-compat
        # message types it doesn't know yet.
        return unpack_message(header, body, loose=True, verify_crc=True)

    # --------------------------------------------------------------
    # Dispatch
    # --------------------------------------------------------------

    def on(
        self,
        message_type: type[M],
    ) -> Callable[[Handler], Handler]:
        """Register a handler for *message_type*; usable as decorator."""
        type_id = getattr(message_type, "TYPE_ID", None)
        if not isinstance(type_id, str):
            raise TypeError(
                f"{message_type.__name__} has no TYPE_ID; not a "
                f"generated OpenIGTLink message class?"
            )

        def register(handler: Handler) -> Handler:
            self._handlers[type_id] = handler
            return handler

        return register

    def on_unknown(self, handler: Handler) -> Handler:
        self._unknown_handler = handler
        return handler

    def on_error(
        self,
        handler: Callable[[BaseException], Awaitable[None]],
    ) -> Callable[[BaseException], Awaitable[None]]:
        self._error_handler = handler
        return handler

    # ---- Lifecycle callbacks (resilience) ------------------------

    def on_connected(
        self, handler: ConnectedCallback,
    ) -> ConnectedCallback:
        """Called after every successful connect (initial + reconnect).

        Handler can be sync or async. Use it to metrics-count, re-
        subscribe to topics, or reset a staleness watchdog.
        """
        self._on_connected = handler
        return handler

    def on_disconnected(
        self, handler: DisconnectedCallback,
    ) -> DisconnectedCallback:
        """Called on drop. Receives the underlying exception or None."""
        self._on_disconnected = handler
        return handler

    def on_reconnect_failed(
        self, handler: ReconnectFailedCallback,
    ) -> ReconnectFailedCallback:
        """Called after each failed reconnect attempt.

        Handler receives ``(attempt_number, next_delay)``. Useful for
        exposing "we're at attempt 5" to an operator UI.
        """
        self._on_reconnect_failed = handler
        return handler

    async def run(self) -> None:
        """Dispatch loop — read messages and route to handlers.

        Returns on :meth:`close` or peer FIN (when auto_reconnect is
        off, or when reconnects exhaust).
        """
        while not self._run_stop.is_set():
            try:
                env = await self._receive_with_reconnect()
            except ConnectionClosedError:
                return
            except BaseException as e:
                if self._error_handler is not None:
                    await self._error_handler(e)
                    continue
                raise

            try:
                await self._dispatch(env)
            except BaseException as e:
                if self._error_handler is not None:
                    await self._error_handler(e)
                else:
                    raise

    async def _dispatch(self, env: Envelope[Any]) -> None:
        handler = self._handlers.get(env.header.type_id)
        if handler is not None:
            await handler(env)
        elif self._unknown_handler is not None:
            await self._unknown_handler(env)


# Backwards-compatible alias — the sentinel body class moved to
# :mod:`oigtl.codec` so the public codec and the transport layers
# share a single definition. New code should import :class:`RawBody`
# directly from ``oigtl.codec`` (or from the top-level ``oigtl``
# re-export).
_RawBody = RawBody
