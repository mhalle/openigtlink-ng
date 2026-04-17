"""Runtime primitives for the OpenIGTLink wire codec.

The :mod:`oigtl_corpus_tools.codec` package implements the actual
field-walking pack/unpack logic; we re-export the pieces this
library needs and add typed wrappers for the public surface
(header dataclass, oracle result, exception hierarchy, dispatch
registry).
"""

from oigtl.runtime.exceptions import (
    CrcMismatchError,
    MalformedMessageError,
    ProtocolError,
    ShortBufferError,
    UnknownMessageTypeError,
)
from oigtl.runtime.header import HEADER_SIZE, Header, pack_header, unpack_header

__all__ = [
    "HEADER_SIZE",
    "Header",
    "pack_header",
    "unpack_header",
    "ProtocolError",
    "CrcMismatchError",
    "MalformedMessageError",
    "ShortBufferError",
    "UnknownMessageTypeError",
]
