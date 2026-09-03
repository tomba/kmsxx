"""Command line: one small subcommand per job, sharing the case
filters, the timing options and the results/cache directories."""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import sys

from . import fingerprint as fp
from . import history as hist
from . import machine
from .abi import load
from .build import (
    BuildError,
    build,
    ensure_built,
    parse_subject,
    resolve_profile,
    source_info,
)
from .cases import Filter, curated, everything
from .measure import measure
from .perf import PerfOpts
from .perf import run as run_perf
from .report import borrow, compare, export_text, render, report
from .size import lib_size, ordered_sections
from .store import (
    Run,
    cache_dir,
    load_runs,
    manifests_dir,
    repo_root,
    resolve_selector,
    results_dir,
    subject_from_build,
)
from .util import CommandError, file_sha256, git


def log(msg: str) -> None:
    print(msg, file=sys.stderr, flush=True)


def _fmt(v) -> str:
    if v is None:
        return ''
    return f'{v:.0f}' if isinstance(v, float) else str(v)


def _lib_spec(spec: str) -> tuple[str, pathlib.Path]:
    """`[NAME=]PATH`; the default name is the file's stem."""
    name, eq, path = spec.partition('=')
    if not eq:
        path, name = spec, pathlib.Path(spec).stem
    return name, pathlib.Path(path).resolve()


def _prebuilt_subject(name: str, path: pathlib.Path) -> dict:
    if not path.is_file():
        raise FileNotFoundError(path)
    return subject_from_build(
        {
            'source': {'commit': None, 'dirty': None, 'describe': None, 'subtrees': {}},
            'config': {'lang': 'cpp'},
            'lib': {'path': str(path), 'sha256': file_sha256(path)},
        }
    )


def perf_table(records: list[dict], names: list[str], fmt: str) -> str:
    keys: list[tuple] = []
    cells: dict[tuple, dict] = {}
    for r in records:
        k = (r['case'], r['threads'])
        if k not in cells:
            keys.append(k)
            cells[k] = {}
        cells[k][r['subject']] = r['mpx_s']
    multi_t = len({k[1] for k in keys}) > 1
    headers = ['case', *(['threads'] if multi_t else []), *[f'{n} Mpx/s' for n in names]]
    rows = [
        [k[0], *([k[1]] if multi_t else []), *[_fmt(cells[k].get(n)) for n in names]] for k in keys
    ]
    return render(headers, rows, fmt)


def _perf_progress(name: str, rec: dict) -> None:
    log(f'  {rec["case"]:32} t={rec["threads"]:<2} {name:14} {rec["mpx_s"]:8.0f} Mpx/s')


# --- subcommands


def cmd_build(args) -> int:
    name, cfg = parse_subject(args.subject)
    repo, cache = repo_root(), cache_dir(args.cache)
    try:
        if args.commit:
            sha = git(repo, 'rev-parse', '--verify', f'{args.commit}^{{commit}}')
            source = hist.source_at(repo, sha, cfg.lang)
            why = hist.buildable(cfg, source)
            if why:
                log(f'{name}: cannot build at {sha[:10]}: {why}')
                return 1
            src = hist.export(repo, sha, cache)
        else:
            src = pathlib.Path(args.tree).resolve()
            source = source_info(src, cfg.lang)
        if args.out:
            info = build(cfg, src, pathlib.Path(args.out), source=source, log=log)
        else:
            info = ensure_built(
                cfg,
                cache=cache,
                source=source,
                profile_sha256=file_sha256(resolve_profile(cfg, src)),
                get_src=lambda: src,
                force=args.force,
                log=log,
            )
    except (BuildError, CommandError, FileNotFoundError) as e:
        log(f'{name}: {e}')
        return 1
    print(info['lib']['path'])
    return 0


def cmd_size(args) -> int:
    infos = [(p, lib_size(p)) for p in args.lib]
    if args.json:
        print(json.dumps({p: i for p, i in infos}, indent=1))
        return 0
    names = [pathlib.Path(p).name for p in args.lib]
    if len(set(names)) != len(names):
        names = list(args.lib)
    rows = []
    for metric in ('bytes', 'stripped_bytes', 'alloc_bytes'):
        rows.append([metric, *[_fmt(i[metric]) for _, i in infos]])
    seen: list[str] = []
    for _, i in infos:
        for sec, _size in ordered_sections(i['sections']):
            if sec not in seen:
                seen.append(sec)
    for sec in seen:
        rows.append([sec, *[_fmt(i['sections'].get(sec)) for _, i in infos]])
    print(render(['size', *names], rows, args.fmt))
    return 0


