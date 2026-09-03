"""Small shared helpers: hashing, subprocesses, git."""

from __future__ import annotations

import hashlib
import math
import pathlib
import subprocess
from collections.abc import Iterable


def file_sha256(path: str | pathlib.Path) -> str:
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def sha256_text(text: str) -> str:
    return hashlib.sha256(text.encode('utf-8')).hexdigest()


class CommandError(Exception):
    def __init__(self, cmd: list[str], rc: int, output: str) -> None:
        super().__init__(f'{" ".join(cmd)} failed with {rc}')
        self.cmd = cmd
        self.rc = rc
        self.output = output


def run(
    cmd: list[str],
    *,
    cwd: str | pathlib.Path | None = None,
    env: dict[str, str] | None = None,
    log=None,
) -> str:
    """Run a command, return its stdout. stderr goes to `log` (a file
    object) when given, else is folded into the error on failure."""
    proc = subprocess.run(
        cmd,
        cwd=cwd,
        env=env,
        stdout=subprocess.PIPE,
        stderr=log if log is not None else subprocess.STDOUT,
        text=True,
        check=False,
    )
    if proc.returncode != 0:
        raise CommandError(cmd, proc.returncode, proc.stdout or '')
    return proc.stdout


def git(repo: str | pathlib.Path, *args: str) -> str:
    return run(['git', '-C', str(repo), *args]).strip()


def git_toplevel(path: pathlib.Path) -> pathlib.Path | None:
    """The git work tree `path` is the root of, or None when `path` is
    not a work-tree root itself (an exported tree inside the repo's
    cache dir must not be mistaken for the repo)."""
    try:
        top = git(path, 'rev-parse', '--show-toplevel')
    except (CommandError, FileNotFoundError):
        return None
    top_path = pathlib.Path(top).resolve()
    return top_path if top_path == path.resolve() else None


def first_line(text: str) -> str:
    return text.strip().splitlines()[0] if text.strip() else ''


def geomean(values: Iterable[float]) -> float | None:
    vals = [v for v in values if v > 0]
    if not vals:
        return None
    return math.exp(sum(math.log(v) for v in vals) / len(vals))
