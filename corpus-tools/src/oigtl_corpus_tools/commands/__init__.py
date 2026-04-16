"""Subcommand implementations for the ``oigtl-corpus`` CLI.

Each submodule exposes a ``register(parser)`` function that attaches its
own subparsers and handler callables to the argparse ``ArgumentParser``
passed in. Handlers take the parsed argparse ``Namespace`` and return an
integer exit code.
"""
