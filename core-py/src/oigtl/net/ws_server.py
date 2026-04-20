"""WebSocket transport for :class:`~oigtl.net.Server`.

Adds :meth:`Server.listen_ws` as an alternative to :meth:`Server.listen`,
accepting WebSocket peers instead of raw TCP. Dispatch, restrictions,
lifecycle callbacks, and the ``accepted_peers()`` iterator all work
unchanged — the transport lives below the :class:`Peer` abstraction.

**Framing rule:** one WebSocket binary frame = one IGTL message.
The WS boundary already delimits messages for us, so we don't run
:class:`V3Framer` on this path. A peer that tries to send a text
frame or a binary frame that doesn't match ``<58-byte header> +
<header.body_size> body`` is disconnected.

No WSS (TLS) yet — that layers on with an ``ssl=`` kwarg when we
need it. ``ws://`` only for now.
"""

from __future__ import annotations

import asyncio
import ipaddress
from typing import Any, AsyncIterator

import websockets
from websockets.asyncio.server import Server as WebsocketsServer, ServerConnection

from oigtl.net._options import RawMessage
from oigtl.net.errors import ConnectionClosedError, FramingError
from oigtl.net.server import (
    Peer,
    PeerAddress,
    Server,
    ServerOptions,
)
from oigtl.runtime.exceptions import CrcMismatchError, ProtocolError
from oigtl.runtime.header import HEADER_SIZE, unpack_header
from oigtl_corpus_tools.codec.crc64 import crc64

__all__ = ["WsPeer"]


# ----------------------------------------------------------------------
# WsPeer — the WebSocket-backed Peer.
# ----------------------------------------------------------------------


class WsPeer(Peer):
    """A :class:`Peer` whose underlying transport is a WebSocket.

    Each inbound binary frame is one IGTL message, delivered whole.
    Outbound messages go out as single binary frames. Handshake
    and framing are already handled by the ``websockets`` library;
    this class only has to translate between WS frames and
    :class:`RawMessage` instances.
    """

    def __init__(
        self,
        *,
        ws: ServerConnection,
        address: PeerAddress,
        default_device: str,
    ) -> None:
        super().__init__(address=address, default_device=default_device)
        self._ws = ws

    async def _write_wire(self, wire: bytes) -> None:
        async with self._send_lock:
            try:
                await self._ws.send(wire)
            except websockets.ConnectionClosed as e:
                self._closed = True
                raise ConnectionClosedError(f"ws send failed: {e}") from e
            except Exception as e:
                self._closed = True
                raise ConnectionClosedError(f"ws send failed: {e}") from e

    async def _close_transport(self) -> None:
        try:
            await self._ws.close()
        except Exception:
            pass

    async def _read_one_raw(
        self, *, max_body_size: int = 0,
    ) -> RawMessage | None:
        try:
            frame = await self._ws.recv()
        except websockets.ConnectionClosedOK:
            return None
        except websockets.ConnectionClosed as e:
            raise ConnectionClosedError(f"ws recv closed: {e}") from e
        except Exception as e:
            raise ConnectionClosedError(f"ws recv failed: {e}") from e

        if isinstance(frame, str):
            # Protocol contract: binary-only. Text frames are a
            # misbehaving peer — close them.
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
        expected = HEADER_SIZE + body_size
        if len(data) != expected:
            raise FramingError(
                f"WS frame size {len(data)} does not match declared "
                f"body_size={body_size} (expected {expected})"
            )

        if max_body_size > 0 and body_size > max_body_size:
            # Unlike TCP, we've already buffered the bytes by the
            # time we get here — the defence is that websockets'
            # own ``max_size`` should be set to match or lower.
            raise FramingError(
                f"body_size {body_size} exceeds "
                f"max_body_size {max_body_size}"
            )

        body = data[HEADER_SIZE:]
        computed = crc64(body)
        if computed != header.crc:
            raise CrcMismatchError(
                f"header crc=0x{header.crc:016x} "
                f"body crc=0x{computed:016x}"
            )

        return RawMessage(header=header, wire=data)


# ----------------------------------------------------------------------
# Server.listen_ws — attached via monkey-patch below.
# ----------------------------------------------------------------------


