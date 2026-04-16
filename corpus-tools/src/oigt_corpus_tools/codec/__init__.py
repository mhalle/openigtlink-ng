"""Reference codec for OpenIGTLink messages.

Schema-driven pack/unpack — every message type is handled by walking
the corresponding JSON schema under ``spec/schemas/``. No per-message
code; correctness over performance.

Public API:

- :func:`unpack_message` — parse a complete wire message (header + body).
- :func:`pack_message` — build a complete wire message.
- :func:`unpack_body` — parse just the body given a loaded schema.
- :func:`pack_body` — serialize just the body given a loaded schema.
- :func:`load_schema` — load and validate a schema by type_id.
"""

from oigt_corpus_tools.codec.header import (
    HEADER_SIZE,
    pack_header,
    unpack_header,
)
from oigt_corpus_tools.codec.message import (
    load_schema,
    pack_body,
    pack_message,
    unpack_body,
    unpack_message,
)

__all__ = [
    "HEADER_SIZE",
    "load_schema",
    "pack_body",
    "pack_header",
    "pack_message",
    "unpack_body",
    "unpack_header",
    "unpack_message",
]
