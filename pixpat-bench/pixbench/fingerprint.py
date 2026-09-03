"""Correctness as a digest.

Every check of the differential matrix is a (group, label, result)
triple, the result being a return code plus the destination planes.
Hashing the results gives a library its fingerprint: two libraries (or
two commits of one) that agree on every byte have the same digest, and
the per-group and per-case digests in the manifest say where they
stopped agreeing. Running the checks on two libraries at once and
comparing results as they come gives the byte-level diff.

The matrix, in two tiers so the cost stays linear-ish in the catalog
size instead of N^2 x sizes x specs x threads:

  breadth: every (src, dst) pair once, every pattern into every sink.
           One size, one thread count, one spec per kind pair. This
           is what catches a mistranscribed layout.
  full:    breadth plus one format per I/O shape family with the full
           size x spec x thread variation, the params parser, and fat
           strides.

The error-path checks are format-independent and always included.
"""

from __future__ import annotations

import ctypes
import hashlib
import json
import pathlib
import random
import struct
from collections.abc import Callable, Iterator
from dataclasses import dataclass

from .abi import Buffer, ConvertOpts, Lib
from .cases import Case, Filter
from .formats import DEPTH, FORMATS, RANGES, RECS, kind, make_frame, run_convert, run_pattern
from .util import file_sha256, sha256_text

Result = tuple[int, list[bytes]]


@dataclass(frozen=True)
class Check:
    group: str
    label: str
    run: Callable[[Lib], Result]


GROUPS = (
    'catalog',
    'coverage',
    'convert-breadth',
    'convert-depth',
    'patterns',
    'pattern-errors',
    'pads',
    'convert-errors',
    'spec-fallback',
)
TIERS = ('full', 'breadth')

# Self-checks compare the library with itself; they hash like any other
# check but also count as failures on their own.
SELF_CHECK_GROUPS = ('coverage', 'spec-fallback')

SIZES = [(12, 4), (96, 32), (192, 64)]

# Every sink sees every catalog pattern (varying and constant fill,
# both color kinds, and the float-math one); the remaining cases only
# vary the params parser, which is format-independent, so they run on
# the depth subset alone.
#
# hbar/vbar have to be in the all-formats pass rather than the depth
# subset: they are the two rows with an RGB-native and a YUV-native
# variant, so the sink's color kind selects a different sampler.
PATTERN_CASES: list[tuple[bytes | None, bytes | None]] = [
    (b'kmstest', None),
    (b'smpte', None),
    (None, None),  # NULL pattern -> kmstest
    (b'plain', b'color=ff8040'),
    (b'checker', None),
    (b'hramp', None),
    (b'vramp', None),
    (b'dramp', None),
    (b'zoneplate', None),
    (b'hbar', b'pos=7'),
    (b'vbar', b'pos=7'),
]
PARAMS_CASES: list[tuple[bytes | None, bytes | None]] = [
    (b'plain', b'color=0x80FF8040'),
    (b'plain', b'color=123456789abc'),
    (b'plain', b'color=ff8040,'),  # trailing comma accepted
    (b'plain', b' COLOR = ff8040 ,bogus=1'),  # trim + case + unknown key
    (b'checker', b'cell=1'),
    (b'checker', b'cell=32'),  # cell wider than the smallest size
    (b'hbar', b'pos=0,width=1'),
    (b'hbar', b'pos=-6,width=8'),  # negative pos clips at the top edge
    (b'vbar', b'pos=-6,width=8'),  # ... and at the left edge
    (b'hbar', b'pos=1000'),  # bar entirely off the image
    (b'vbar', b'pos=90,width=100'),  # runs off the right edge
]
PATTERN_ERRORS: list[tuple[str, bytes, bytes | None]] = [
    ('plain-no-color', b'plain', None),
    ('plain-bad-hex', b'plain', b'color=zzzzzz'),
    ('plain-bad-len', b'plain', b'color=ff80'),
    ('malformed-params', b'kmstest', b'foo,bar'),
    ('empty-item', b'kmstest', b',a=b'),
    ('unknown-pattern', b'no_such_pattern', None),
    ('checker-zero-cell', b'checker', b'cell=0'),
    ('checker-neg-cell', b'checker', b'cell=-1'),
    ('checker-bad-cell', b'checker', b'cell=oops'),
    ('checker-empty-cell', b'checker', b'cell='),
    ('hbar-no-pos', b'hbar', None),
    ('vbar-no-pos', b'vbar', b'width=8'),
    ('hbar-bad-pos', b'hbar', b'pos=x'),
    ('vbar-zero-width', b'vbar', b'pos=0,width=0'),
    ('hbar-neg-width', b'hbar', b'pos=0,width=-4'),
    ('vbar-bad-width', b'vbar', b'pos=0,width=oops'),
]

