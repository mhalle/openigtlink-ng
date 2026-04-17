"""Fuzzing infrastructure for the conformance corpus.

Two pieces live here:

- :mod:`.generators` produces candidate byte sequences — uniform
  random, mutate-from-fixture, and structurally-constructed.
- :mod:`.runner` feeds candidates through each of the four oracles
  (reference Python, typed Python, typed C++, typed TS) and asserts
  they agree on the decision and (when accepting) the semantic
  outputs.

The `oigtl-corpus fuzz differential` CLI is the front-end for both.
"""
