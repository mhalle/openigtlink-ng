"""Path helpers shared across subcommands.

The corpus-tools CLI is almost always invoked from within the repository
tree. Subcommands locate spec files (``spec/meta-schema.json``,
``spec/schemas/*.json``, ``spec/corpus/``) by walking upward from the
current working directory (or from a caller-supplied path) until a
directory containing the expected markers is found. This keeps the tool
usable whether invoked from the repo root, from ``corpus-tools/``, or
from anywhere else inside the tree, without hard-coding layout.
"""

from __future__ import annotations

from pathlib import Path


_REPO_MARKER = Path("spec") / "meta-schema.json"


class RepoRootNotFound(RuntimeError):
    """Raised when no openigtlink-ng repo root can be located."""


def find_repo_root(start: Path | None = None) -> Path:
    """Walk upward from ``start`` looking for the openigtlink-ng repo root.

    A directory is the repo root if it contains ``spec/meta-schema.json``.
    Defaults to searching upward from the current working directory.

    Raises ``RepoRootNotFound`` if no ancestor matches.
    """
    origin = (start or Path.cwd()).resolve()
    for candidate in [origin, *origin.parents]:
        if (candidate / _REPO_MARKER).is_file():
            return candidate
    raise RepoRootNotFound(
        f"Could not locate an openigtlink-ng repository root "
        f"(no ancestor of {origin} contains {_REPO_MARKER})."
    )


def spec_dir(repo_root: Path) -> Path:
    """Return the ``spec/`` directory inside a repo root."""
    return repo_root / "spec"


def meta_schema_path(repo_root: Path) -> Path:
    """Return the path to the meta-schema used to validate message schemas."""
    return repo_root / _REPO_MARKER


def schemas_dir(repo_root: Path) -> Path:
    """Return the directory holding per-message-type JSON schemas."""
    return repo_root / "spec" / "schemas"
