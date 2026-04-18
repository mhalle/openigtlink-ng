"""Async OpenIGTLink client.

Mirrors the *capability* of ``oigtl::Client`` in
``core-cpp/include/oigtl/client.hpp`` — typed send/receive, a
dispatch loop for callback-style code, and an async-context-manager
entry. Resilience features (auto-reconnect, offline buffer, TCP
keepalive) are additive in Phase 4; this module is the happy-path
foundation.

Researcher-first API:

    async with await Client.connect("tracker.lab", 18944) as c:
        await c.send(Transform(matrix=[...]))
        env = await c.receive(Status)
        print(env.body.status_message)

Dispatch loop:

    c = await Client.connect("tracker.lab", 18944)

    @c.on(Transform)
    async def _(env):
        renderer.update_pose(env.body.matrix)

    @c.on(Status)
    async def _(env):
        if env.body.code != 1:
            log.error(env.body.status_message)

    await c.run()           # blocks until peer closes or c.close()
"""

from __future__ import annotations

import asyncio
import contextlib
from datetime import timedelta
from typing import (
    Any,
    AsyncIterator,
    Awaitable,
    Callable,
    TypeVar,
)

from pydantic import BaseModel

from oigtl.messages import REGISTRY as _MESSAGE_REGISTRY
from oigtl.net._options import ClientOptions, Envelope, as_timedelta
from oigtl.net.errors import (
    ConnectionClosedError,
    FramingError,
    TimeoutError as NetTimeoutError,
)
from oigtl.runtime.exceptions import CrcMismatchError, ProtocolError
from oigtl.runtime.header import HEADER_SIZE, Header, pack_header, unpack_header
from oigtl_corpus_tools.codec.crc64 import crc64

__all__ = ["Client"]

M = TypeVar("M", bound=BaseModel)
Handler = Callable[[Envelope[Any]], Awaitable[None]]


