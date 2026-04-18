"""Async OpenIGTLink server.

Mirrors the *capability* of ``oigtl::Server`` in
``core-cpp/include/oigtl/server.hpp``. Researcher-friendly API:

    from oigtl.net import Server, interfaces
    from oigtl.messages import Transform

    server = await Server.listen(18944)
    server.allow(interfaces.subnets())          # LAN-only (Phase 6)

    @server.on(Transform)
    async def _(env, peer):
        print(f"got pose from {peer.address}: {env.body.matrix[-3:]}")

    await server.serve()                        # blocks

Phase 5 ships the happy-path accept + per-peer dispatch loop.
Phase 6 adds the restriction builders on top of it.
"""

from __future__ import annotations

import asyncio
import contextlib
import ipaddress
from dataclasses import dataclass
from typing import (
    Any,
    AsyncIterator,
    Awaitable,
    Callable,
    TypeVar,
)

from pydantic import BaseModel

from oigtl.messages import REGISTRY as _MESSAGE_REGISTRY
from oigtl.net._options import Envelope
from oigtl.net.errors import ConnectionClosedError, FramingError
from oigtl.runtime.exceptions import CrcMismatchError, ProtocolError
from oigtl.runtime.header import HEADER_SIZE, pack_header, unpack_header
from oigtl_corpus_tools.codec.crc64 import crc64

__all__ = ["Peer", "Server", "ServerOptions"]

M = TypeVar("M", bound=BaseModel)
PeerHandler = Callable[[Envelope[Any], "Peer"], Awaitable[None]]


# ----------------------------------------------------------------------
# Peer — the server's view of one accepted connection.
# ----------------------------------------------------------------------


@dataclass(frozen=True)
class PeerAddress:
    """Immutable view of an accepted peer's address + port."""

    address: ipaddress.IPv4Address | ipaddress.IPv6Address
    port: int

    def __str__(self) -> str:
        if self.address.version == 6:
            return f"[{self.address}]:{self.port}"
        return f"{self.address}:{self.port}"


class Peer:
    """A single accepted client connection.

    Passed into every handler so researchers can reply (``peer.send``),
    check who's talking (``peer.address``), or close a specific
    connection without affecting others.

    Methods here are a narrow subset of :class:`Client` — a server
    peer doesn't need to dial, reconnect, or buffer. The Server
    itself owns the lifecycle.
    """

    def __init__(
        self,
        *,
        reader: asyncio.StreamReader,
        writer: asyncio.StreamWriter,
        address: PeerAddress,
        default_device: str,
    ) -> None:
        self._reader = reader
        self._writer = writer
        self._address = address
        self._default_device = default_device
        self._send_lock = asyncio.Lock()
        self._closed = False

    @property
    def address(self) -> PeerAddress:
        return self._address

    @property
    def is_connected(self) -> bool:
        return not self._closed

    async def send(
        self,
        message: BaseModel,
        *,
        device_name: str | None = None,
        timestamp: int = 0,
    ) -> None:
        """Frame and transmit *message* to this peer."""
        if self._closed:
            raise ConnectionClosedError("peer is closed")

        type_id = getattr(type(message), "TYPE_ID", None)
        if not isinstance(type_id, str):
            raise TypeError(
                f"{type(message).__name__} has no TYPE_ID"
            )

        body = message.pack()
        header = pack_header(
            version=2,
            type_id=type_id,
            device_name=device_name or self._default_device,
            timestamp=timestamp,
            body=body,
        )
        async with self._send_lock:
            try:
                self._writer.write(header + body)
                await self._writer.drain()
            except (ConnectionError, BrokenPipeError, OSError) as e:
                self._closed = True
                raise ConnectionClosedError(f"send failed: {e}") from e

    async def close(self) -> None:
        """Close this peer's connection. Idempotent."""
        if self._closed:
            return
        self._closed = True
        try:
            self._writer.close()
            with contextlib.suppress(Exception):
                await self._writer.wait_closed()
        except Exception:
            pass


# ----------------------------------------------------------------------
# ServerOptions — configuration knobs.
# ----------------------------------------------------------------------


@dataclass
class ServerOptions:
    """Knobs for :class:`Server`.

    Phase 5 uses only the basics; Phase 6 adds restriction fields.
    """

    default_device: str = "python-server"
    max_message_size: int = 0
    """If non-zero, reject inbound messages with body_size above this
    cap. Pre-parse DoS defence."""


# ----------------------------------------------------------------------
# Server — accept loop + dispatch.
# ----------------------------------------------------------------------


