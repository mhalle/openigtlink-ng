"""WebSocket client — Python counterpart to a browser or Node WS peer.

Parallel to :class:`~oigtl.net.Client` in shape (connect, send,
receive, dispatch, ``raw_messages``, ``send_raw``, ``run``) but
uses WebSocket framing: one binary frame = one IGTL message.

No WSS / TLS yet.

    async with await WsClient.connect("ws://tracker.lab:18945/") as c:
        await c.send(Transform(matrix=[...]))
        env = await c.receive(Status)
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

import websockets
from pydantic import BaseModel
from websockets.asyncio.client import ClientConnection, connect as ws_connect

from oigtl.codec import RawBody, unpack_message
from oigtl.net._options import (
    ClientOptions,
    Envelope,
    RawMessage,
    as_timedelta,
)
from oigtl.net.errors import (
    ConnectionClosedError,
    FramingError,
    TimeoutError as NetTimeoutError,
)
from oigtl.runtime.exceptions import CrcMismatchError, ProtocolError
from oigtl.runtime.header import HEADER_SIZE, pack_header, unpack_header
from oigtl_corpus_tools.codec.crc64 import crc64

__all__ = ["WsClient"]

M = TypeVar("M", bound=BaseModel)
Handler = Callable[[Envelope[Any]], Awaitable[None]]


class WsClient:
    """Async OpenIGTLink client over WebSocket.

    Same researcher-facing surface as :class:`~oigtl.net.Client` —
    typed send/receive, dispatch decorator, async iterator,
    ``raw_messages`` escape hatch. No resilience features in this
    initial pass; layer on if needed.

    Construct via :meth:`connect` (classmethod, awaits the handshake).
    """

    # --------------------------------------------------------------
    # Construction / teardown
    # --------------------------------------------------------------

    def __init__(
        self,
        *,
        ws: ClientConnection,
        url: str,
        options: ClientOptions,
    ) -> None:
        self._ws = ws
        self._url = url
        self._options = options
        self._send_lock = asyncio.Lock()
        self._closed = asyncio.Event()
        self._run_stop = asyncio.Event()

        self._handlers: dict[str, Handler] = {}
        self._unknown_handler: Handler | None = None
        self._error_handler: Callable[[BaseException], Awaitable[None]] | None = None

    @classmethod
    async def connect(
        cls,
        url: str,
        options: ClientOptions | None = None,
    ) -> "WsClient":
        """Open a WS connection and return a ready :class:`WsClient`.

        Accepts a full URL (``ws://host:port/`` or ``ws://host:port/path``).
        Honours ``options.connect_timeout``. Raises
        :class:`~oigtl.net.errors.TimeoutError` on budget exhaustion,
        :class:`~oigtl.net.errors.ConnectionClosedError` on other
        failures. ``wss://`` not supported yet.
        """
        opt = options or ClientOptions()
        if not (url.startswith("ws://") or url.startswith("wss://")):
            raise ValueError(
                f"ws URL must start with ws:// or wss://; got {url!r}"
            )
        if url.startswith("wss://"):
            raise NotImplementedError(
                "wss:// (TLS) not supported yet; use ws:// for now"
            )

        try:
            coro = ws_connect(
                url,
                max_size=(
                    opt.max_message_size + HEADER_SIZE
                    if opt.max_message_size > 0
                    else None
                ),
            )
            if opt.connect_timeout is None:
                ws = await coro
            else:
                ws = await asyncio.wait_for(
                    coro,
                    timeout=opt.connect_timeout.total_seconds(),
                )
        except asyncio.TimeoutError as e:
            raise NetTimeoutError(
                f"ws connect to {url} timed out after "
                f"{opt.connect_timeout}"
            ) from e
        except (OSError, websockets.InvalidHandshake,
                websockets.InvalidURI) as e:
            raise ConnectionClosedError(
                f"ws connect to {url} failed: {e}"
            ) from e

        return cls(ws=ws, url=url, options=opt)

    # --------------------------------------------------------------
    # Introspection
    # --------------------------------------------------------------

    @property
    def options(self) -> ClientOptions:
        return self._options

    @property
    def url(self) -> str:
        """The URL originally passed to :meth:`connect`."""
        return self._url

    @property
    def is_connected(self) -> bool:
        return not self._closed.is_set()

    # --------------------------------------------------------------
    # Close
    # --------------------------------------------------------------

    async def close(self) -> None:
        """Close the WebSocket. Idempotent."""
        if self._closed.is_set():
            return
        self._closed.set()
        self._run_stop.set()
        with contextlib.suppress(Exception):
            await self._ws.close()

    async def __aenter__(self) -> "WsClient":
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
        """Frame and transmit *message* as one WebSocket binary frame."""
        if self._closed.is_set():
            raise ConnectionClosedError("client is closed")

        type_id = getattr(type(message), "TYPE_ID", None)
        if not isinstance(type_id, str):
            raise TypeError(
                f"{type(message).__name__} has no TYPE_ID"
            )
        body = message.pack()
        # v1 framing — see :meth:`oigtl.net.client.Client.send`.
        header = pack_header(
            version=1,
            type_id=type_id,
            device_name=device_name or self._options.default_device,
            timestamp=timestamp,
            body=body,
        )
        await self._send_wire(header + body)

    async def send_raw(self, msg: RawMessage) -> None:
        """Send already-framed wire bytes as one binary frame."""
        if self._closed.is_set():
            raise ConnectionClosedError("client is closed")
        await self._send_wire(msg.wire)

    async def _send_wire(self, wire: bytes) -> None:
        async with self._send_lock:
            try:
                await self._ws.send(wire)
            except websockets.ConnectionClosed as e:
                self._closed.set()
                raise ConnectionClosedError(f"ws send failed: {e}") from e

    # --------------------------------------------------------------
    # Receive
    # --------------------------------------------------------------

    async def receive_any(
        self,
        *,
        timeout: timedelta | float | int | None = None,
    ) -> Envelope[BaseModel]:
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
        expected = getattr(message_type, "TYPE_ID", None)
        if not isinstance(expected, str):
            raise TypeError(
                f"{message_type.__name__} has no TYPE_ID"
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
                        self._receive_one(), timeout=remaining,
                    )
                except asyncio.TimeoutError as e:
                    raise NetTimeoutError(
                        f"receive({message_type.__name__}) timed out "
                        f"after {budget}"
                    ) from e
            else:
                env = await self._receive_one()

            if env.header.type_id == expected:
                return env  # type: ignore[return-value]
            await self._dispatch(env)

    async def messages(self) -> AsyncIterator[Envelope[BaseModel]]:
        """Async iterator over every received message, in order."""
        try:
            while not self._closed.is_set():
                yield await self._receive_one()
        except ConnectionClosedError:
            return

    async def raw_messages(self) -> AsyncIterator[RawMessage]:
        """Async iterator yielding each frame as a :class:`RawMessage`."""
        try:
            while not self._closed.is_set():
                yield await self._receive_one_raw()
        except ConnectionClosedError:
            return

    async def _receive_one(self) -> Envelope[BaseModel]:
        raw = await self._receive_one_raw()
        # The framer that produced *raw* already verified the CRC,
        # so skip the second check here.
        body = raw.wire[HEADER_SIZE:]
        return unpack_message(
            raw.header, body, loose=True, verify_crc=False,
        )

    async def _receive_one_raw(self) -> RawMessage:
        if self._closed.is_set():
            raise ConnectionClosedError("client is closed")

        try:
            frame = await self._ws.recv()
        except websockets.ConnectionClosedOK:
            self._closed.set()
            raise ConnectionClosedError("peer closed connection")
        except websockets.ConnectionClosed as e:
            self._closed.set()
            raise ConnectionClosedError(f"ws recv closed: {e}") from e

        if isinstance(frame, str):
            raise FramingError(
                "received text WebSocket frame; OIGTL requires binary"
            )

        data: bytes = bytes(frame)
        if len(data) < HEADER_SIZE:
            raise FramingError(
                f"WS binary frame {len(data)} bytes < header {HEADER_SIZE}"
            )

        try:
            header = unpack_header(data[:HEADER_SIZE])
        except ValueError as e:
            raise ProtocolError(str(e)) from e

        body_size = int(header.body_size)
        expected_size = HEADER_SIZE + body_size
        if len(data) != expected_size:
            raise FramingError(
                f"WS frame size {len(data)} does not match declared "
                f"body_size={body_size} (expected {expected_size})"
            )

        body = data[HEADER_SIZE:]
        computed = crc64(body)
        if computed != header.crc:
            raise CrcMismatchError(
                f"header crc=0x{header.crc:016x} "
                f"body crc=0x{computed:016x}"
            )

        return RawMessage(header=header, wire=data)

    # --------------------------------------------------------------
    # Dispatch
    # --------------------------------------------------------------

    def on(
        self,
        message_type: type[M],
    ) -> Callable[[Handler], Handler]:
        type_id = getattr(message_type, "TYPE_ID", None)
        if not isinstance(type_id, str):
            raise TypeError(
                f"{message_type.__name__} has no TYPE_ID"
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

    async def run(self) -> None:
        """Dispatch loop — read and route until the peer closes."""
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
        handler = self._handlers.get(env.header.type_id)
        if handler is not None:
            await handler(env)
        elif self._unknown_handler is not None:
            await self._unknown_handler(env)


# Backwards-compatible alias — see the matching note in
# :mod:`oigtl.net.client`. New code should use :class:`oigtl.codec.RawBody`.
_RawBody = RawBody