_DEFAULT_OPTS = ConvertOpts(0, 0, 0)


def _simple_convert(
    lib: Lib,
    src_fmt='XRGB8888',
    dst_fmt='NV12',
    w=96,
    h=32,
    opts: ConvertOpts | None = _DEFAULT_OPTS,
    dst_wh=None,
) -> int:
    src = make_frame(src_fmt, w, h, fill=random.Random(1))
    dw, dh = dst_wh or (w, h)
    dst = make_frame(dst_fmt, dw, dh)
    optr = ctypes.byref(opts) if opts is not None else None
    return lib.convert(ctypes.byref(dst.buf), ctypes.byref(src.buf), optr)


def _unknown_src(lib: Lib) -> int:
    dst = make_frame('NV12', 96, 32)
    src = Buffer()
    src.format = b'NOT_A_FORMAT'
    src.width = 96
    src.height = 32
    src.num_planes = 1
    return lib.convert(ctypes.byref(dst.buf), ctypes.byref(src), None)


CONVERT_ERRORS: list[tuple[str, Callable[[Lib], int]]] = [
    ('unknown-src', _unknown_src),
    ('null-opts-ok', lambda lib: _simple_convert(lib, opts=None)),
    ('misaligned-nv12', lambda lib: _simple_convert(lib, 'XRGB8888', 'NV12', 97, 31)),
    ('odd-width-yuyv', lambda lib: _simple_convert(lib, 'XRGB8888', 'YUYV', 97, 32)),
    (
        'dims-differ',
        lambda lib: _simple_convert(lib, 'XRGB8888', 'XRGB8888', 96, 32, dst_wh=(96, 16)),
    ),
    ('neg-threads', lambda lib: _simple_convert(lib, opts=ConvertOpts(0, 0, -1))),
    ('rec-out-of-range', lambda lib: _simple_convert(lib, opts=ConvertOpts(5, 7, 1))),
    ('zero-dim', lambda lib: _simple_convert(lib, 'XRGB8888', 'XRGB8888', 0, 0)),
]


def _pattern_name(pattern: bytes | None) -> str:
    return (pattern or b'kmstest').decode('ascii')


def _catalog_names(lib: Lib) -> Result:
    return lib.format_count(), ['\n'.join(lib.formats()).encode('ascii')]


def _name_past_end(lib: Lib) -> Result:
    return (0 if lib.format_name(lib.format_count()) is None else 1), []


def _supported(fmt: str) -> Callable[[Lib], Result]:
    return lambda lib: (lib.format_supported(fmt.encode('ascii')), [])


def _coverage(src: str, dst: str, seed: int) -> Callable[[Lib], Result]:
    """Convert into a 0- and a 0xFF-prefilled destination: at a tight
    stride every byte of every plane must be written, so any difference
    means the destination is larger than the format actually occupies —
    i.e. the descriptor table (word_span / bpp / plane_sub_y) is wrong
    for dst, or the library writes short."""

    def run(lib: Lib) -> Result:
        s = (seed, src, dst, 96, 32, 0, 0)
        rc, out = run_convert(lib, src, dst, 96, 32, 0, 0, 1, s)
        if rc != 0:
            return rc, []
        _, out_ff = run_convert(lib, src, dst, 96, 32, 0, 0, 1, s, prefill=0xFF)
        return 0, [b'ok' if out == out_ff else b'differs']

    return run


def _conv(group, src, dst, w, h, rec, rng, threads, seed, pad=0) -> Check:
    s = (seed, 'pad', src, dst) if pad else (seed, src, dst, w, h, rec, rng)
    if pad:
        label = f'{src}->{dst} t={threads}'
    else:
        label = f'{src}->{dst} {w}x{h} rec={rec} range={rng} t={threads}'
    return Check(
        group,
        label,
        lambda lib: run_convert(lib, src, dst, w, h, rec, rng, threads, s, pad=pad),
    )


def _pat(pattern, params, dst, w, h, rec, rng, threads) -> Check:
    name = pattern.decode('ascii') if pattern else 'NULL'
    ptxt = params.decode('ascii') if params else '-'
    label = f'{name}:{ptxt}:{dst} {w}x{h} rec={rec} range={rng} t={threads}'
    return Check(
        'patterns',
        label,
        lambda lib: run_pattern(lib, pattern, dst, w, h, rec, rng, threads, params),
    )


def _spec_fallback(lib: Lib) -> Result:
    """Out-of-range rec/range must not only succeed, but produce the
    BT601/limited result."""
    rc_a, out_a = run_convert(lib, 'XRGB8888', 'NV12', 96, 32, 5, 7, 1, (2,))
    rc_b, out_b = run_convert(lib, 'XRGB8888', 'NV12', 96, 32, 0, 0, 1, (2,))
    ok = rc_a == rc_b == 0 and out_a == out_b
    return (0 if ok else 1), [b'ok' if ok else b'differs']


