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
from oigtl.net import interfaces
from oigtl.net._options import Envelope, RawMessage
from oigtl.net.errors import ConnectionClosedError, FramingError
from oigtl.net.policy import IpRange, PeerPolicy
from oigtl.runtime.exceptions import CrcMismatchError, ProtocolError
from oigtl.runtime.header import HEADER_SIZE, pack_header, unpack_header
from oigtl_corpus_tools.codec.crc64 import crc64

__all__ = ["Peer", "Server", "ServerOptions"]


# Sentinel enqueued to ``_accept_queue`` when the server is closing,
# so an ``accepted_peers()`` iterator knows to exit cleanly.
_ACCEPT_SENTINEL: object = object()

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
        await self._write_wire(header + body)

    async def send_raw(self, msg: RawMessage) -> None:
        """Send already-framed wire bytes.

        Escape hatch for gateways: moves a :class:`RawMessage` from
        another endpoint through this peer without a decode /
        re-encode round trip. Caller is responsible for valid OIGTL
        framing — typically the ``RawMessage`` came from another
        endpoint's :meth:`raw_messages`.
        """
        if self._closed:
            raise ConnectionClosedError("peer is closed")
        await self._write_wire(msg.wire)

    async def _write_wire(self, wire: bytes) -> None:
        async with self._send_lock:
            try:
                self._writer.write(wire)
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

    async def raw_messages(
        self, *, max_body_size: int = 0,
    ) -> AsyncIterator[RawMessage]:
        """Async iterator yielding raw (header + wire bytes) messages.

        Gateway-friendly: reads framed bytes off this peer's stream
        without decoding the body. Yields :class:`RawMessage`
        instances whose ``wire`` bytes can be forwarded via
        :meth:`send_raw` on any other endpoint.

        ``max_body_size`` of 0 means no cap; non-zero rejects any
        message announcing a larger ``body_size``, raising
        :class:`FramingError` before the body bytes are allocated.

        Stops on peer FIN or :meth:`close`.
        """
        while not self._closed:
            try:
                header_bytes = await self._reader.readexactly(HEADER_SIZE)
            except (asyncio.IncompleteReadError,
                    ConnectionError, OSError):
                return

            try:
                header = unpack_header(header_bytes)
            except ValueError as e:
                raise ProtocolError(str(e)) from e

            if max_body_size > 0 and header.body_size > max_body_size:
                raise FramingError(
                    f"body_size {header.body_size} exceeds "
                    f"max_body_size {max_body_size}"
                )

            body_size = int(header.body_size)
            try:
                body = await self._reader.readexactly(body_size)
            except asyncio.IncompleteReadError:
                return

            computed = crc64(body)
            if computed != header.crc:
                raise CrcMismatchError(
                    f"header crc=0x{header.crc:016x} "
                    f"body crc=0x{computed:016x}"
                )

            yield RawMessage(
                header=header,
                wire=header_bytes + body,
            )


# ----------------------------------------------------------------------
# ServerOptions — configuration knobs.
# ----------------------------------------------------------------------