class Server:
    """Async OpenIGTLink server.

    Construct via :meth:`listen`; accept loop runs under
    :meth:`serve`. Each accepted peer gets its own ``asyncio.Task``
    that reads messages and dispatches through the decorator-style
    handlers.
    """

    # --------------------------------------------------------------
    # Construction
    # --------------------------------------------------------------

    def __init__(
        self,
        *,
        server: asyncio.base_events.Server,
        options: ServerOptions,
    ) -> None:
        self._server = server
        self._options = options
        self._closed = asyncio.Event()

        # type_id → handler. Handlers receive (envelope, peer).
        self._handlers: dict[str, PeerHandler] = {}
        self._unknown_handler: PeerHandler | None = None
        self._on_connected: Callable[[Peer], Awaitable[None] | None] | None = None
        self._on_disconnected: Callable[
            [Peer, BaseException | None], Awaitable[None] | None,
        ] | None = None
        self._error_handler: Callable[[BaseException], Awaitable[None]] | None = None

        # Active peer tasks; tracked so serve() can cancel them on
        # shutdown.
        self._peer_tasks: set[asyncio.Task] = set()
        self._peers: set[Peer] = set()

    @classmethod
    async def listen(
        cls,
        port: int,
        options: ServerOptions | None = None,
        *,
        host: str = "0.0.0.0",
    ) -> "Server":
        """Bind and start listening on *port*.

        Doesn't start accepting yet — call :meth:`serve` to run the
        accept loop. This split matches the C++ API and lets callers
        attach handlers before the first peer arrives.
        """
        opt = options or ServerOptions()
        inst = cls.__new__(cls)
        # Temporarily set empty state so _peer_connected can run
        # during start_server.
        inst._options = opt
        inst._handlers = {}
        inst._unknown_handler = None
        inst._on_connected = None
        inst._on_disconnected = None
        inst._error_handler = None
        inst._peer_tasks = set()
        inst._peers = set()
        inst._closed = asyncio.Event()

        server = await asyncio.start_server(
            inst._peer_connected,
            host=host,
            port=port,
            reuse_address=True,
        )
        inst._server = server
        return inst

    # --------------------------------------------------------------
    # Introspection
    # --------------------------------------------------------------

    @property
    def options(self) -> ServerOptions:
        return self._options

    @property
    def port(self) -> int:
        """The actual listening port (resolved from ``port=0``)."""
        assert self._server.sockets
        return self._server.sockets[0].getsockname()[1]

    @property
    def peers(self) -> frozenset[Peer]:
        """Snapshot of currently-connected peers."""
        return frozenset(self._peers)

    # --------------------------------------------------------------
    # Handler registration
    # --------------------------------------------------------------

    def on(
        self,
        message_type: type[M],
    ) -> Callable[[PeerHandler], PeerHandler]:
        """Register a handler for *message_type* messages on any peer.

        Decorator style::

            @server.on(Transform)
            async def _(env, peer):
                await peer.send(Status(code=1, ...))

        Same handler runs for every peer; differentiate via the
        ``peer`` argument.
        """
        type_id = getattr(message_type, "TYPE_ID", None)
        if not isinstance(type_id, str):
            raise TypeError(
                f"{message_type.__name__} has no TYPE_ID"
            )

        def register(handler: PeerHandler) -> PeerHandler:
            self._handlers[type_id] = handler
            return handler

        return register

    def on_unknown(self, handler: PeerHandler) -> PeerHandler:
        self._unknown_handler = handler
        return handler

    def on_connected(
        self, handler: Callable[[Peer], Awaitable[None] | None],
    ) -> Callable[[Peer], Awaitable[None] | None]:
        """Called for each newly-accepted peer, after restriction checks."""
        self._on_connected = handler
        return handler

    def on_disconnected(
        self,
        handler: Callable[
            [Peer, BaseException | None],
            Awaitable[None] | None,
        ],
    ) -> Callable[[Peer, BaseException | None], Awaitable[None] | None]:
        """Called when a peer's connection ends (normal close or error)."""
        self._on_disconnected = handler
        return handler

    def on_error(
        self,
        handler: Callable[[BaseException], Awaitable[None]],
    ) -> Callable[[BaseException], Awaitable[None]]:
        self._error_handler = handler
        return handler

    # --------------------------------------------------------------
    # Accept loop
    # --------------------------------------------------------------

    async def _peer_connected(
        self,
        reader: asyncio.StreamReader,
        writer: asyncio.StreamWriter,
    ) -> None:
        """Called by asyncio when a new peer connects."""
        peername = writer.get_extra_info("peername")
        if peername is None:
            writer.close()
            return

        try:
            raw_ip = peername[0].split("%", 1)[0]
            peer_addr = PeerAddress(
                address=ipaddress.ip_address(raw_ip),
                port=peername[1],
            )
        except ValueError:
            writer.close()
            return

        peer = Peer(
            reader=reader,
            writer=writer,
            address=peer_addr,
            default_device=self._options.default_device,
        )

        # Subclass hook runs here in Phase 6 — restriction check.
        if not await self._admit(peer):
            await peer.close()
            return

        self._peers.add(peer)
        if self._on_connected is not None:
            result = self._on_connected(peer)
            if asyncio.iscoroutine(result):
                await result

        task = asyncio.create_task(
            self._peer_loop(peer),
            name=f"oigtl-peer-{peer.address}",
        )
        self._peer_tasks.add(task)
        task.add_done_callback(self._peer_tasks.discard)

    async def _admit(self, peer: Peer) -> bool:
        """Restriction hook — Phase 5 admits everyone.

        Overridden in Phase 6 to consult :class:`PeerPolicy` etc.
        """
        return True

    async def _peer_loop(self, peer: Peer) -> None:
        """Read + dispatch loop for a single peer.

        Exits on peer FIN, framing error, or server close. Fires
        on_disconnected on the way out.
        """
        cause: BaseException | None = None
        try:
            while not self._closed.is_set() and peer.is_connected:
                try:
                    env = await self._receive_from(peer)
                except ConnectionClosedError:
                    break
                except BaseException as e:
                    cause = e
                    if self._error_handler is not None:
                        await self._error_handler(e)
                    break

                handler = self._handlers.get(env.header.type_id)
                if handler is not None:
                    try:
                        await handler(env, peer)
                    except BaseException as e:
                        if self._error_handler is not None:
                            await self._error_handler(e)
                        else:
                            raise
                elif self._unknown_handler is not None:
                    await self._unknown_handler(env, peer)
        finally:
            self._peers.discard(peer)
            await peer.close()
            if self._on_disconnected is not None:
                result = self._on_disconnected(peer, cause)
                if asyncio.iscoroutine(result):
                    await result

    async def _receive_from(self, peer: Peer) -> Envelope[BaseModel]:
        """Read one framed message from *peer*'s stream."""
        try:
            header_bytes = await peer._reader.readexactly(HEADER_SIZE)
        except asyncio.IncompleteReadError as e:
            raise ConnectionClosedError(
                f"peer closed after {len(e.partial)}/{HEADER_SIZE} "
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
            body = await peer._reader.readexactly(header.body_size)
        except asyncio.IncompleteReadError as e:
            raise ConnectionClosedError(
                f"peer closed mid-body: got {len(e.partial)} of "
                f"{header.body_size}"
            ) from e

        computed = crc64(body)
        if computed != header.crc:
            raise CrcMismatchError(
                f"header crc=0x{header.crc:016x} "
                f"body crc=0x{computed:016x}"
            )

        cls = _MESSAGE_REGISTRY.get(header.type_id)
        decoded: BaseModel
        if cls is not None:
            try:
                decoded = cls.unpack(body)
            except (ValueError, ProtocolError) as e:
                raise ProtocolError(
                    f"failed to decode {header.type_id}: {e}"
                ) from e
        else:
            decoded = _RawBody(type_id=header.type_id, body=body)

        return Envelope(header=header, body=decoded)

    # --------------------------------------------------------------
    # Serve / shutdown
    # --------------------------------------------------------------

    async def serve(self) -> None:
        """Run the accept loop until :meth:`close` is called.

        Blocks. The underlying :func:`asyncio.start_server` already
        put us in accept mode; this just waits until the close event
        is set.
        """
        await self._closed.wait()

    async def close(self) -> None:
        """Stop accepting and close every active peer."""
        if self._closed.is_set():
            return
        self._closed.set()

        self._server.close()

        # Close active peers first so wait_closed() doesn't block.
        for p in list(self._peers):
            await p.close()
        for t in list(self._peer_tasks):
            t.cancel()
            with contextlib.suppress(
                asyncio.CancelledError, Exception,
            ):
                await t

        with contextlib.suppress(Exception):
            await asyncio.wait_for(
                self._server.wait_closed(), timeout=2,
            )

    async def __aenter__(self) -> "Server":
        return self

    async def __aexit__(self, *_: object) -> None:
        await self.close()


class _RawBody(BaseModel):
    """Sentinel body for wire messages whose type_id is unknown."""

    type_id: str
    body: bytes
