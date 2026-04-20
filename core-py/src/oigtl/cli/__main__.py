"""``python -m oigtl`` → :func:`oigtl.cli.main`.

The ``[project.scripts]`` entry in ``pyproject.toml`` gives us
``oigtl`` on $PATH; this file gives the same behaviour to
``python -m oigtl`` so developers who've checked out the source
without installing the wheel still get the CLI.
"""

from __future__ import annotations

from oigtl.cli import main

if __name__ == "__main__":
    main()
