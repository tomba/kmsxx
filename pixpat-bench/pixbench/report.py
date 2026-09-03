"""Reading the run files back: A/B comparison, matrices over commits
or subjects, and the flat export."""

from __future__ import annotations

import csv
import io
import json
import pathlib
from collections.abc import Sequence

from . import fingerprint as fp
from .cases import Filter, curated, group_order, groups, parse_case
from .formats import family
from .perf import record_key
from .size import HEADLINE
from .store import commit_of, short_commit, subject_label
from .util import CommandError, geomean, git

SIZE_METRICS = ('bytes', 'stripped_bytes', 'alloc_bytes', 'text_bytes', 'rodata_bytes')


# --- tables


def render(headers: Sequence[str], rows: Sequence[Sequence], fmt: str = 'text') -> str:
    cells = [[('' if c is None else str(c)) for c in r] for r in rows]
    if fmt == 'csv':
        out = io.StringIO()
        w = csv.writer(out)
        w.writerow(headers)
        w.writerows(cells)
        return out.getvalue().rstrip('\n')
    if fmt == 'markdown':
        lines = ['| ' + ' | '.join(headers) + ' |', '|' + '|'.join('---' for _ in headers) + '|']
        lines += ['| ' + ' | '.join(r) + ' |' for r in cells]
        return '\n'.join(lines)
    widths = [len(h) for h in headers]
    for r in cells:
        for i, c in enumerate(r):
            widths[i] = max(widths[i], len(c))
    numeric = [
        all(_is_num(r[i]) for r in cells if i < len(r) and r[i] != '') for i in range(len(headers))
    ]

    def line(r):
        return '  '.join(
            (c.rjust(widths[i]) if numeric[i] else c.ljust(widths[i])) for i, c in enumerate(r)
        ).rstrip()

    return '\n'.join(
        [line(list(headers)), line(['-' * w for w in widths])] + [line(r) for r in cells]
    )


def _is_num(s: str) -> bool:
    try:
        float(s.rstrip('%x').replace('+', ''))
        return True
    except ValueError:
        return False


def _fmt(v, digits=0) -> str:
    if v is None:
        return ''
    if isinstance(v, float):
        return f'{v:.{digits}f}'
    return str(v)


def _pct(a, b) -> str:
    if not a or b is None:
        return ''
    return f'{(b - a) / a * 100:+.1f}%'


# --- subjects


def size_metric(subject: dict, metric: str):
    lib = subject.get('lib') or {}
    if metric == 'text_bytes':
        return (lib.get('sections') or {}).get('.text')
    if metric == 'rodata_bytes':
        return (lib.get('sections') or {}).get('.rodata')
    return lib.get(metric)


def describe_subject(name: str, s: dict) -> str:
    b = s.get('build') or {}
    tc = b.get('cc') or s.get('lang')
    versions = b.get('toolchain_versions') or {}
    ver = versions.get('cxx') or ''
    bits = [subject_label(name, s), tc]
    if b.get('profile'):
        bits.append(pathlib.Path(b['profile']).stem)
    for k in ('arch', 'lto', 'opt', 'buildtype'):
        if b.get(k) is not None:
            bits.append(f'{k}={b[k]}')
    return ' '.join(str(x) for x in bits if x) + (f'  [{ver}]' if ver else '')


def perf_of(run: dict, name: str) -> dict[tuple, dict]:
    return {record_key(r): r for r in run.get('perf', []) if r.get('subject') == name}


# --- groups


def group_rows(pairs: Sequence[tuple[str, float]], threshold: float) -> list[list]:
    """Per-group geomean of (case name, B/A ratio) pairs: one row per
    group with at least two cases — group, cases, geomean, min, max,
    flag. Singletons are already visible in the per-case table."""
    by: dict[str, list[float]] = {}
    for case_name, ratio in pairs:
        for g in groups(parse_case(case_name)):
            by.setdefault(g, []).append(ratio)
    rows = []
    for g in sorted(by, key=group_order):
        rs = by[g]
        if len(rs) < 2:
            continue
        gm = geomean(rs)
        flag = 'slower' if gm < 1 - threshold else 'faster' if gm > 1 + threshold else ''
        rows.append([g, len(rs), f'{gm:.3f}x', f'{min(rs):.3f}x', f'{max(rs):.3f}x', flag])
    return rows