async def _listen_ws(
    cls: type[Server],
    port: int,
    options: ServerOptions | None = None,
    *,
    host: str = "0.0.0.0",
    max_size: int | None = None,
) -> Server:
    """Bind a WebSocket listener on *port* and return a ready :class:`Server`.

    The returned server has the full API surface of a TCP server —
    ``on(T)``, ``allow(...)``, ``accepted_peers()``, ``serve()`` —
    and dispatches identically. Only the transport differs.

    Args:
        port: TCP port to listen on. Pass 0 to let the OS choose.
        options: Standard :class:`ServerOptions`. ``max_message_size``
            also caps inbound WebSocket frame size.
        host: Bind address. Defaults to all interfaces.
        max_size: Override the underlying ``websockets`` library's
            per-frame size cap. Defaults to
            ``options.max_message_size + HEADER_SIZE`` if set,
            else the library default (1 MiB).
    """
    opt = options or ServerOptions()

    # Compute the effective max_size so the websockets library
    # rejects oversize frames BEFORE they hit our buffer. This is
    # the WS equivalent of the TCP pre-body-read cap.
    if max_size is None and opt.max_message_size > 0:
        max_size = opt.max_message_size + HEADER_SIZE

    # Two-step: pre-allocate the Server instance, then hand
    # websockets.serve a handler that routes through it.
    inst = cls.__new__(cls)
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

    async def on_ws(ws: ServerConnection) -> None:
        await _handle_ws_peer(inst, ws)

    ws_server = await websockets.asyncio.server.serve(
        on_ws,
        host=host,
        port=port,
        max_size=max_size,
    )
    # Adapt the websockets server to the shape Server.close()
    # expects: a ``close()`` / ``wait_closed()`` pair. The
    # ``_WsServerAdapter`` below wraps the asyncio-based accept loop
    # in the same lifecycle API ``asyncio.base_events.Server``
    # provides.
    inst._server = _WsServerAdapter(ws_server)    # type: ignore[assignment]
    return inst


async def _handle_ws_peer(
    server: Server,
    ws: ServerConnection,
) -> None:
    """Translate a WebSocket accept into the Server's peer pipeline."""
    # Peer address — websockets gives us an (ip, port) tuple for
    # IPv4 or an (ip, port, flow, scope) tuple for IPv6.
    remote = ws.remote_address
    if remote is None:
        await ws.close()
        return
    raw_ip = remote[0].split("%", 1)[0]
    try:
        peer_addr = PeerAddress(
            address=ipaddress.ip_address(raw_ip),
            port=remote[1],
        )
    except ValueError:
        await ws.close()
        return

    peer = WsPeer(
        ws=ws,
        address=peer_addr,
        default_device=server._options.default_device,
    )

    # Admission: run the same _admit hook TCP uses (allow-list,
    # max_clients, etc.). Rejected peers are closed without ever
    # reaching on_connected or any handler.
    if not await server._admit(peer):
        await peer.close()
        return

    server._peers.add(peer)
    if server._on_connected is not None:
        result = server._on_connected(peer)
        if asyncio.iscoroutine(result):
            await result

    if server._accept_queue is not None:
        # Iterator mode — caller drives per-peer I/O.
        await server._accept_queue.put(peer)
        # websockets.serve requires the handler to stay alive for
        # the duration of the connection; otherwise the socket is
        # closed immediately. Wait for the peer to be closed from
        # outside before returning.
        while peer.is_connected:
            await asyncio.sleep(0.05)
    else:
        # Dispatch mode — run the per-peer loop synchronously in
        # this handler. The websockets library expects the handler
        # to block for the lifetime of the connection.
        await server._peer_loop(peer)


class _WsServerAdapter:
    """Wrap ``websockets.asyncio.server.Server`` in the methods
    :meth:`Server.close` uses on the TCP server object.

    Two methods needed: ``close()`` (synchronous stop-accepting)
    and ``wait_closed()`` (async drain). The websockets library
    already provides both under its own names.
    """

    def __init__(self, ws_server: WebsocketsServer) -> None:
        self._ws = ws_server
        self._sockets = ws_server.sockets

    @property
    def sockets(self) -> Any:
        return self._sockets

    def close(self) -> None:
        self._ws.close(close_connections=True)

    async def wait_closed(self) -> None:
        await self._ws.wait_closed()


# Attach the classmethod. We do it here rather than on the Server
# class to keep ws_server.py strictly additive — importing this
# module enables the feature; not importing it keeps the Server
# surface WS-free (useful for environments without the websockets
# library, though it's a base dep).
Server.listen_ws = classmethod(_listen_ws)  # type: ignore[attr-defined]