def checks(filt: Filter, tier: str = 'full', seed: int = 1) -> Iterator[Check]:
    """The matrix, narrowed by `filt`, as lazily-run checks."""
    if tier not in TIERS:
        raise ValueError(f'unknown tier {tier!r}')
    full = tier == 'full'
    fmts = filt.formats()
    formats = [f for f in FORMATS if fmts is None or f in fmts]
    depth = [f for f in DEPTH if f in formats]

    # --- introspection
    yield Check('catalog', 'names', _catalog_names)
    yield Check(
        'catalog', 'unknown-format', lambda lib: (lib.format_supported(b'NOT_A_FORMAT'), [])
    )
    yield Check('catalog', 'name-past-end', _name_past_end)
    for f in formats:
        yield Check('catalog', f'supported:{f}', _supported(f))

    # --- destination coverage depends only on the sink, once per format
    for dst in formats:
        src = 'XRGB8888' if kind(dst) == 'rgb' else 'AVUY16161616'
        yield Check('coverage', dst, _coverage(src, dst, seed))

    # --- conversions
    for src in FORMATS:
        for dst in FORMATS:
            if not filt.matches(Case('convert', src, dst)):
                continue
            cross = kind(src) != kind(dst)
            # breadth: BT709/full for cross-kind so the matrix and the
            # range scaling are both exercised; specs are swept in depth.
            brec, brng = (1, 1) if cross else (0, 0)
            yield _conv('convert-breadth', src, dst, 96, 32, brec, brng, 1, seed)
            if not full or src not in depth or dst not in depth:
                continue
            specs = [(r, g) for r in RECS for g in RANGES] if cross else [(0, 0)]
            for w, h in SIZES:
                for rec, rng in specs:
                    for threads in (1, 4, 16):
                        if (w, h, rec, rng, threads) == (96, 32, brec, brng, 1):
                            continue  # already covered by the breadth pass
                        yield _conv('convert-depth', src, dst, w, h, rec, rng, threads, seed)

    # --- patterns
    for pattern, params in PATTERN_CASES:
        for dst in FORMATS:
            if filt.matches(Case('pattern', _pattern_name(pattern), dst)):
                yield _pat(pattern, params, dst, 96, 32, 0, 0, 1)
    if full:
        for pattern, params in PATTERN_CASES + PARAMS_CASES:
            for dst in depth:
                if not filt.matches(Case('pattern', _pattern_name(pattern), dst)):
                    continue
                for w, h in SIZES:
                    for rec, rng, threads in ((0, 0, 1), (1, 1, 4)):
                        yield _pat(pattern, params, dst, w, h, rec, rng, threads)

    for label, pattern, params in PATTERN_ERRORS:
        yield Check(
            'pattern-errors',
            label,
            lambda lib, p=pattern, q=params: (
                run_pattern(lib, p, 'XRGB8888', 96, 32, 0, 0, 1, q)[0],
                [],
            ),
        )

    # --- fat strides (tight + 16), one size, single + multi thread
    if full:
        for src in FORMATS:
            for dst in ('XRGB8888', 'NV12', 'BGR888'):
                if not filt.matches(Case('convert', src, dst)):
                    continue
                for threads in (1, 4):
                    yield _conv('pads', src, dst, 96, 32, 0, 0, threads, seed, pad=16)

    # --- error-path parity
    for label, fn in CONVERT_ERRORS:
        yield Check('convert-errors', label, lambda lib, fn=fn: (fn(lib), []))

    yield Check(
        'spec-fallback',
        'out',
        lambda lib: run_convert(lib, 'XRGB8888', 'NV12', 96, 32, 5, 7, 1, (2,)),
    )
    yield Check('spec-fallback', 'matches-bt601', _spec_fallback)


def hash_result(res: Result) -> str:
    rc, planes = res
    h = hashlib.sha256()
    h.update(struct.pack('<q', rc))
    for p in planes:
        h.update(struct.pack('<Q', len(p)))
        h.update(p)
    return h.hexdigest()


def _digest(lines: Iterator[str]) -> str:
    return sha256_text('\n'.join(lines))