@dataclass
class ServerOptions:
    """Knobs for :class:`Server`.

    The restriction fields (``policy``, ``max_clients``,
    ``idle_timeout_seconds``) can also be set fluently via
    builder methods on :class:`Server` — the builders are the
    researcher-facing surface; this struct exists so the same
    config is serialisable.
    """

    default_device: str = "python-server"
    max_message_size: int = 0
    """If non-zero, reject inbound messages with body_size above this
    cap. Pre-parse DoS defence."""

    policy: PeerPolicy | None = None
    """Accept-time peer-address filter. ``None`` = admit any peer."""

    max_clients: int = 0
    """Maximum simultaneous connections. 0 = unlimited. Connections
    arriving over the cap are rejected immediately."""

    idle_timeout_seconds: float = 0.0
    """A peer with no received bytes for this many seconds is
    disconnected. 0 = no timeout."""


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

        # If `accepted_peers()` is in use, newly-admitted peers go
        # onto this queue instead of the typed-dispatch loop. Use
        # one mode or the other — not both simultaneously.
        self._accept_queue: asyncio.Queue[Peer | object] | None = None

    @classmethod
    def listen_sync(
        cls,
        port: int,
        options: ServerOptions | None = None,
        *,
        host: str = "0.0.0.0",
    ) -> "oigtl.net.sync_server.SyncServer":   # noqa: F821
        """Synchronous counterpart to :meth:`listen`.

        Returns a :class:`~oigtl.net.sync_server.SyncServer` — same
        capability, blocking surface, no ``await`` required.
        """
        from oigtl.net.sync_server import SyncServer
        return SyncServer.listen(port, options, host=host)

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
        inst._accept_queue = None
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

    def accepted_peers(self) -> AsyncIterator[Peer]:
        """Async iterator yielding each accepted peer exactly once.

        The dispatch-loop / handler-decorator API
        (:meth:`on` + :meth:`serve`) is idiomatic for servers that
        handle typed messages. For the gateway pattern — where the
        server is one end of a byte pipe — an iterator of peers is
        more natural::

            async for peer in server.accepted_peers():
                asyncio.create_task(handle_one(peer))

        Under the hood this wraps the same accept machinery
        :meth:`serve` uses; exactly one of the two should be driven
        on a given server instance.
        """
        return self._accepted_iter()

    async def _accepted_iter(self) -> AsyncIterator[Peer]:
        """Install a queue-based peer sink and yield from it."""
        if self._accept_queue is not None:
            raise RuntimeError(
                "accepted_peers() is already being iterated elsewhere"
            )
        self._accept_queue = asyncio.Queue()
        try:
            while not self._closed.is_set():
                peer = await self._accept_queue.get()
                if peer is _ACCEPT_SENTINEL:
                    return
                yield peer
        finally:
            self._accept_queue = None

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
    # Restrictions (fluent builders)
    # --------------------------------------------------------------

    def allow(
        self,
        peers: (
            IpRange
            | str
            | ipaddress.IPv4Network
            | ipaddress.IPv6Network
            | ipaddress.IPv4Address
            | ipaddress.IPv6Address
            | list
            | tuple
        ),
    ) -> "Server":
        """Narrow the set of peers allowed to connect.

        Accepts a single item or a list. Items can be:

        - An :class:`IpRange` (parsed via :func:`oigtl.net.parse`).
        - A stdlib :class:`~ipaddress.IPv4Network` /
          :class:`~ipaddress.IPv6Network` (covers ``interfaces.subnets()``).
        - A stdlib :class:`~ipaddress.IPv4Address` /
          :class:`~ipaddress.IPv6Address` (single-host range).
        - A string — passes through :func:`oigtl.net.parse`, which
          accepts single IPs, CIDR, and dash-ranges.

        Multiple calls compose (additive union). Returns ``self`` so
        builders chain::

            server = (await Server.listen(18944)) \\
                .allow(interfaces.subnets()) \\
                .allow("10.42.0.0/24")
        """
        if self._options.policy is None:
            self._options.policy = PeerPolicy()

        for item in _flatten(peers):
            rng = _coerce_to_iprange(item)
            if rng is None:
                raise ValueError(
                    f"cannot interpret {item!r} as an IP range"
                )
            self._options.policy.allowed_peers.append(rng)
        return self

    def restrict_to_local_subnet(self) -> "Server":
        """Accept only peers on the same LAN(s) as this host.

        One-line equivalent of ``server.allow(interfaces.subnets())``
        with link-local filtering. Common in research labs where
        the tracker should only accept connections from the bench
        machine or other lab peers.
        """
        return self.allow(interfaces.subnets())

    def restrict_to_this_machine_only(self) -> "Server":
        """Accept only connections from this host's own addresses.

        Includes loopback (``127.0.0.1``, ``::1``). Equivalent to
        firewalling everything except localhost — useful for tests
        and for IPC-style use of the transport on a shared box.
        """
        return self.allow(interfaces.subnets(include_loopback=True))

    def set_max_clients(self, n: int) -> "Server":
        """Cap simultaneous connections. 0 = unlimited (default)."""
        if n < 0:
            raise ValueError("max_clients must be >= 0")
        self._options.max_clients = n
        return self

    def disconnect_if_silent_for(
        self, timeout: timedelta | float | int,
    ) -> "Server":
        """Close a peer that hasn't sent any bytes in *timeout*.

        Accepts :class:`~datetime.timedelta`, seconds as float, or
        milliseconds as int — mirrors the ``ClientOptions`` convention.
        ``0`` disables (default).
        """
        if isinstance(timeout, timedelta):
            secs = timeout.total_seconds()
        elif isinstance(timeout, int) and not isinstance(timeout, bool):
            # Int = milliseconds (matches ClientOptions convention).
            secs = timeout / 1000.0
        elif isinstance(timeout, float):
            secs = float(timeout)
        else:
            raise TypeError(
                f"disconnect_if_silent_for expects timedelta/int ms/"
                f"float seconds, got {type(timeout).__name__}"
            )
        if secs < 0:
            raise ValueError("timeout must be >= 0")
        self._options.idle_timeout_seconds = secs
        return self

    def set_max_message_size_bytes(self, n: int) -> "Server":
        """Reject inbound messages with body_size greater than *n*."""
        if n < 0:
            raise ValueError("max_message_size must be >= 0")
        self._options.max_message_size = n
        return self

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

        # Two accept modes:
        #  - dispatch mode: start a per-peer task driving the typed
        #    handler loop (the @server.on(T) API).
        #  - iterator mode: push the peer onto accept_queue so
        #    `async for peer in server.accepted_peers()` picks it up;
        #    the caller drives the per-peer I/O themselves.
        if self._accept_queue is not None:
            await self._accept_queue.put(peer)
        else:
            task = asyncio.create_task(
                self._peer_loop(peer),
                name=f"oigtl-peer-{peer.address}",
            )
            self._peer_tasks.add(task)
            task.add_done_callback(self._peer_tasks.discard)

    async def _admit(self, peer: Peer) -> bool:
        """Consult the configured restrictions; return True to admit."""
        # max_clients — reject when over cap.
        if (self._options.max_clients
                and len(self._peers) >= self._options.max_clients):
            return False

        # Peer-address filter.
        if self._options.policy is not None:
            if not self._options.policy.is_peer_allowed(
                peer.address.address,
            ):
                return False

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
        """Read one framed message from *peer*'s stream.

        Honours :attr:`ServerOptions.idle_timeout_seconds` — a peer
        that's silent past the budget is disconnected by raising
        :class:`ConnectionClosedError`, which the peer loop handles
        as a normal close.
        """
        idle = self._options.idle_timeout_seconds
        read_coro = peer._reader.readexactly(HEADER_SIZE)
        try:
            if idle > 0:
                header_bytes = await asyncio.wait_for(
                    read_coro, timeout=idle,
                )
            else:
                header_bytes = await read_coro
        except asyncio.TimeoutError as e:
            raise ConnectionClosedError(
                f"peer idle > {idle}s; disconnecting"
            ) from e
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

        # Wake any accepted_peers() iterator so it returns cleanly.
        if self._accept_queue is not None:
            await self._accept_queue.put(_ACCEPT_SENTINEL)

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


# ----------------------------------------------------------------------
# Restriction helpers.
# ----------------------------------------------------------------------


def _flatten(item) -> list:
    """Accept a scalar or iterable (list/tuple) and yield individual items."""
    if isinstance(item, (list, tuple)):
        out: list = []
        for sub in item:
            out.extend(_flatten(sub))
        return out
    return [item]


def _coerce_to_iprange(item) -> IpRange | None:
    """Convert one allow()-accepted value to an :class:`IpRange`.

    Returns None if the value isn't recognisable. The public
    :meth:`Server.allow` turns that into a ``ValueError``.
    """
    # Already an IpRange.
    if isinstance(item, IpRange):
        return item
    # Network — use first/last of the network block.
    if isinstance(item, (ipaddress.IPv4Network, ipaddress.IPv6Network)):
        return IpRange(
            first=item.network_address,
            last=item.broadcast_address,
        )
    # Single address — range of one.
    if isinstance(item, (ipaddress.IPv4Address, ipaddress.IPv6Address)):
        return IpRange(first=item, last=item)
    # String — delegate to the parse helper.
    if isinstance(item, str):
        from oigtl.net.policy import parse
        return parse(item)
    return None
