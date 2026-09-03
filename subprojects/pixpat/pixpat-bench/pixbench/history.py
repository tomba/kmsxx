"""Measuring across git history: export each commit, build every
subject from it (cached by the content of the subtrees it builds
from), skip what came out byte-identical to the previous commit, and
measure the rest."""

from __future__ import annotations

import pathlib
import shutil
import subprocess

from .build import LANG_PATHS, BuildConfig, subtree_hashes
from .util import CommandError, git


def commits(
    repo: pathlib.Path, revs: str | None, commit_list: str | None, last: int | None
) -> list[str]:
    """Full SHAs, oldest first."""
    if commit_list:
        return [
            git(repo, 'rev-parse', '--verify', f'{c.strip()}^{{commit}}')
            for c in commit_list.split(',')
            if c.strip()
        ]
    if last:
        return git(repo, 'rev-list', '--reverse', '-n', str(last), 'HEAD').split()
    if revs:
        if '..' not in revs and not revs.startswith('-'):
            # A single rev would list its whole ancestry; measure just it.
            return [git(repo, 'rev-parse', '--verify', f'{revs}^{{commit}}')]
        return git(repo, 'rev-list', '--reverse', revs).split()
    raise ValueError('nothing to measure: give REVS (A..B), --commits or --last')


def export(repo: pathlib.Path, sha: str, cache: pathlib.Path) -> pathlib.Path:
    """The commit's tree, extracted under the cache (once)."""
    dest = cache / 'src' / sha
    marker = dest / '.exported'
    if marker.exists():
        return dest
    if dest.exists():
        shutil.rmtree(dest)
    dest.mkdir(parents=True)
    with subprocess.Popen(
        ['git', '-C', str(repo), 'archive', '--format=tar', sha], stdout=subprocess.PIPE
    ) as p:
        subprocess.run(['tar', '-x', '-C', str(dest)], stdin=p.stdout, check=True)
    if p.returncode != 0:
        raise CommandError(['git', 'archive', sha], p.returncode, '')
    marker.touch()
    return dest


def source_at(repo: pathlib.Path, sha: str, lang: str) -> dict:
    return {
        'commit': sha,
        'dirty': False,
        'describe': git(repo, 'describe', '--always', sha),
        'subtrees': subtree_hashes(repo, sha, LANG_PATHS[lang]),
    }


def file_at(repo: pathlib.Path, sha: str, path: str) -> bytes | None:
    proc = subprocess.run(
        ['git', '-C', str(repo), 'show', f'{sha}:{path}'], capture_output=True, check=False
    )
    return proc.stdout if proc.returncode == 0 else None


def buildable(cfg: BuildConfig, source: dict) -> str | None:
    """Why this commit cannot build this subject, or None."""
    root = LANG_PATHS[cfg.lang][0]
    if root not in source['subtrees']:
        return f'no {root}/ at this commit'
    return None
