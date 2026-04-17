"""C++ wire-codec code generation from message schemas.

Translates ``spec/schemas/*.json`` into C++17 source. Public surface:

- :func:`cpp_emit.render_message` — render one message's ``.hpp`` and
  ``.cpp`` text from a loaded schema dict.
- :func:`cpp_emit.write_message` — convenience: render and write.

The :mod:`.cpp_types` module owns the schema-type → C++-type mapping
and is the only place to extend when a new field shape ships.

Phase 2 scope: TRANSFORM-class only — primitives and arrays with a
fixed integer ``count`` whose elements are primitives. Phase 3+
extends to length-prefixed strings, sibling-counted arrays, structs,
etc. The mapping table is defined declaratively so each phase adds
one branch in :mod:`.cpp_types` and (if needed) a Jinja macro.
"""