def cmd_fingerprint(args) -> int:
    filt = Filter.from_args(args)
    if args.diff:
        a, b = (fp.load_manifest(p) for p in args.diff)
        if not fp.comparable(a, b):
            print('not comparable: different tier, filter or seed')
            return 2
        diffs = fp.diff_manifests(a, b)
        for label, what in diffs:
            print(f'{label}: {what}')
        print('identical' if not diffs else f'{len(diffs)} cases differ')
        return 0 if not diffs else 1
    if args.ref:
        ref, tst = load([args.ref, args.lib])
        n, failures = fp.diff_libs(ref, tst, filt, args.tier, args.seed)
        for f in failures:
            print(f'FAIL {f}')
        print(
            f'{n - len(failures)}/{n} checks passed'
            + (f', {len(failures)} FAILED' if failures else '')
        )
        return 1 if failures else 0
    (lib,) = load([args.lib])
    m = fp.fingerprint(lib, filt, args.tier, args.seed)
    if args.out:
        path = pathlib.Path(args.out)
        path.write_text(json.dumps(m, indent=1) + '\n')
    else:
        path = fp.write_manifest(m, manifests_dir(results_dir(args.results)))
    print(f'digest    {m["digest"]}')
    for g, d in sorted(m['group_digests'].items()):
        print(f'  {g:16} {d[:16]}  {len(m["groups"][g]):6} cases')
    print(f'cases     {m["cases"]}  (tier {m["tier"]}, filter {m["filter"]}, seed {m["seed"]})')
    print(f'manifest  {path}')
    for w in m['warnings']:
        print(f'warning   {w}')
    for s in m['self_check_failures']:
        print(f'FAIL      {s}')
    return 1 if m['self_check_failures'] else 0


def cmd_perf(args) -> int:
    filt = Filter.from_args(args)
    opts = PerfOpts.from_args(args)
    specs = [_lib_spec(s) for s in args.lib]
    names = [n for n, _ in specs]
    if len(set(names)) != len(names):
        log('library names are not unique; use NAME=PATH')
        return 1
    libs = load([p for _, p in specs])
    cases = [c for c in (everything() if args.all else curated()) if filt.matches(c)]
    if not cases:
        log('no cases match the filter')
        return 1
    log(
        f'{len(cases)} cases x {len(libs)} libraries, {opts.w}x{opts.h}, threads '
        f'{",".join(map(str, opts.threads))}, {args.rec}/{args.range}, iters={opts.iters} '
        f'warmup={opts.warmup} rounds={opts.rounds}, cpus={opts.cpus or "unpinned"}'
    )
    records, notes = run_perf(libs, cases, opts, lambda rec: _perf_progress(names[rec['lib']], rec))
    for rec in records:
        rec['subject'] = names[rec.pop('lib')]
    print(perf_table(records, names, args.fmt))
    for n in notes:
        log(f'note: {n}')
    if args.save:
        run = Run(args.label or 'perf', sys.argv[1:], machine.info(opts.cpus))
        for name, path in specs:
            subj = _prebuilt_subject(name, path)
            subj['lib'].update(lib_size(path))
            run.add_subject(name, **subj)
        run.add_perf(records)
        run.add_notes(notes)
        log(f'wrote {run.write(results_dir(args.results))}')
    return 0


def _build_subjects(args, run: Run, cache: pathlib.Path, libs: dict[str, pathlib.Path]) -> None:
    """Resolve --tree/--commit, build every --subject (cached), add
    each as a subject of `run` and its library to `libs`."""
    repo = repo_root()
    if args.commit:
        sha = git(repo, 'rev-parse', '--verify', f'{args.commit}^{{commit}}')

        def source_for(lang):
            return hist.source_at(repo, sha, lang)

        def get_src():
            return hist.export(repo, sha, cache)

        def profile_sha(cfg):
            p = pathlib.Path(cfg.profile)
            data = p.read_bytes() if p.is_absolute() else hist.file_at(repo, sha, cfg.profile)
            if data is None:
                raise BuildError(f'{cfg.profile}: not present at {sha[:10]}')
            return hashlib.sha256(data).hexdigest()

    else:
        src = pathlib.Path(args.tree).resolve()

        def source_for(lang):
            return source_info(src, lang)

        def get_src():
            return src

        def profile_sha(cfg):
            return file_sha256(resolve_profile(cfg, src))

    for spec in args.subject or []:
        name, cfg = parse_subject(spec)
        source = source_for(cfg.lang)
        try:
            if args.commit:
                why = hist.buildable(cfg, source)
                if why:
                    raise BuildError(why)
            info = ensure_built(
                cfg,
                cache=cache,
                source=source,
                profile_sha256=profile_sha(cfg),
                get_src=get_src,
                force=args.force_build,
                log=log,
            )
        except (BuildError, CommandError, FileNotFoundError) as e:
            log(f'{name}: build failed: {e}')
            run.add_subject(
                name,
                lang=cfg.lang,
                source=source,
                build={**cfg.as_dict(), 'error': str(e)},
                lib={},
            )
            continue
        run.add_subject(name, **subject_from_build(info))
        libs[name] = pathlib.Path(info['lib']['path'])