GROUP_HEADERS = ['group', 'cases', 'geomean', 'min', 'max', 'flag']


# --- compare


def compare(
    a: tuple[dict, str],
    b: tuple[dict, str],
    *,
    filt: Filter,
    threshold: float = 0.05,
    verbose: bool = False,
    fmt: str = 'text',
) -> tuple[str, bool]:
    """Returns (report text, everything-fine)."""
    (run_a, name_a), (run_b, name_b) = a, b
    sa, sb = run_a['subjects'][name_a], run_b['subjects'][name_b]
    out: list[str] = []
    ok = True
    out.append(f'A: {describe_subject(name_a, sa)}')
    out.append(f'B: {describe_subject(name_b, sb)}')
    for side, run in (('A', run_a), ('B', run_b)):
        if run.get('_borrowed'):
            out.append(
                f'   {side} was not measured in its run; using the identical library '
                f'measured as {run["_borrowed"]}'
            )
    out.append('')

    # correctness
    fa, fb = run_a.get('fingerprint', {}).get(name_a), run_b.get('fingerprint', {}).get(name_b)
    if fa and fb:
        if not fp.comparable(fa, fb):
            va = f'{fa["tier"]}/{fa["filter"]}/{fa["seed"]}'
            vb = f'{fb["tier"]}/{fb["filter"]}/{fb["seed"]}'
            out.append(f'correctness: fingerprints not comparable (A: {va}, B: {vb})')
        elif fa['digest'] == fb['digest']:
            out.append(f'correctness: identical ({fa["cases"]} cases, digest {fa["digest"][:16]})')
        else:
            ok = False
            bad = [g for g in fa['groups'] if fa['groups'].get(g) != fb['groups'].get(g)]
            out.append(f'correctness: DIFFERS in {", ".join(bad)}')
            ma = fa.get('manifest') and pathlib.Path(fa['manifest'])
            mb = fb.get('manifest') and pathlib.Path(fb['manifest'])
            if ma and mb and ma.is_file() and mb.is_file():
                diffs = fp.diff_manifests(fp.load_manifest(ma), fp.load_manifest(mb))
                limit = None if verbose else 20
                for label, what in diffs[:limit]:
                    out.append(f'  {label}: {what}')
                if limit and len(diffs) > limit:
                    out.append(f'  ... {len(diffs) - limit} more (--verbose)')
        for name, f in ((name_a, fa), (name_b, fb)):
            if f.get('self_check_failures'):
                ok = False
                out.append(f'  {name}: self-check failures: {", ".join(f["self_check_failures"])}')
    else:
        out.append('correctness: not measured on both sides')
    out.append('')

    # size
    rows = []
    for metric in ('bytes', 'stripped_bytes', 'alloc_bytes'):
        va, vb = size_metric(sa, metric), size_metric(sb, metric)
        if va is not None or vb is not None:
            rows.append([metric, va, vb, _pct(va, vb)])
    seca = (sa.get('lib') or {}).get('sections') or {}
    secb = (sb.get('lib') or {}).get('sections') or {}
    for sec in HEADLINE:
        if sec in seca or sec in secb:
            rows.append([sec, seca.get(sec), secb.get(sec), _pct(seca.get(sec), secb.get(sec))])
    if rows:
        out.append(render(['size', 'A', 'B', 'B vs A'], rows, fmt))
        out.append('')

    # perf
    pa, pb = perf_of(run_a, name_a), perf_of(run_b, name_b)
    keys = [k for k in pa if k in pb and filt.matches(parse_case(k[0]))]
    uniform = len({k[1:] for k in keys}) <= 1
    rows = []
    ratios = []
    pairs: list[tuple[str, float]] = []
    worse = better = 0
    for k in keys:
        ra, rb = pa[k], pb[k]
        ratio = rb['mpx_s'] / ra['mpx_s'] if ra['mpx_s'] else None
        flag = ''
        if ratio is not None:
            ratios.append(ratio)
            pairs.append((k[0], ratio))
            if ratio < 1 - threshold:
                flag, worse = 'slower', worse + 1
            elif ratio > 1 + threshold:
                flag, better = 'faster', better + 1
        cond = '' if uniform else f'{k[1]}x{k[2]} t={k[3]} {k[4]}/{k[5]}'
        row = (
            [k[0]]
            + ([cond] if not uniform else [])
            + [
                _fmt(ra['mpx_s']),
                _fmt(rb['mpx_s']),
                _fmt(ratio, 3) + 'x' if ratio is not None else '',
                flag,
            ]
        )
        rows.append(row)
    if rows:
        cond_hdr = [] if uniform else ['conditions']
        out.append(render(['case', *cond_hdr, 'A Mpx/s', 'B Mpx/s', 'B/A', 'flag'], rows, fmt))
        grouped = group_rows(pairs, threshold)
        if len(grouped) > 1:
            out.append('')
            out.append(render(GROUP_HEADERS, grouped, fmt))
        gm = geomean(ratios)
        if uniform and keys:
            k = keys[0]
            out.append(f'conditions: {k[1]}x{k[2]} threads={k[3]} {k[4]}/{k[5]}')
        out.append(
            f'geomean B/A: {gm:.3f}x over {len(ratios)} cases; '
            f'{worse} slower, {better} faster beyond {threshold:.0%}'
            if gm
            else 'no comparable throughput records'
        )
    else:
        out.append('perf: no cases measured on both sides')
    only_a = [k[0] for k in pa if k not in pb]
    only_b = [k[0] for k in pb if k not in pa]
    if only_a:
        out.append(f'only in A: {", ".join(sorted(set(only_a)))}')
    if only_b:
        out.append(f'only in B: {", ".join(sorted(set(only_b)))}')
    return '\n'.join(out), ok


