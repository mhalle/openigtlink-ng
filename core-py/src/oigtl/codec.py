"""Pure (transport-independent) OpenIGTLink wire codec.

This module is the public surface for turning OpenIGTLink bytes into
typed :class:`~oigtl.net._options.Envelope` instances and back. It
sits strictly above the header codec (:mod:`oigtl.runtime.header`)
and the message-type registry (:mod:`oigtl.messages.registry`), and
strictly below every transport in :mod:`oigtl.net` (TCP, WebSocket,
future MQTT adapter, file replay, …).

The four-step pattern
---------------------

Decoding one wire message follows the same four steps regardless of
transport:

1. **read_header_bytes**  — I/O. Get exactly ``HEADER_SIZE`` bytes.
2. **unpack_header**      — pure. Parse them into a :class:`Header`.
3. **read_message_bytes** — I/O. Read ``header.body_size`` more bytes.
4. **unpack_message**     — pure. Parse (header, body) into an Envelope.

Steps 2 and 4 are the two pure entry points this module exposes.
Callers that already have a *complete* wire message in memory (MQTT
payload, file slice, unit-test fixture) can skip the interleaving
and call :func:`unpack_envelope` in one shot.

The inverse (encode) has no data dependency, so it's two functions,
not three: :func:`pack_envelope` handles the whole message, and
:func:`oigtl.runtime.header.pack_header` stays available for the
occasional caller that needs only the 58-byte header.

Extensions
----------

Any class that satisfies::

    cls.TYPE_ID: str                               # 12-ASCII-char wire id
    cls.unpack(body: bytes) -> cls                 # classmethod
    instance.pack() -> bytes                       # instance method

can be registered via :func:`oigtl.register_message_type`. From that
moment on, wire messages whose ``header.type_id`` matches the
registered TYPE_ID decode into that class through the identical path
the built-ins use — there is no second code path for extensions.

Unknown types
-------------

Wire messages whose ``type_id`` is not registered still decode
successfully when ``loose=True``: the returned Envelope holds a
:class:`RawBody` carrying the raw body bytes. Strict mode
(``loose=False``, the default for streaming transports) raises
:class:`~oigtl.runtime.exceptions.UnknownMessageTypeError` instead,
which lets production code surface schema drift loudly. The gateway
path never reaches here — it uses the raw-byte framer directly.
"""

from __future__ import annotations

from typing import Any

from pydantic import BaseModel, ConfigDict

from oigtl.messages.registry import lookup_message_class
from oigtl.runtime.envelope import Envelope
from oigtl.runtime.exceptions import (
    CrcMismatchError,
    MalformedMessageError,
    ProtocolError,
    ShortBufferError,
    UnknownMessageTypeError,
)
from oigtl.runtime.header import HEADER_SIZE, Header, pack_header, unpack_header

from oigtl_corpus_tools.codec.crc64 import crc64

__all__ = [
    "HEADER_SIZE",
    "Header",
    "RawBody",
    "pack_envelope",
    "pack_header",
    "unpack_envelope",
    "unpack_header",
    "unpack_message",
]


class RawBody(BaseModel):
    """Fallback body for wire messages whose type_id has no registered class.

    Lets the codec decode a message whose *header* is valid and whose
    *body* is well-framed even when no body decoder is installed. The
    body bytes are kept verbatim so callers can inspect them, forward
    them through a gateway, or decode them later once a handler is
    registered.

    Only produced when ``unpack_message`` / ``unpack_envelope`` is
    called with ``loose=True``; the default ``loose=False`` raises
    :class:`UnknownMessageTypeError` instead.
    """

    model_config = ConfigDict(arbitrary_types_allowed=True)

    type_id: str
    body: bytes


# --- unpack --------------------------------------------------------