def cmd_measure(args) -> int:
    filt = Filter.from_args(args)
    opts = PerfOpts.from_args(args)
    results, cache = results_dir(args.results), cache_dir(args.cache)
    run = Run(args.label, sys.argv[1:], machine.info(opts.cpus))
    libs: dict[str, pathlib.Path] = {}
    for spec in args.lib or []:
        name, path = _lib_spec(spec)
        subj = _prebuilt_subject(name, path)
        run.add_subject(name, **subj)
        libs[name] = path
    if not args.subject and not args.lib:
        log('nothing to measure: give --subject and/or --lib')
        return 1
    _build_subjects(args, run, cache, libs)
    if not libs:
        log('no library to measure')
        run.write(results)
        return 1
    measure(
        run,
        libs,
        filt=filt,
        cases=everything() if args.all else curated(),
        perf_opts=opts,
        results=results,
        do_fingerprint=not args.no_fingerprint,
        do_perf=not args.no_perf,
        tier=args.tier,
        fp_seed=args.fp_seed,
        log=log,
        on_perf=_perf_progress,
    )
    if run.data['perf']:
        print(perf_table(run.data['perf'], list(libs), 'text'))
    path = run.write(results)
    print(path)
    return 0


def cmd_history(args) -> int:
    filt = Filter.from_args(args)
    opts = PerfOpts.from_args(args)
    repo, results, cache = repo_root(), results_dir(args.results), cache_dir(args.cache)
    shas = hist.commits(repo, args.revs, args.commits, args.last)
    subjects = [parse_subject(s) for s in args.subject]
    prev_sha: dict[str, str] = {}
    prev_commit: dict[str, str] = {}
    failures = 0
    for sha in shas:
        short = sha[:10]
        log(f'=== {short} {git(repo, "show", "-s", "--format=%s", sha)}')
        run = Run(f'history-{short}', sys.argv[1:], machine.info(opts.cpus))
        libs: dict[str, pathlib.Path] = {}
        for name, cfg in subjects:
            source = hist.source_at(repo, sha, cfg.lang)
            why = hist.buildable(cfg, source)
            if why:
                log(f'{name}: skipped, {why}')
                continue
            p = pathlib.Path(cfg.profile)
            data = p.read_bytes() if p.is_absolute() else hist.file_at(repo, sha, cfg.profile)
            if data is None:
                log(f'{name}: skipped, {cfg.profile} not present at this commit')
                continue
            try:
                info = ensure_built(
                    cfg,
                    cache=cache,
                    source=source,
                    profile_sha256=hashlib.sha256(data).hexdigest(),
                    get_src=lambda sha=sha: hist.export(repo, sha, cache),
                    force=args.force_build,
                    log=log,
                )
            except (BuildError, CommandError, FileNotFoundError) as e:
                failures += 1
                log(f'{name}: build failed: {e}')
                run.add_subject(
                    name,
                    lang=cfg.lang,
                    source=source,
                    build={**cfg.as_dict(), 'error': str(e)},
                    lib={},
                )
                continue
            run.add_subject(name, **subject_from_build(info))
            lib_sha = info['lib']['sha256']
            if prev_sha.get(name) == lib_sha and not args.measure_unchanged:
                log(f'{name}: library unchanged since {prev_commit[name][:10]}, not re-measured')
                subj = run.data['subjects'][name]
                subj['unchanged_from'] = prev_commit[name]
                subj['lib'].update(lib_size(info['lib']['path']))
            else:
                libs[name] = pathlib.Path(info['lib']['path'])
            prev_sha[name], prev_commit[name] = lib_sha, sha
        if libs:
            measure(
                run,
                libs,
                filt=filt,
                cases=everything() if args.all else curated(),
                perf_opts=opts,
                results=results,
                do_fingerprint=not args.no_fingerprint,
                do_perf=not args.no_perf,
                tier=args.tier,
                fp_seed=args.fp_seed,
                log=log,
                on_perf=_perf_progress,
            )
        if run.data['subjects']:
            log(f'wrote {run.write(results)}')
    if failures:
        log(f'{failures} build(s) failed')
    return 1 if failures else 0


