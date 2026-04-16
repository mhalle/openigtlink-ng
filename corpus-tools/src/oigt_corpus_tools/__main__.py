"""Support ``python -m oigt_corpus_tools`` as an alternative to the
installed ``oigt-corpus`` entry point."""

from oigt_corpus_tools.cli import main

if __name__ == "__main__":
    raise SystemExit(main())
