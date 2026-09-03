"""The measurement proper, shared by `measure` and `history`: given
named libraries, record their sizes, fingerprints and throughput into
a run."""

from __future__ import annotations

import pathlib
import sys
from collections.abc import Callable

from . import fingerprint as fp
from .abi import Lib, load
from .cases import Case, Filter
from .perf import PerfOpts
from .perf import run as run_perf
from .size import lib_size
from .store import Run, manifests_dir


def measure(
    run: Run,
    libs: dict[str, pathlib.Path],
    *,
    filt: Filter,
    cases: list[Case],
    perf_opts: PerfOpts,
    results: pathlib.Path,
    do_fingerprint: bool = True,
    do_perf: bool = True,
    tier: str = 'full',
    fp_seed: int = 1,
    log: Callable[[str], None] = lambda m: print(m, file=sys.stderr),
    on_perf: Callable[[str, dict], None] | None = None,
) -> None:
    """`libs` maps subject name -> library path; every name must already
    be a subject of `run` (its 'lib' entry gains the sizes)."""
    names = list(libs)
    loaded: list[Lib] = load([libs[n] for n in names])
    for name, lib in zip(names, loaded):
        sizes = lib_size(lib.path)
        run.data['subjects'][name]['lib'].update(sizes)
        log(f'{name}: {lib.path} (.text {sizes["sections"].get(".text")} bytes)')

    if do_fingerprint:
        for name, lib in zip(names, loaded):
            m = fp.fingerprint(lib, filt, tier, fp_seed)
            path = fp.write_manifest(m, manifests_dir(results))
            run.add_fingerprint(name, fp.summary(m, path))
            state = 'ok' if not m['self_check_failures'] else 'SELF-CHECK FAILED'
            log(f'{name}: fingerprint {m["digest"][:16]} ({m["cases"]} cases, {state})')
            for w in m['warnings']:
                log(f'{name}: warning: {w}')

    if do_perf:
        cases = [c for c in cases if filt.matches(c)]
        log(f'timing {len(cases)} cases x {len(names)} libraries')

        def on_result(rec: dict) -> None:
            if on_perf:
                on_perf(names[rec['lib']], rec)

        records, notes = run_perf(loaded, cases, perf_opts, on_result)
        for rec in records:
            rec['subject'] = names[rec.pop('lib')]
        run.add_perf(records)
        run.add_notes(notes)
        for n in notes:
            log(f'note: {n}')