def cmd_compare(args) -> int:
    filt = Filter.from_args(args)
    repo, results = repo_root(), results_dir(args.results)
    a = resolve_selector(args.a, results, repo)
    b = resolve_selector(args.b, results, repo)
    runs = load_runs(results)
    a = (borrow(a[0], a[1], runs), a[1])
    b = (borrow(b[0], b[1], runs), b[1])
    if args.remeasure:
        opts = PerfOpts.from_args(args)
        paths = [pathlib.Path(r['subjects'][n]['lib']['path']) for r, n in (a, b)]
        libs = load(paths)
        cases = [c for c in (everything() if args.all else curated()) if filt.matches(c)]
        log(f're-measuring {len(cases)} cases on both libraries, interleaved')
        names = [a[1], b[1]]
        records, _notes = run_perf(
            libs, cases, opts, lambda rec: _perf_progress(names[rec['lib']], rec)
        )
        fresh: list[list[dict]] = [[], []]
        for rec in records:
            i = rec.pop('lib')
            rec['subject'] = names[i]
            fresh[i].append(rec)
        for (run, name), recs in zip((a, b), fresh):
            run['perf'] = [r for r in run.get('perf', []) if r.get('subject') != name] + recs
    text, ok = compare(
        a, b, filt=filt, threshold=args.threshold, verbose=args.verbose, fmt=args.fmt
    )
    print(text)
    return 0 if ok else 1


def cmd_report(args) -> int:
    filt = Filter.from_args(args)
    repo, results = repo_root(), results_dir(args.results)
    runs = load_runs(results)
    commits = None
    if args.revs or args.commits or args.last:
        commits = set(hist.commits(repo, args.revs, args.commits, args.last))
    subjects = {s.strip() for s in args.subjects.split(',')} if args.subjects else None
    print(
        report(
            runs,
            metric=args.metric,
            by=args.by,
            filt=filt,
            subjects=subjects,
            commits=commits,
            fmt=args.fmt,
            repo=repo,
        )
    )
    return 0


def cmd_export(args) -> int:
    runs = load_runs(results_dir(args.results))
    text = export_text(runs, 'csv' if args.csv else 'json')
    if args.out:
        pathlib.Path(args.out).write_text(text)
        log(f'{len(runs)} runs -> {args.out}')
    else:
        sys.stdout.write(text)
    return 0


# --- parser


def _add_output(p: argparse.ArgumentParser) -> None:
    g = p.add_mutually_exclusive_group()
    g.add_argument('--markdown', action='store_const', const='markdown', dest='fmt')
    g.add_argument('--csv', action='store_const', const='csv', dest='fmt')
    p.set_defaults(fmt='text')


def _add_fingerprint_opts(p: argparse.ArgumentParser) -> None:
    g = p.add_argument_group('fingerprint')
    g.add_argument('--tier', choices=fp.TIERS, default='full')
    g.add_argument('--fp-seed', type=int, default=1, help='source fill seed for the matrix')


def _add_scope(p: argparse.ArgumentParser) -> None:
    p.add_argument('--all', action='store_true', help='time every pair, not the curated list')
    Filter.add_arguments(p)


