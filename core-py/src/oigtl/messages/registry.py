"""Public registry of message body classes, keyed by wire ``type_id``.

The generated :mod:`oigtl.messages` module ships a ``REGISTRY`` dict
populated at import time with every built-in OpenIGTLink type_id →
class mapping. This module exposes a thin public API on top of that
dict so that:

- Built-ins and user-supplied extension classes share one lookup path.
- Third-party code can register additional message types (e.g. the
  PlusToolkit ``TRACKEDFRAME`` extension or vendor-specific messages)
  without patching core.
- :mod:`oigtl.codec` has a single well-defined dispatch surface.

The built-in types are imported lazily on first registry access. This
avoids any chance of a circular import between ``oigtl.codec`` and
``oigtl.messages``, and keeps the registry module importable from
anywhere without pulling in the (large) generated message module
until it is actually needed.

Example — registering a third-party extension::

    from oigtl import register_message_type

    class TrackedFrame(BaseModel):
        TYPE_ID = "TRACKEDFRAME"
        @classmethod
        def unpack(cls, body: bytes) -> "TrackedFrame": ...
        def pack(self) -> bytes: ...

    register_message_type(TrackedFrame.TYPE_ID, TrackedFrame)

After that, :func:`oigtl.unpack_envelope` decodes TRACKEDFRAME wire
messages into ``Envelope[TrackedFrame]`` with no further ceremony.
"""

from __future__ import annotations

from typing import Any

__all__ = [
    "RegistryConflictError",
    "register_message_type",
    "unregister_message_type",
    "lookup_message_class",
    "registered_types",
]


class RegistryConflictError(ValueError):
    """Raised when :func:`register_message_type` would overwrite an entry.

    Pass ``override=True`` to force replacement (intended for tests
    and deliberate swaps; not for silent conflict resolution).
    """


# The canonical mutable registry. Built-ins are loaded on first access;
# subsequent user registrations live in this same dict so there is one
# lookup path, not two.
_REGISTRY: dict[str, type[Any]] = {}

# Guard so the built-in bootstrap runs at most once.
_BUILTINS_LOADED = False


def _load_builtins() -> None:
    """Populate the registry with the generated built-in mappings.

    Called lazily on the first call to any public registry function.
    Safe to call repeatedly; the second call is a no-op.
    """
    global _BUILTINS_LOADED
    if _BUILTINS_LOADED:
        return
    _BUILTINS_LOADED = True

    # Import inside the function to keep registry.py side-effect-free
    # at module load and avoid any ordering issues with
    # oigtl.messages (which pulls in many generated modules).
    from oigtl.messages import REGISTRY as _BUILTIN  # noqa: WPS433

    # setdefault so that anything already registered before the
    # bootstrap ran (unusual but possible — e.g. an extension imported
    # before oigtl.messages) wins over the built-in.
    for type_id, cls in _BUILTIN.items():
        _REGISTRY.setdefault(type_id, cls)


def register_message_type(
    type_id: str,
    cls: type[Any],
    *,
    override: bool = False,
) -> None:
    """Register *cls* as the body class for wire ``type_id``.

    After registration, :func:`oigtl.codec.unpack_message` and
    :func:`oigtl.codec.unpack_envelope` dispatch wire messages whose
    header ``type_id`` matches *type_id* through ``cls.unpack(body)``.

    Args:
        type_id: The wire type_id string (up to 12 ASCII chars per
            the OpenIGTLink spec). Matches exactly — no case folding.
        cls: The body class. Must expose ``cls.unpack(body: bytes)``
            returning an instance, and ``instance.pack()`` returning
            body bytes. Typically a Pydantic model following the same
            shape as the built-in classes in :mod:`oigtl.messages`.
        override: If ``True``, silently replace any existing entry.
            If ``False`` (the default) and *type_id* is already
            registered to a different class, raises
            :class:`RegistryConflictError`. Re-registering the *same*
            (type_id, cls) pair is always idempotent, override or not.

    Raises:
        RegistryConflictError: *type_id* is already bound to a
            different class and *override* is False.
    """
    _load_builtins()
    existing = _REGISTRY.get(type_id)
    if existing is not None and existing is not cls and not override:
        raise RegistryConflictError(
            f"type_id {type_id!r} is already registered to "
            f"{existing.__module__}.{existing.__qualname__}; "
            f"pass override=True to replace it, or pick a different "
            f"type_id"
        )
    _REGISTRY[type_id] = cls


def unregister_message_type(type_id: str) -> type[Any] | None:
    """Remove and return the class bound to *type_id*, if any.

    Primarily useful in tests that need to undo a registration
    between cases. Production code should not need this; once a
    message type is registered, it typically stays registered for
    the lifetime of the process.

    Returns the class that was previously bound, or ``None`` if
    *type_id* was not registered.
    """
    _load_builtins()
    return _REGISTRY.pop(type_id, None)


def lookup_message_class(type_id: str) -> type[Any] | None:
    """Return the class registered for *type_id*, or ``None``.

    Unlike :func:`register_message_type`, this never raises — callers
    can use ``None`` as the signal to fall back to a raw-body path
    (see :class:`oigtl.codec.RawBody`).
    """
    _load_builtins()
    return _REGISTRY.get(type_id)


def registered_types() -> list[str]:
    """Return a sorted list of all currently-registered type_ids.

    Useful for tooling (CLI ``info`` output, documentation generators,
    sanity checks in tests). The list is a snapshot — registering
    more types later has no effect on previously-returned lists.
    """
    _load_builtins()
    return sorted(_REGISTRY)
