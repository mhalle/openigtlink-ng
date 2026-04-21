"""Typed Python codec for the OpenIGTLink protocol.

The public surface splits into three concentric layers:

- **Wire codec** — pure, transport-independent decode/encode. The
  ``unpack_*`` and ``pack_*`` family below. Sufficient on its own
  for MQTT payloads, file replays, browser-bundled consumers, and
  any other caller that already holds bytes in memory.
- **Messages + registry** — :mod:`oigtl.messages` supplies the
  built-in typed message classes (``Transform``, ``Status``, …) and
  a registration API for extensions (``register_message_type``).
  Built-ins and extensions share a single dispatch path.
- **Transports** — :mod:`oigtl.net` provides TCP/WebSocket
  clients and servers, the gateway pattern, and resilience helpers.

Each layer depends only on the ones above it.

Extending the protocol
----------------------

A new message type (PLUS's ``TRACKEDFRAME``, a vendor extension,
a research prototype) is a body class that satisfies the same
contract as the built-ins plus a one-line registration::

    class TrackedFrame(BaseModel):
        TYPE_ID = "TRACKEDFRAME"
        @classmethod
        def unpack(cls, body: bytes) -> "TrackedFrame": ...
        def pack(self) -> bytes: ...

    register_message_type(TrackedFrame.TYPE_ID, TrackedFrame)

After that single ``register_message_type`` call, the new type
decodes through :func:`unpack_envelope` and is returned by the
Client/WsClient receive loops exactly the same way a built-in
message is. No transport changes; no special-casing; no fork of
the core library.
"""

from oigtl.codec import (
    HEADER_SIZE,
    Header,
    RawBody,
    pack_envelope,
    pack_header,
    unpack_envelope,
    unpack_header,
    unpack_message,
)
from oigtl.messages.registry import (
    RegistryConflictError,
    lookup_message_class,
    register_message_type,
    registered_types,
    unregister_message_type,
)

__version__ = "0.1.0"

__all__ = [
    "__version__",
    # Wire codec
    "HEADER_SIZE",
    "Header",
    "RawBody",
    "pack_envelope",
    "pack_header",
    "unpack_envelope",
    "unpack_header",
    "unpack_message",
    # Registry
    "RegistryConflictError",
    "lookup_message_class",
    "register_message_type",
    "registered_types",
    "unregister_message_type",
]