def build_parser() -> argparse.ArgumentParser:
    common = argparse.ArgumentParser(add_help=False)
    common.add_argument('--results', metavar='DIR', help='run files (default measurements/)')
    common.add_argument(
        '--cache', metavar='DIR', help='builds and exports (default .pixbench-cache/)'
    )

    p = argparse.ArgumentParser(
        prog='pixbench',
        description='Build, measure and compare pixpat native libraries.',
    )
    sub = p.add_subparsers(dest='cmd', required=True, metavar='COMMAND')

    s = sub.add_parser('build', parents=[common], help='build one subject from a tree or commit')
    s.add_argument('subject', help='[NAME=]BASE[:PROFILE][,key=value...] (see README)')
    src = s.add_mutually_exclusive_group()
    src.add_argument('--tree', default='.', metavar='DIR', help='source tree (default .)')
    src.add_argument('--commit', metavar='REV', help='build this commit from a clean export')
    s.add_argument('--out', metavar='DIR', help='build here instead of the cache')
    s.add_argument('--force', action='store_true', help='rebuild even if cached')
    s.set_defaults(func=cmd_build)

    s = sub.add_parser('size', parents=[common], help='file, stripped and section sizes')
    s.add_argument('lib', nargs='+')
    s.add_argument('--json', action='store_true')
    _add_output(s)
    s.set_defaults(func=cmd_size)

    s = sub.add_parser(
        'fingerprint', parents=[common], help='digest of every output byte, or a live diff'
    )
    s.add_argument('lib', nargs='?')
    s.add_argument('--ref', metavar='LIB', help='byte-compare LIB against this reference')
    s.add_argument('--diff', nargs=2, metavar='MANIFEST', help='compare two saved manifests')
    s.add_argument('--tier', choices=fp.TIERS, default='full')
    s.add_argument('--seed', type=int, default=1, help='source fill seed')
    s.add_argument('--out', metavar='FILE', help='write the manifest here')
    Filter.add_arguments(s)
    s.set_defaults(func=cmd_fingerprint)

    s = sub.add_parser('perf', parents=[common], help='time cases through one or more libraries')
    s.add_argument('lib', nargs='+', metavar='[NAME=]LIB')
    _add_scope(s)
    PerfOpts.add_arguments(s)
    s.add_argument('--save', action='store_true', help='also write a run file')
    s.add_argument('--label')
    _add_output(s)
    s.set_defaults(func=cmd_perf)

    s = sub.add_parser(
        'measure', parents=[common], help='build (cached), size, fingerprint and time subjects'
    )
    s.add_argument('--subject', action='append', metavar='SPEC', help='what to build (repeatable)')
    s.add_argument('--lib', action='append', metavar='[NAME=]LIB', help='a built library')
    src = s.add_mutually_exclusive_group()
    src.add_argument('--tree', default='.', metavar='DIR', help='source tree (default .)')
    src.add_argument('--commit', metavar='REV', help='build this commit from a clean export')
    s.add_argument('--label')
    s.add_argument('--no-fingerprint', action='store_true')
    s.add_argument('--no-perf', action='store_true')
    s.add_argument('--force-build', action='store_true')
    _add_scope(s)
    _add_fingerprint_opts(s)
    PerfOpts.add_arguments(s)
    s.set_defaults(func=cmd_measure)

    s = sub.add_parser('history', parents=[common], help='measure subjects across commits')
    s.add_argument(
        'revs', nargs='?', metavar='REVS', help='a rev-list range, e.g. master~5..master'
    )
    s.add_argument('--commits', metavar='REV,...', help='explicit commits')
    s.add_argument('--last', type=int, metavar='N', help='the last N commits of HEAD')
    s.add_argument('--subject', action='append', required=True, metavar='SPEC')
    s.add_argument('--measure-unchanged', action='store_true', help='re-measure identical libs')
    s.add_argument('--no-fingerprint', action='store_true')
    s.add_argument('--no-perf', action='store_true')
    s.add_argument('--force-build', action='store_true')
    _add_scope(s)
    _add_fingerprint_opts(s)
    PerfOpts.add_arguments(s)
    s.set_defaults(func=cmd_history)

    s = sub.add_parser('compare', parents=[common], help='A/B two measured subjects')
    s.add_argument('a', help='RUN.json[#SUBJECT], @REV[:SUBJECT], or SUBJECT')
    s.add_argument('b')
    s.add_argument('--threshold', type=float, default=0.05, help='flag beyond this ratio')
    s.add_argument('--verbose', action='store_true', help='list every differing case')
    s.add_argument('--remeasure', action='store_true', help='re-time both libraries first')
    _add_scope(s)
    PerfOpts.add_arguments(s)
    _add_output(s)
    s.set_defaults(func=cmd_compare)

    s = sub.add_parser('report', parents=[common], help='a matrix over subjects or commits')
    s.add_argument(
        '--metric',
        default='mpx_s',
        choices=(
            'mpx_s',
            'min_ns',
            'median_ns',
            'bytes',
            'stripped_bytes',
            'alloc_bytes',
            'text_bytes',
            'rodata_bytes',
        ),
    )
    s.add_argument(
        '--by',
        choices=('subject', 'commit', 'group'),
        default='subject',
        help='rows: cases per subject, commits, or case groups (geomean per group)',
    )
    s.add_argument('--subjects', metavar='NAME,...')
    s.add_argument('revs', nargs='?', metavar='REVS', help='restrict to these commits')
    s.add_argument('--commits', metavar='REV,...')
    s.add_argument('--last', type=int, metavar='N')
    Filter.add_arguments(s)
    _add_output(s)
    s.set_defaults(func=cmd_report)

    s = sub.add_parser('export', parents=[common], help='every record, flat, for other tools')
    s.add_argument('--out', metavar='FILE')
    s.add_argument('--csv', action='store_true', help='throughput records as CSV')
    s.set_defaults(func=cmd_export)
    return p


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        return args.func(args)
    except (ValueError, FileNotFoundError, RuntimeError) as e:
        log(f'pixbench {args.cmd}: {e}')
        return 2
