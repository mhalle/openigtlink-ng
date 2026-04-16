"""Support ``python -m oigtl_corpus_tools`` as an alternative to the
installed ``oigtl-corpus`` entry point."""

from oigtl_corpus_tools.cli import main

if __name__ == "__main__":
    raise SystemExit(main())