def unpack_message(
    header: Header,
    body: bytes,
    *,
    loose: bool = False,
    verify_crc: bool = True,
) -> Envelope[Any]:
    """Decode one message's body and pair it with *header*.

    This is step 4 of the four-step pattern: callers that have just
    read (or sliced out) exactly ``header.body_size`` body bytes use
    this to produce the final typed :class:`Envelope`.

    Args:
        header: The already-parsed header. Carries the ``type_id``
            that drives body dispatch and the declared CRC.
        body: Exactly ``header.body_size`` body bytes. Callers that
            have a longer buffer must slice first.
        loose: If ``True``, unknown ``type_id`` values produce an
            Envelope whose body is :class:`RawBody`. If ``False``
            (default), unknown types raise
            :class:`UnknownMessageTypeError`.
        verify_crc: If ``True`` (default), computes CRC-64 on *body*
            and compares against ``header.crc``. Mismatches raise
            :class:`CrcMismatchError`. Set ``False`` to skip the
            check (acceptable only when an upstream layer has already
            validated, or for bulk replay of pre-verified fixtures).

    Raises:
        MalformedMessageError: *body* length disagrees with
            ``header.body_size``.
        CrcMismatchError: ``verify_crc`` is True and the CRC doesn't
            match.
        UnknownMessageTypeError: ``loose`` is False and no body class
            is registered for ``header.type_id``.
        ProtocolError: The registered body class's ``unpack`` raised
            during decoding (malformed body contents).
    """
    if len(body) != header.body_size:
        raise MalformedMessageError(
            f"body length {len(body)} does not match "
            f"header.body_size {header.body_size}"
        )

    if verify_crc:
        computed = crc64(body)
        if computed != header.crc:
            raise CrcMismatchError(
                f"CRC mismatch: header=0x{header.crc:016X}, "
                f"computed=0x{computed:016X}"
            )

    # v1 messages carry the body content verbatim. v2/v3 wrap it in
    # [12-byte extended header | content | metadata]; the body class
    # expects only the content slice. We peel the ext header + the
    # trailing metadata region off before dispatching.
    content = _extract_content(header, body)

    cls = lookup_message_class(header.type_id)
    decoded: BaseModel
    if cls is not None:
        try:
            decoded = cls.unpack(content)
        except (ValueError, ProtocolError) as e:
            # Rewrap ValueError from Pydantic / struct.unpack as a
            # ProtocolError so callers only need to catch one family.
            raise ProtocolError(
                f"failed to decode {header.type_id}: {e}"
            ) from e
    elif loose:
        # RawBody keeps the *original* body bytes (including the v2
        # ext-header/metadata regions) so gateway code that wants to
        # re-emit the wire untouched still works.
        decoded = RawBody(type_id=header.type_id, body=body)
    else:
        raise UnknownMessageTypeError(
            f"no codec registered for type_id={header.type_id!r}; "
            f"pass loose=True to accept as RawBody, or register a "
            f"body class via oigtl.register_message_type()"
        )

    return Envelope(header=header, body=decoded)


# Minimum v2 extended-header size. A v2/v3 body always starts with
# at least this many bytes encoding (ext_header_size, meta_header_size,
# meta_size, message_id). Any v2/v3 body shorter than this is
# malformed.
_V2_EXT_HEADER_MIN_SIZE = 12


def _extract_content(header: Header, body: bytes) -> bytes:
    """Return the content slice of *body* for this header version.

    - v1: body == content, returned as-is.
    - v2/v3: [ext_header | content | metadata] — strip both ends.

    Raises :class:`MalformedMessageError` if the v2/v3 framing
    fields declare region sizes that don't fit in the available
    body bytes.
    """
    if header.version < 2:
        return body

    if len(body) < _V2_EXT_HEADER_MIN_SIZE:
        raise MalformedMessageError(
            f"v{header.version} body too short for extended header: "
            f"{len(body)} < {_V2_EXT_HEADER_MIN_SIZE}"
        )
    # Layout: HH HH II II  (big-endian: ext_hdr_size, meta_hdr_size,
    # meta_size, message_id).
    ext_header_size = int.from_bytes(body[0:2], "big")
    meta_header_size = int.from_bytes(body[2:4], "big")
    meta_size = int.from_bytes(body[4:8], "big")

    if ext_header_size < _V2_EXT_HEADER_MIN_SIZE or ext_header_size > len(body):
        raise MalformedMessageError(
            f"bogus extended_header_size {ext_header_size} "
            f"(body is {len(body)} bytes)"
        )
    metadata_total = meta_header_size + meta_size
    if metadata_total > len(body) - ext_header_size:
        raise MalformedMessageError(
            f"declared metadata region ({metadata_total} bytes) "
            f"exceeds remaining body after ext header "
            f"({len(body) - ext_header_size} bytes)"
        )
    content_end = len(body) - metadata_total
    return body[ext_header_size:content_end]