def fingerprint(lib: Lib, filt: Filter, tier: str = 'full', seed: int = 1) -> dict:
    """The manifest: one hash per check, one per group, one overall."""
    groups: dict[str, dict[str, str]] = {}
    self_check_failures: list[str] = []
    for chk in checks(filt, tier, seed):
        res = chk.run(lib)
        groups.setdefault(chk.group, {})[chk.label] = hash_result(res)
        if chk.group in SELF_CHECK_GROUPS and res[1] == [b'differs']:
            self_check_failures.append(f'{chk.group}/{chk.label}')
    group_digests = {
        g: _digest(f'{label} {sha}' for label, sha in sorted(labels.items()))
        for g, labels in groups.items()
    }
    digest = _digest(f'{g} {d}' for g, d in sorted(group_digests.items()))
    catalog = lib.formats()
    warnings = [f'{n}: not in the format table, untested' for n in catalog if n not in FORMATS]
    return {
        'schema': 1,
        'lib': {'path': str(lib.path), 'sha256': file_sha256(lib.path)},
        'tier': tier,
        'filter': filt.describe(),
        'seed': seed,
        'catalog': catalog,
        'cases': sum(len(v) for v in groups.values()),
        'digest': digest,
        'group_digests': group_digests,
        'self_check_failures': self_check_failures,
        'warnings': warnings,
        'groups': groups,
    }


def manifest_name(m: dict) -> str:
    variant = sha256_text(f'{m["tier"]}|{m["filter"]}|{m["seed"]}')[:8]
    return f'{m["lib"]["sha256"][:16]}-{variant}.fp.json'


def write_manifest(m: dict, directory: pathlib.Path) -> pathlib.Path:
    directory.mkdir(parents=True, exist_ok=True)
    path = directory / manifest_name(m)
    path.write_text(json.dumps(m, indent=1) + '\n')
    return path


def load_manifest(path: str | pathlib.Path) -> dict:
    return json.loads(pathlib.Path(path).read_text())


def summary(m: dict, manifest_path: pathlib.Path | None) -> dict:
    """What a run file records about a fingerprint."""
    return {
        'tier': m['tier'],
        'filter': m['filter'],
        'seed': m['seed'],
        'digest': m['digest'],
        'groups': m['group_digests'],
        'cases': m['cases'],
        'self_check_failures': m['self_check_failures'],
        'warnings': m['warnings'],
        'manifest': str(manifest_path) if manifest_path else None,
    }


def comparable(a: dict, b: dict) -> bool:
    """Two fingerprints/summaries cover the same checks."""
    return all(a.get(k) == b.get(k) for k in ('tier', 'filter', 'seed'))


def diff_manifests(a: dict, b: dict) -> list[tuple[str, str]]:
    """[(group/label, 'differs' | 'only in A' | 'only in B')]."""
    out = []
    for g in sorted(set(a['groups']) | set(b['groups'])):
        ga, gb = a['groups'].get(g, {}), b['groups'].get(g, {})
        for label in sorted(set(ga) | set(gb)):
            if label not in ga:
                out.append((f'{g}/{label}', 'only in B'))
            elif label not in gb:
                out.append((f'{g}/{label}', 'only in A'))
            elif ga[label] != gb[label]:
                out.append((f'{g}/{label}', 'differs'))
    return out


def describe_diff(a: Result, b: Result) -> str:
    if a[0] != b[0]:
        return f'rc ref={a[0]} test={b[0]}'
    for pi, (x, y) in enumerate(zip(a[1], b[1])):
        if x == y:
            continue
        if len(x) != len(y):
            return f'plane {pi} length ref={len(x)} test={len(y)}'
        off = next(i for i in range(len(x)) if x[i] != y[i])
        return f'plane {pi} first diff at {off}: ref={x[off]:#04x} test={y[off]:#04x}'
    if len(a[1]) != len(b[1]):
        return f'plane count ref={len(a[1])} test={len(b[1])}'
    return 'differs'


def diff_libs(
    ref: Lib, tst: Lib, filt: Filter, tier: str = 'full', seed: int = 1
) -> tuple[int, list[str]]:
    """Run the matrix through both libraries, byte-comparing every
    result. Returns (checks run, failure descriptions)."""
    failures: list[str] = []
    n = 0

    def check(label: str, ok: bool, detail: str = '') -> None:
        nonlocal n
        n += 1
        if not ok:
            failures.append(f'{label} {detail}'.rstrip())

    # Introspection parity: every test-lib format must exist in the
    # reference library (the test lib may be a subset build), and where
    # both enable the same set, the enumeration order must match too —
    # pixpat_format_name(idx) is part of the ABI. A format the reference
    # knows but the table does not describe must be unsupported in the
    # test library rather than accepted half-implemented.
    tst_names, ref_names = tst.formats(), ref.formats()
    check('format_count>0', len(tst_names) > 0)
    if len(tst_names) == len(ref_names):
        check('catalog-order', tst_names == ref_names, f'{tst_names} != {ref_names}')
    for name in tst_names:
        check(f'ref-supports:{name}', ref.supports(name))
    for name in ref_names:
        if name not in FORMATS:
            check(f'subset:fmt:{name}', not tst.supports(name), 'not in the format table')

    for chk in checks(filt, tier, seed):
        a, b = chk.run(ref), chk.run(tst)
        check(f'{chk.group}:{chk.label}', a == b, '' if a == b else describe_diff(a, b))
    return n, failures