# --- report matrices


def _commit_order(repo: pathlib.Path, shas: list[str]) -> list[str]:
    """Oldest first by commit date, falling back to the given order."""
    dated = []
    for s in shas:
        try:
            dated.append((int(git(repo, 'show', '-s', '--format=%ct', s)), s))
        except CommandError:
            dated.append((0, s))
    return [s for _, s in sorted(dated)]


def collect(runs: list[dict], metric: str, filt: Filter) -> dict[tuple[str, str], dict]:
    """(subject key, column) -> {'value', 'commit', 'name', 'subject', 'lib'}
    with the latest record winning. For throughput the column is the
    case name (conditions folded into the value's record); for sizes
    it is the metric name."""
    out: dict[tuple[str, str], dict] = {}
    by_sha: dict[str, dict[str, dict]] = {}
    unmeasured: list[tuple[str, dict, str]] = []
    for run in runs:  # chronological, so later overwrite
        for name, s in run['subjects'].items():
            skey = f'{name}@{short_commit(s)}'
            base = {'commit': commit_of(s), 'name': name, 'subject': s}
            sha = (s.get('lib') or {}).get('sha256')
            if metric in SIZE_METRICS:
                v = size_metric(s, metric)
                if v is not None:
                    out[(skey, metric)] = {**base, 'value': v}
                continue
            found = False
            for r in run.get('perf', []):
                if r.get('subject') != name or not filt.matches(parse_case(r['case'])):
                    continue
                entry = {**base, 'value': r[metric], 'record': r}
                out[(skey, r['case'])] = entry
                if sha:
                    by_sha.setdefault(sha, {})[r['case']] = entry
                found = True
            if not found and sha:
                unmeasured.append((skey, base, sha))
    # A subject that was never timed but is byte-identical to one that
    # was (history does not re-measure those) shows that one's numbers.
    for skey, base, sha in unmeasured:
        for col, entry in by_sha.get(sha, {}).items():
            out.setdefault(
                (skey, col),
                {**base, 'value': entry['value'], 'record': entry.get('record'), 'carried': True},
            )
    return out