def unpack_envelope(
    wire: bytes,
    *,
    loose: bool = False,
    verify_crc: bool = True,
) -> Envelope[Any]:
    """Decode a complete wire message (header + body) in one call.

    Convenience for callers who already hold the full bytes of one
    message in memory — MQTT payloads, file slices, unit-test
    fixtures, browser ``onmessage`` handlers. Streaming transports
    (TCP, incrementally-read WS) use the two-step pair
    :func:`unpack_header` + :func:`unpack_message` instead, because
    they can't know ``body_size`` ahead of time.

    The ``wire`` buffer must be exactly ``HEADER_SIZE +
    header.body_size`` bytes long; extra trailing bytes are treated
    as framing corruption and raise :class:`MalformedMessageError`.
    (Callers who want to process a concatenated stream should iterate
    with :func:`unpack_header` on the leading bytes to discover
    ``body_size``, then slice.)

    Args:
        wire: The full message bytes.
        loose: See :func:`unpack_message`.
        verify_crc: See :func:`unpack_message`.

    Raises:
        ShortBufferError: *wire* is shorter than one complete message.
        MalformedMessageError: *wire* is longer than exactly one
            message (trailing garbage).
        See also the exceptions documented on :func:`unpack_message`.
    """
    if len(wire) < HEADER_SIZE:
        raise ShortBufferError(
            f"wire shorter than header: {len(wire)} < {HEADER_SIZE}"
        )

    try:
        header = unpack_header(wire[:HEADER_SIZE])
    except ValueError as e:
        raise MalformedMessageError(str(e)) from e

    expected = HEADER_SIZE + header.body_size
    if len(wire) < expected:
        raise ShortBufferError(
            f"wire truncated: header declares body_size="
            f"{header.body_size} (needs {expected} total), got "
            f"{len(wire)}"
        )
    if len(wire) > expected:
        raise MalformedMessageError(
            f"wire has trailing bytes: header declares "
            f"body_size={header.body_size} (expected {expected} "
            f"total), got {len(wire)}"
        )

    body = wire[HEADER_SIZE:expected]
    return unpack_message(
        header, body, loose=loose, verify_crc=verify_crc,
    )


# --- pack ----------------------------------------------------------


def pack_envelope(envelope: Envelope[Any]) -> bytes:
    """Serialize *envelope* to its complete wire representation.

    Round-trips with :func:`unpack_envelope`: for any Envelope
    produced by ``unpack_envelope(w)``, ``pack_envelope(env)`` returns
    wire bytes that byte-compare equal to ``w``.

    The CRC carried in the header is **recomputed** from the body.
    An envelope that was loaded with ``verify_crc=False`` and whose
    header.crc has intentionally been left stale will therefore round
    trip to a *different* header on re-pack — the canonical CRC wins.
    This is the correct behaviour for a serialiser; it keeps the
    produced bytes internally consistent regardless of where the
    envelope came from.

    The body bytes come from the body object's ``pack()`` method for
    typed messages, or from ``body.body`` verbatim for
    :class:`RawBody` fallbacks. Both paths reproduce the original
    wire body bit-for-bit.

    Args:
        envelope: The Envelope to serialize. Must have a ``header``
            (with at minimum ``version``, ``type_id``, ``device_name``,
            ``timestamp``) and a body that is either a registered
            message class instance or a :class:`RawBody`.

    Raises:
        AttributeError: the body isn't a RawBody and has no ``pack``
            method. Means the caller either constructed the envelope
            with a non-message object or is using a registered class
            that's missing the expected contract.
    """
    body = envelope.body
    if isinstance(body, RawBody):
        body_bytes = body.body
    else:
        # Duck-typed call. Any registered body class exposes .pack().
        body_bytes = body.pack()  # type: ignore[union-attr]

    header = envelope.header
    header_bytes = pack_header(
        version=header.version,
        type_id=header.type_id,
        device_name=header.device_name,
        timestamp=header.timestamp,
        body=body_bytes,
    )
    return header_bytes + body_bytes
