"""Run files: what a measurement writes, and what the readers load.

A run file holds one invocation's subjects (what was built, from
where, how, and how big it came out), their fingerprints and the
throughput records, plus where and by which tool version it was
taken. The results directory is the database; readers glob it.
"""

from __future__ import annotations

import datetime
import json
import os
import pathlib
import re

from .util import CommandError, git

SCHEMA = 1


def repo_root() -> pathlib.Path:
    return pathlib.Path(__file__).resolve().parents[2]


def results_dir(arg: str | None) -> pathlib.Path:
    return pathlib.Path(arg or os.environ.get('PIXBENCH_RESULTS') or repo_root() / 'measurements')


def cache_dir(arg: str | None) -> pathlib.Path:
    return pathlib.Path(arg or os.environ.get('PIXBENCH_CACHE') or repo_root() / '.pixbench-cache')


def manifests_dir(results: pathlib.Path) -> pathlib.Path:
    return results / 'manifests'


def tool_info() -> dict:
    root = repo_root()
    try:
        return {
            'commit': git(root, 'rev-parse', 'HEAD'),
            'dirty': bool(git(root, 'status', '--porcelain', '--', 'pixpat-bench', 'scripts')),
        }
    except (CommandError, FileNotFoundError):
        return {'commit': None, 'dirty': None}


class Run:
    def __init__(self, label: str | None, argv: list[str], machine: dict) -> None:
        self.time = datetime.datetime.now(datetime.timezone.utc)
        self.data: dict = {
            'schema': SCHEMA,
            'run': {
                'id': None,
                'time': self.time.isoformat(timespec='seconds'),
                'label': label,
                'argv': argv,
                'tool': tool_info(),
                'machine': machine,
            },
            'subjects': {},
            'fingerprint': {},
            'perf': [],
            'notes': [],
        }

    def add_subject(self, name: str, *, lang: str, source: dict, build: dict, lib: dict) -> None:
        self.data['subjects'][name] = {'lang': lang, 'source': source, 'build': build, 'lib': lib}

    def add_fingerprint(self, name: str, summary: dict) -> None:
        self.data['fingerprint'][name] = summary

    def add_perf(self, records: list[dict]) -> None:
        self.data['perf'].extend(records)

    def add_notes(self, notes: list[str]) -> None:
        self.data['notes'].extend(notes)

    def write(self, results: pathlib.Path) -> pathlib.Path:
        results.mkdir(parents=True, exist_ok=True)
        stamp = self.time.strftime('%Y%m%dT%H%M%SZ')
        label = re.sub(r'[^A-Za-z0-9_.+-]+', '-', self.data['run']['label'] or 'run').strip('-')
        base = f'{stamp}-{label}'
        path = results / f'{base}.json'
        n = 2
        while path.exists():
            path = results / f'{base}-{n}.json'
            n += 1
        self.data['run']['id'] = path.stem
        path.write_text(json.dumps(self.data, indent=1) + '\n')
        return path


def subject_from_build(info: dict) -> dict:
    """The subject entry a build record (build.json) turns into."""
    cfg = info['config']
    return {
        'lang': cfg['lang'],
        'source': info['source'],
        'build': {
            **{k: cfg.get(k) for k in ('cc', 'cxx', 'arch', 'lto', 'opt', 'buildtype')},
            'profile': cfg.get('profile'),
            'profile_sha256': info.get('profile_sha256'),
            'toolchain_versions': info.get('toolchain_versions', {}),
            'config_hash': info.get('config_hash'),
            'error': info.get('error'),
        },
        'lib': dict(info.get('lib') or {}),
    }


def load_run(path: str | pathlib.Path) -> dict:
    p = pathlib.Path(path)
    data = json.loads(p.read_text())
    if data.get('schema') != SCHEMA:
        raise ValueError(f'{p}: schema {data.get("schema")}, expected {SCHEMA}')
    data['_path'] = str(p)
    return data


def load_runs(results: pathlib.Path) -> list[dict]:
    runs = []
    for p in sorted(results.glob('*.json')):
        try:
            runs.append(load_run(p))
        except (ValueError, json.JSONDecodeError) as e:
            print(f'skipping {p}: {e}')
    runs.sort(key=lambda r: (r['run']['time'], r['_path']))
    return runs


def commit_of(subject: dict) -> str | None:
    return (subject.get('source') or {}).get('commit')


def short_commit(subject: dict) -> str:
    c = commit_of(subject)
    if not c:
        return 'work'
    return c[:10] + ('+' if subject['source'].get('dirty') else '')


def subject_label(name: str, subject: dict) -> str:
    return f'{name}@{short_commit(subject)}'


def resolve_selector(sel: str, results: pathlib.Path, repo: pathlib.Path) -> tuple[dict, str]:
    """`RUN.json[#SUBJECT]`, `@REV[:SUBJECT]` (latest record of that
    commit), or a bare `SUBJECT` (its latest record)."""
    if sel.startswith('@'):
        rev, _, subj = sel[1:].partition(':')
        try:
            sha = git(repo, 'rev-parse', '--verify', '-q', f'{rev}^{{commit}}')
        except CommandError:
            sha = rev
        for run in reversed(load_runs(results)):
            names = [
                n
                for n, s in run['subjects'].items()
                if (commit_of(s) or '').startswith(sha) and (not subj or n == subj)
            ]
            if not names:
                continue
            if len(names) > 1:
                raise ValueError(
                    f'{sel}: several subjects in {run["_path"]}: {", ".join(names)}; '
                    f'pick one with @{rev}:NAME'
                )
            return run, names[0]
        raise ValueError(f'{sel}: no measurement of that commit in {results}')
    path, _, subj = sel.partition('#')
    if path.endswith('.json') or '/' in path:
        run = load_run(path)
    else:
        subj = path
        runs = [r for r in reversed(load_runs(results)) if subj in r['subjects']]
        if not runs:
            raise ValueError(f'{sel}: no run in {results} has a subject named {subj!r}')
        run = runs[0]
    names = list(run['subjects'])
    if not subj:
        if len(names) != 1:
            raise ValueError(f'{path} has subjects {", ".join(names)}; pick one with #NAME')
        subj = names[0]
    if subj not in run['subjects']:
        raise ValueError(f'{path}: no subject {subj!r} (has {", ".join(names)})')
    return run, subj