def borrow(run: dict, name: str, runs: list[dict]) -> dict:
    """`run` with the perf records and fingerprint of `name` filled in
    from any run that measured a byte-identical library, when `run`
    itself has none (history skips re-measuring unchanged builds)."""
    if perf_of(run, name) or name in run.get('fingerprint', {}):
        return run
    sha = (run['subjects'][name].get('lib') or {}).get('sha256')
    if not sha:
        return run
    for other in reversed(runs):
        for oname, s in other['subjects'].items():
            if (s.get('lib') or {}).get('sha256') != sha or not perf_of(other, oname):
                continue
            merged = dict(run)
            merged['perf'] = [
                dict(r, subject=name) for r in other.get('perf', []) if r.get('subject') == oname
            ]
            merged['fingerprint'] = dict(run.get('fingerprint', {}))
            if oname in other.get('fingerprint', {}):
                merged['fingerprint'][name] = other['fingerprint'][oname]
            merged['_borrowed'] = f'{oname} in {other.get("_path")}'
            return merged
    return run


def report(
    runs: list[dict],
    *,
    metric: str,
    by: str,
    filt: Filter,
    subjects: set[str] | None,
    commits: set[str] | None,
    fmt: str,
    repo: pathlib.Path,
) -> str:
    data = collect(runs, metric, filt)
    if subjects:
        data = {k: v for k, v in data.items() if v['name'] in subjects}
    if commits is not None:
        data = {k: v for k, v in data.items() if v['commit'] and v['commit'] in commits}
    if not data:
        return 'nothing to report'
    cases_order = {c.name: i for i, c in enumerate(curated())}

    def col_order(c: str) -> tuple:
        return (cases_order.get(c, len(cases_order)), c)

    if by == 'group':
        # Rows are groups, columns builds, a cell the geomean of the
        # metric over the group's cases — restricted to the cases every
        # column has, so that the ratio of two cells is the geomean of
        # the per-case ratios compare would print for those two builds.
        if metric in SIZE_METRICS:
            raise CommandError('--by group is for throughput metrics')
        cols = sorted({k[0] for k in data})
        common = set.intersection(*[{k[1] for k in data if k[0] == c} for c in cols])
        by_group: dict[str, dict[str, list[float]]] = {}
        for (c, case_name), v in data.items():
            if case_name in common:
                for g in groups(parse_case(case_name)):
                    by_group.setdefault(g, {}).setdefault(c, []).append(v['value'])
        rows = []
        for g in sorted(by_group, key=group_order):
            n = len(by_group[g][cols[0]])
            if n >= 2:
                rows.append([g, n, *[_fmt(geomean(by_group[g][c])) for c in cols]])
        text = render(['group', 'cases', *cols], rows, fmt)
        dropped = len({k[1] for k in data}) - len(common)
        if dropped:
            text += f'\n({dropped} cases not measured for every column left out)'
        return text

    if by == 'subject':
        cols = sorted({k[0] for k in data})
        rows_keys = sorted({k[1] for k in data}, key=col_order)
        headers = [metric if metric in SIZE_METRICS else 'case', *cols]
        rows = [[rk, *[_fmt(data.get((c, rk), {}).get('value')) for c in cols]] for rk in rows_keys]
        return render(headers, rows, fmt)

    # by commit: rows are commits (oldest first), columns are
    # subject-name x column; a subject name is dropped from the header
    # when only one is present.
    shas = sorted({v['commit'] for v in data.values() if v['commit']})
    order = {s: i for i, s in enumerate(_commit_order(repo, shas))}
    names = sorted({v['name'] for v in data.values()})
    cols = sorted(
        {(v['name'], k[1]) for k, v in data.items()}, key=lambda c: (c[0], col_order(c[1]))
    )
    by_row: dict[str, dict] = {}
    for (skey, col), v in data.items():
        row_id = v['commit'] or skey.partition('@')[2]
        by_row.setdefault(row_id, {})[(v['name'], col)] = v['value']
    row_ids = sorted(by_row, key=lambda r: (order.get(r, len(order)), r))
    headers = ['commit', *[(c[1] if len(names) == 1 else f'{c[0]}:{c[1]}') for c in cols]]
    rows = [[r[:10], *[_fmt(by_row[r].get(c)) for c in cols]] for r in row_ids]
    return render(headers, rows, fmt)