class Client:
    """Async OpenIGTLink client.

    Construct via :meth:`connect` (a classmethod that awaits the
    TCP dial). Close via :meth:`close` or the ``async with``
    context manager. Every I/O method is a coroutine; there's no
    sync surface here (the sync wrapper lands in Phase 3).

    All state lives on the instance; the class is not shareable
    across event loops. Sending and receiving concurrently from
    different tasks is safe — a single writer lock serialises
    sends, and reads are naturally serialised by the stream.
    """

    # --------------------------------------------------------------
    # Construction / teardown
    # --------------------------------------------------------------

    def __init__(
        self,
        *,
        reader: asyncio.StreamReader,
        writer: asyncio.StreamWriter,
        options: ClientOptions,
    ) -> None:
        # Intentionally private — callers use Client.connect(). The
        # alternative "construct then connect" split exists in C++
        # for symmetry with adopt(); in Python we use a classmethod
        # because that's the idiomatic "async constructor".
        self._reader = reader
        self._writer = writer
        self._options = options

        self._send_lock = asyncio.Lock()
        self._closed = asyncio.Event()
        self._run_stop = asyncio.Event()

        # type_id → handler. Populated by @c.on(MessageType).
        self._handlers: dict[str, Handler] = {}
        self._unknown_handler: Handler | None = None
        self._error_handler: Callable[[BaseException], Awaitable[None]] | None = None

    @classmethod
    async def connect(
        cls,
        host: str,
        port: int,
        options: ClientOptions | None = None,
    ) -> "Client":
        """Open a TCP connection and return a ready :class:`Client`.

        Honours ``options.connect_timeout``. Raises
        :class:`~oigtl.net.errors.TimeoutError` on budget exhaustion,
        :class:`~oigtl.net.errors.ConnectionClosedError` on other
        connect failures. Never returns a half-open handle.
        """
        opt = options or ClientOptions()
        try:
            coro = asyncio.open_connection(host, port)
            if opt.connect_timeout is None:
                reader, writer = await coro
            else:
                reader, writer = await asyncio.wait_for(
                    coro,
                    timeout=opt.connect_timeout.total_seconds(),
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

        return cls(reader=reader, writer=writer, options=opt)

    @property
    def options(self) -> ClientOptions:
        """Current options (read-only snapshot)."""
        return self._options

    @property
    def peer(self) -> tuple[str, int] | None:
        """``(host, port)`` of the remote end, or None if not connected."""
        info = self._writer.get_extra_info("peername")
        if info is None:
            return None
        # IPv6 peers give (host, port, flowinfo, scopeid); we project
        # the researcher-relevant pair.
        return info[0], info[1]

    async def close(self) -> None:
        """Close the underlying TCP connection.

        Safe to call repeatedly. Wakes any in-flight ``run()`` loop
        at the next dispatch tick and unblocks a pending
        ``receive()`` with :class:`ConnectionClosedError`.
        """
        if self._closed.is_set():
            return
        self._closed.set()
        self._run_stop.set()
        try:
            self._writer.close()
            with contextlib.suppress(Exception):
                await self._writer.wait_closed()
        except Exception:
            # wait_closed can raise on a pre-broken socket; we're
            # tearing down anyway.
            pass

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

        *message* must be one of the generated types in
        :mod:`oigtl.messages`; it carries its own ``TYPE_ID``.
        ``device_name`` defaults to ``options.default_device``.
        ``timestamp`` is the OpenIGTLink 64-bit wire timestamp —
        callers who want "now" should produce it via
        :func:`oigtl_corpus_tools.codec.timestamp.now_igtl` or
        equivalent; this module stays policy-free on clocks.

        Raises :class:`ConnectionClosedError` if the peer has gone
        away.
        """
        type_id = getattr(type(message), "TYPE_ID", None)
        if not isinstance(type_id, str):
            raise TypeError(
                f"{type(message).__name__} has no TYPE_ID; not a "
                f"generated OpenIGTLink message class?"
            )

        body = message.pack()
        header = pack_header(
            version=2,
            type_id=type_id,
            device_name=device_name or self._options.default_device,
            timestamp=timestamp,
            body=body,
        )

        async with self._send_lock:
            if self._closed.is_set():
                raise ConnectionClosedError("client is closed")
            try:
                self._writer.write(header + body)
                await self._writer.drain()
            except (ConnectionError, BrokenPipeError, OSError) as e:
                await self._mark_broken()
                raise ConnectionClosedError(f"send failed: {e}") from e

    async def _mark_broken(self) -> None:
        """Internal: flag the connection as gone without awaiting close.

        Used when send/recv hit an error mid-operation. Phase 4
        extends this to trigger the reconnect worker.
        """
        self._closed.set()
        self._run_stop.set()

    # --------------------------------------------------------------
    # Receive
    # --------------------------------------------------------------

    async def receive_any(
        self,
        *,
        timeout: timedelta | float | int | None = None,
    ) -> Envelope[BaseModel]:
        """Receive the next message of any registered type.

        Returns an :class:`Envelope` whose ``body`` is a typed
        instance when the wire ``type_id`` is registered, or a bare
        ``dict`` when it isn't — callers that only speak specific
        types should prefer :meth:`receive(T)`.

        Raises :class:`ConnectionClosedError` on peer-FIN,
        :class:`~oigtl.net.errors.TimeoutError` if the budget is
        exceeded.
        """
        budget = as_timedelta(timeout) or self._options.receive_timeout
        coro = self._receive_one()
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
        """Receive until a message of *message_type* arrives.

        Intermediate messages that match a registered ``@on()``
        handler are dispatched to it; those that don't are dropped.
        Callers that want the full stream should use
        :meth:`messages` or :meth:`receive_any`.

        Raises :class:`ConnectionClosedError` on peer-FIN,
        :class:`~oigtl.net.errors.TimeoutError` if the budget is
        exceeded before a matching message arrives.
        """
        expected_type_id = getattr(message_type, "TYPE_ID", None)
        if not isinstance(expected_type_id, str):
            raise TypeError(
                f"{message_type.__name__} has no TYPE_ID; not a "
                f"generated OpenIGTLink message class?"
            )

        budget = as_timedelta(timeout) or self._options.receive_timeout
        # Compute a single overall deadline so dispatched-and-ignored
        # messages don't reset the budget.
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
                        self._receive_one(), timeout=remaining,
                    )
                except asyncio.TimeoutError as e:
                    raise NetTimeoutError(
                        f"receive({message_type.__name__}) timed out "
                        f"after {budget}"
                    ) from e
            else:
                env = await self._receive_one()

            if env.header.type_id == expected_type_id:
                # The body is the type the caller asked for.
                return env     # type: ignore[return-value]

            # Not our target — dispatch if registered, else drop.
            await self._dispatch(env)

    async def messages(self) -> AsyncIterator[Envelope[BaseModel]]:
        """Async iterator yielding every received message, in order.

        Exits cleanly on peer-FIN or ``close()``. Errors propagate;
        callers that want them swallowed should wrap in a try
        inside the loop.
        """
        try:
            while not self._closed.is_set():
                yield await self._receive_one()
        except ConnectionClosedError:
            return

    async def _receive_one(self) -> Envelope[BaseModel]:
        """Read exactly one framed message off the stream."""
        if self._closed.is_set():
            raise ConnectionClosedError("client is closed")

        try:
            header_bytes = await self._reader.readexactly(HEADER_SIZE)
        except asyncio.IncompleteReadError as e:
            await self._mark_broken()
            raise ConnectionClosedError(
                f"peer closed after {len(e.partial)} of {HEADER_SIZE} "
                f"header bytes"
            ) from e
        except (ConnectionError, OSError) as e:
            await self._mark_broken()
            raise ConnectionClosedError(f"recv failed: {e}") from e

        try:
            header = unpack_header(header_bytes)
        except ValueError as e:
            await self._mark_broken()
            raise ProtocolError(str(e)) from e

        if (self._options.max_message_size
                and header.body_size > self._options.max_message_size):
            await self._mark_broken()
            raise FramingError(
                f"body_size {header.body_size} exceeds "
                f"max_message_size {self._options.max_message_size}"
            )

        try:
            body = await self._reader.readexactly(header.body_size)
        except asyncio.IncompleteReadError as e:
            await self._mark_broken()
            raise ConnectionClosedError(
                f"peer closed mid-body: got {len(e.partial)} of "
                f"{header.body_size}"
            ) from e

        computed = crc64(body)
        if computed != header.crc:
            # CRC mismatch doesn't always mean the stream is toast,
            # but it does mean *this* message is garbage. Raise; the
            # caller decides whether to keep reading.
            raise CrcMismatchError(
                f"header crc=0x{header.crc:016x} body crc=0x{computed:016x}"
            )

        # Decode to a typed instance if we know the type_id; otherwise
        # return the raw body dict-ish view (the header is always
        # typed either way).
        cls = _MESSAGE_REGISTRY.get(header.type_id)
        decoded: BaseModel | dict[str, bytes]
        if cls is not None:
            try:
                decoded = cls.unpack(body)
            except (ValueError, ProtocolError) as e:
                raise ProtocolError(
                    f"failed to decode {header.type_id}: {e}"
                ) from e
        else:
            # Unknown type_id — hand the raw bytes through so the
            # on_unknown handler can log or forward.
            decoded = _RawBody(type_id=header.type_id, body=body)

        return Envelope(header=header, body=decoded)

    # --------------------------------------------------------------
    # Dispatch (decorator + run loop)
    # --------------------------------------------------------------

    def on(
        self,
        message_type: type[M],
    ) -> Callable[[Handler], Handler]:
        """Register *handler* for messages of *message_type*.

        Usable as a decorator::

            @c.on(Transform)
            async def _(env):
                renderer.update_pose(env.body.matrix)

        Returning the handler so ``@c.on(T)`` composes with other
        decorators. Only one handler per type_id — re-registering
        replaces the previous entry.
        """
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
        """Register a fallback for messages with no typed handler.

        Receives an :class:`Envelope` whose ``body`` is a
        :class:`_RawBody` with ``type_id`` and ``body`` bytes.
        """
        self._unknown_handler = handler
        return handler

    def on_error(
        self,
        handler: Callable[[BaseException], Awaitable[None]],
    ) -> Callable[[BaseException], Awaitable[None]]:
        """Register an error callback for :meth:`run`.

        Fires on any exception raised during a receive or handler
        invocation. If unset, exceptions propagate out of ``run()``.
        """
        self._error_handler = handler
        return handler

    async def run(self) -> None:
        """Dispatch loop — read messages and route to handlers.

        Returns when :meth:`close` is called or when the peer
        closes the stream. Exceptions during receive or within a
        handler go to :meth:`on_error` if registered, else they
        propagate.
        """
        while not self._run_stop.is_set():
            try:
                env = await self._receive_one()
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
        """Internal: route one envelope to the registered handler."""
        handler = self._handlers.get(env.header.type_id)
        if handler is not None:
            await handler(env)
        elif self._unknown_handler is not None:
            await self._unknown_handler(env)
        # Else silently drop — caller opted out of this type_id.


class _RawBody(BaseModel):
    """Sentinel body type for wire messages whose type_id is unknown.

    Exposed through :meth:`Client.on_unknown` so loggers / forwarders
    can still see the bytes without the dispatcher having to decode.
    """

    type_id: str
    body: bytes