# --- export


def flatten(runs: list[dict]) -> list[dict]:
    """One record per measurement, context inlined."""
    out: list[dict] = []
    for run in runs:
        rmeta = run['run']
        ctx_run = {
            'run_id': rmeta.get('id'),
            'time': rmeta.get('time'),
            'label': rmeta.get('label'),
            'tool_commit': (rmeta.get('tool') or {}).get('commit'),
            'hostname': (rmeta.get('machine') or {}).get('hostname'),
            'cpu_model': (rmeta.get('machine') or {}).get('cpu_model'),
            'cpu_pinned': (rmeta.get('machine') or {}).get('cpu_pinned'),
        }
        for name, s in run['subjects'].items():
            b = s.get('build') or {}
            src = s.get('source') or {}
            lib = s.get('lib') or {}
            ctx = {
                **ctx_run,
                'subject': name,
                'lang': s.get('lang'),
                'commit': src.get('commit'),
                'dirty': src.get('dirty'),
                'describe': src.get('describe'),
                'cc': b.get('cc'),
                'cxx': b.get('cxx'),
                'profile': b.get('profile'),
                'arch': b.get('arch'),
                'lto': b.get('lto'),
                'opt': b.get('opt'),
                'buildtype': b.get('buildtype'),
                'toolchain': ' / '.join((b.get('toolchain_versions') or {}).values()),
                'config_hash': b.get('config_hash'),
                'lib_sha256': lib.get('sha256'),
            }
            if lib.get('bytes') is not None:
                out.append(
                    {
                        'kind': 'size',
                        **ctx,
                        'bytes': lib.get('bytes'),
                        'stripped_bytes': lib.get('stripped_bytes'),
                        'alloc_bytes': lib.get('alloc_bytes'),
                        'text_bytes': (lib.get('sections') or {}).get('.text'),
                        'rodata_bytes': (lib.get('sections') or {}).get('.rodata'),
                        'sections': lib.get('sections'),
                    }
                )
            f = run.get('fingerprint', {}).get(name)
            if f:
                out.append(
                    {
                        'kind': 'fingerprint',
                        **ctx,
                        'digest': f['digest'],
                        'tier': f['tier'],
                        'filter': f['filter'],
                        'seed': f['seed'],
                        'cases': f['cases'],
                        'self_check_failures': f.get('self_check_failures'),
                        'groups': f['groups'],
                    }
                )
            for r in run.get('perf', []):
                if r.get('subject') == name:
                    fields = {k: v for k, v in r.items() if k not in ('subject', 'kind')}
                    case = parse_case(r['case'])
                    fam = {
                        'src_family': family(case.src) if case.kind == 'convert' else None,
                        'dst_family': family(case.dst),
                        'case_groups': list(groups(case)),
                    }
                    out.append({'kind': 'perf', **ctx, 'case_kind': r.get('kind'), **fields, **fam})
    return out


PERF_CSV_COLUMNS = [
    'run_id', 'time', 'hostname', 'cpu_model', 'cpu_pinned', 'subject', 'lang', 'commit', 'dirty',
    'cc', 'cxx', 'profile', 'arch', 'lto', 'opt', 'buildtype', 'toolchain', 'lib_sha256',
    'case', 'case_kind', 'src_family', 'dst_family',
    'w', 'h', 'threads', 'rec', 'range', 'iters', 'warmup', 'rounds',
    'samples', 'min_ns', 'median_ns', 'mpx_s',
]  # fmt: skip


def export_text(runs: list[dict], fmt: str) -> str:
    records = flatten(runs)
    if fmt == 'csv':
        out = io.StringIO()
        w = csv.DictWriter(out, fieldnames=PERF_CSV_COLUMNS, extrasaction='ignore')
        w.writeheader()
        w.writerows(r for r in records if r['kind'] == 'perf')
        return out.getvalue()
    return json.dumps(records, indent=1) + '\n'
