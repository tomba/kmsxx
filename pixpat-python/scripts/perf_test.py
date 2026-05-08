#!/usr/bin/env python3
"""Micro-benchmark pixpat across draw_pattern and convert call paths.

Each row times one ``<source> -> <destination>`` operation:

* a pattern draw, where ``source`` is a pattern name (``smpte``,
  ``kmstest`` etc.) and the row times ``pixpat.draw_pattern``;
* a format conversion, where both sides are format names and the row
  times ``pixpat.convert``.

Buffer allocation is driven by ``pixutils.formats.PixelFormats`` —
``framesize`` / ``planesize`` / ``stride`` give us per-plane geometry
without per-format hand-coding.

Filtering is uniform across both kinds because the case name always
has the form ``"<lhs> -> <rhs>"``: ``--ionly kmstest`` keeps that
pattern's cases, ``--ionly NV12`` keeps NV12-source convert cases,
``--oonly BGR888`` keeps any case (pattern or convert) writing
BGR888, and so on.

Usage:
    python perf_test.py [--width 1920] [--height 1080] [--iters 200]
                        [--warmup 10] [--pp-threads 1]
                        [--rec BT709] [--range LIMITED]
                        [--only-pattern | --only-convert]
                        [--only STR] [--ionly STR] [--oonly STR]
                        [--cases STR] [--tsv]
"""

from __future__ import annotations

import argparse
import gc
import math
import sys
import time
from dataclasses import dataclass
from typing import Callable, Optional

import numpy as np

import pixpat
from pixutils.formats import PixelFormat, PixelFormats


PATTERNS = ['kmstest', 'smpte', 'plain']

PATTERN_SINK_FMTS = [
    'XRGB8888',
    'BGR888',
    'RGB565',
    'RGBA1010102',
    'ABGR16161616',
    'Y8',
    'YUYV',
    'UYVY',
    'NV12',
    'NV16',
    'YUV420',
    'SRGGB8',
    'SRGGB10P',
]

CONVERT_PAIRS: list[tuple[str, str]] = [
    # RGB shuffles
    ('RGB888', 'BGR888'),
    ('BGR888', 'BGRA8888'),
    ('BGRA8888', 'BGR888'),
    # Grayscale
    ('BGR888', 'Y8'),
    ('Y8', 'BGR888'),
    # Packed YUV decode
    ('YUYV', 'BGR888'),
    ('UYVY', 'BGR888'),
    # Semiplanar YUV decode
    ('NV12', 'BGR888'),
    ('NV21', 'BGR888'),
    # Planar YUV decode
    ('YUV420', 'BGR888'),
    ('YVU420', 'BGR888'),
    # Planar YUV encode
    ('BGR888', 'YUV420'),
    ('BGR888', 'YVU420'),
    # NV12 encode/decode against XRGB8888
    ('XRGB8888', 'NV12'),
    ('BGR888', 'NV12'),
    ('NV12', 'XRGB8888'),
    # Cold path: neither side is BGR888
    ('XRGB8888', 'RGBA8888'),
    ('RGBA8888', 'XRGB8888'),
    ('NV12', 'YUV420'),
    ('YUV420', 'NV12'),
    ('NV12', 'YUYV'),
    ('NV12', 'RGB565'),
    ('Y8', 'NV12'),
    # Wider coverage
    ('BGR888', 'YUYV'),
    ('BGR888', 'NV16'),
    ('NV16', 'BGR888'),
    ('BGR888', 'YUV422'),
    ('YUV422', 'BGR888'),
    ('BGR888', 'YUV444'),
    ('YUV444', 'BGR888'),
    # 16-bit packed RGB
    ('BGR888', 'RGB565'),
    ('RGB565', 'BGR888'),
    # 10-bit packed RGB
    ('BGR888', 'RGBA1010102'),
    ('RGBA1010102', 'BGR888'),
    # 64-bit packed RGB (pixpat's normalized wide form)
    ('BGR888', 'ABGR16161616'),
    ('ABGR16161616', 'BGR888'),
    # Multi-pixel-per-word semiplanar YUV
    ('BGR888', 'P030'),
    ('P030', 'BGR888'),
    ('BGR888', 'P230'),
    # Multi-pixel-per-word planar YUV
    ('BGR888', 'T430'),
    ('T430', 'BGR888'),
    # Multi-pixel-per-word grayscale
    ('BGR888', 'XYYY2101010'),
    # Single-pixel-per-word YUV
    ('BGR888', 'XVUY2101010'),
    ('BGR888', 'AVUY16161616'),
    # Bayer
    ('BGR888', 'SRGGB8'),
    ('SRGGB8', 'BGR888'),
    ('BGR888', 'SRGGB10P'),
    ('SRGGB10P', 'BGR888'),
    ('BGR888', 'SRGGB12P'),
    ('SRGGB12P', 'BGR888'),
    # Single-channel RGB
    ('BGR888', 'R8'),
    ('R8', 'BGR888'),
    # MIPI CSI-2 packed grayscale
    ('BGR888', 'Y10P'),
    ('Y10P', 'BGR888'),
    # 4:2:2 packed YUV in 64-bit words
    ('BGR888', 'Y210'),
    ('Y210', 'BGR888'),
]


_PF_BY_NAME = {v.name: v for v in PixelFormats.__dict__.values() if isinstance(v, PixelFormat)}


def _required_align(pf: PixelFormat) -> tuple[int, int]:
    # pf.pixel_align doesn't always capture the per-plane pixels_per_block /
    # vsub requirements (e.g. T430 lists (1,1) but each plane is 3 px wide),
    # so combine them to get the effective alignment we need to skip cleanly.
    w_align = pf.pixel_align[0]
    h_align = pf.pixel_align[1]
    for p in pf.planes:
        w_align = math.lcm(w_align, p.pixels_per_block * p.hsub)
        h_align = math.lcm(h_align, p.vsub)
    return w_align, h_align


def _alloc_buffer(fmt_name: str, w: int, h: int) -> Optional[tuple[pixpat.Buffer, np.ndarray]]:
    """Build a pixpat.Buffer + its 1-D backing array, or None when the
    resolution doesn't fit the format's alignment."""
    pf = _PF_BY_NAME[fmt_name]
    w_align, h_align = _required_align(pf)
    if w % w_align or h % h_align:
        return None

    backing = np.zeros(pf.framesize(w, h), dtype=np.uint8)
    planes: list[np.ndarray] = []
    strides: list[int] = []
    offset = 0
    for i in range(len(pf.planes)):
        s = pf.stride(w, i)
        psize = pf.planesize(s, h, i)
        view = backing[offset : offset + psize].reshape(psize // s, s)
        planes.append(view)
        strides.append(s)
        offset += psize
    return pixpat.Buffer(planes, fmt_name, w, h, strides), backing


@dataclass
class Case:
    name: str
    kind: str  # 'pattern' or 'convert'
    dst: pixpat.Buffer
    src: Optional[pixpat.Buffer] = None
    src_arr: Optional[np.ndarray] = None  # 1-D backing for random fill


def _build_cases(w: int, h: int, kinds: set[str]) -> list[Case]:
    cases: list[Case] = []

    if 'pattern' in kinds:
        for fmt in PATTERN_SINK_FMTS:
            alloc = _alloc_buffer(fmt, w, h)
            if alloc is None:
                continue
            buf, _ = alloc
            for pat in PATTERNS:
                cases.append(Case(name=f'{pat} -> {fmt}', kind='pattern', dst=buf))

    if 'convert' in kinds:
        for src_fmt, dst_fmt in CONVERT_PAIRS:
            src_alloc = _alloc_buffer(src_fmt, w, h)
            dst_alloc = _alloc_buffer(dst_fmt, w, h)
            if src_alloc is None or dst_alloc is None:
                continue
            src_buf, src_backing = src_alloc
            dst_buf, _ = dst_alloc
            cases.append(
                Case(
                    name=f'{src_fmt} -> {dst_fmt}',
                    kind='convert',
                    dst=dst_buf,
                    src=src_buf,
                    src_arr=src_backing,
                )
            )

    return cases


def _parse_filter(s: Optional[str]) -> Optional[set[str]]:
    if s is None:
        return None
    return {x.strip().upper() for x in s.split(',') if x.strip()}


def _norm_case(name: str) -> str:
    return ' '.join(name.upper().split())


def _filter_cases(
    cases: list[Case],
    only: Optional[str],
    ionly: Optional[str],
    oonly: Optional[str],
    cases_filter: Optional[str],
) -> list[Case]:
    only_set = _parse_filter(only)
    ionly_set = _parse_filter(ionly)
    oonly_set = _parse_filter(oonly)
    cases_set: Optional[set[str]] = None
    if cases_filter is not None:
        cases_set = {_norm_case(x) for x in cases_filter.split(',') if x.strip()}
    if only_set is None and ionly_set is None and oonly_set is None and cases_set is None:
        return cases

    out: list[Case] = []
    for c in cases:
        lhs, _, rhs = c.name.partition(' -> ')
        lhs, rhs = lhs.upper(), rhs.upper()
        if cases_set is not None and _norm_case(c.name) not in cases_set:
            continue
        if only_set is not None and lhs not in only_set and rhs not in only_set:
            continue
        if ionly_set is not None and lhs not in ionly_set:
            continue
        if oonly_set is not None and rhs not in oonly_set:
            continue
        out.append(c)
    return out


def _bind(
    case: Case,
    num_threads: int,
    rec: 'pixpat.Rec',
    color_range: 'pixpat.Range',
) -> Callable[[], object]:
    if case.kind == 'pattern':
        fn = pixpat.draw_pattern
        dst = case.dst
        pat = case.name.partition(' -> ')[0]
        # `plain` needs an explicit color; the rest ignore params.
        params = {'color': 'ff0000'} if pat == 'plain' else None
        return lambda: fn(
            dst,
            pat,
            rec=rec,
            color_range=color_range,
            num_threads=num_threads,
            params=params,
        )
    fn = pixpat.convert
    assert case.src is not None
    src, dst = case.src, case.dst
    return lambda: fn(dst, src, rec=rec, color_range=color_range, num_threads=num_threads)


def _time_n(fn: Callable[[], object], iters: int) -> float:
    """Return min seconds over ``iters`` calls."""
    best = float('inf')
    for _ in range(iters):
        t0 = time.perf_counter_ns()
        fn()
        dt = time.perf_counter_ns() - t0
        if dt < best:
            best = dt
    return best * 1e-9


def main() -> int:
    p = argparse.ArgumentParser(
        description='Micro-benchmark pixpat across draw_pattern and convert.'
    )
    p.add_argument('--width', type=int, default=1920)
    p.add_argument('--height', type=int, default=1080)
    p.add_argument('--iters', type=int, default=200)
    p.add_argument('--warmup', type=int, default=10)
    p.add_argument(
        '--pp-threads',
        type=int,
        default=1,
        help='pixpat thread count; 0 = sensible default',
    )
    p.add_argument(
        '--rec',
        default='BT709',
        choices=[r.name for r in pixpat.Rec],
        help='YCbCr matrix for YUV cases (default BT709)',
    )
    p.add_argument(
        '--range',
        dest='color_range',
        default='LIMITED',
        choices=[r.name for r in pixpat.Range],
        help='Quantization range for YUV cases (default LIMITED)',
    )
    kind_group = p.add_mutually_exclusive_group()
    kind_group.add_argument(
        '--only-pattern',
        action='store_true',
        help='restrict to pattern-draw cases',
    )
    kind_group.add_argument(
        '--only-convert',
        action='store_true',
        help='restrict to convert cases',
    )
    p.add_argument(
        '--only',
        default=None,
        help='comma-separated names; keep cases whose lhs OR rhs of " -> " matches',
    )
    p.add_argument(
        '--ionly',
        default=None,
        help='comma-separated names; keep cases whose lhs (pattern or src fmt) matches',
    )
    p.add_argument(
        '--oonly',
        default=None,
        help='comma-separated names; keep cases whose rhs (dst fmt) matches',
    )
    p.add_argument(
        '--cases',
        default=None,
        help='comma-separated full case names (e.g. "smpte -> BGR888,RGB888 -> BGR888")',
    )
    p.add_argument(
        '--tsv',
        action='store_true',
        help='emit tab-separated rows on stdout; meta and warnings go to stderr',
    )
    args = p.parse_args()

    w, h = args.width, args.height
    mp = (w * h) / 1e6

    rec = pixpat.Rec[args.rec]
    color_range = pixpat.Range[args.color_range]

    kinds: set[str] = {'pattern', 'convert'}
    if args.only_pattern:
        kinds = {'pattern'}
    elif args.only_convert:
        kinds = {'convert'}

    cases = _build_cases(w, h, kinds)
    cases = _filter_cases(cases, args.only, args.ionly, args.oonly, args.cases)
    if not cases:
        print('no cases matched filter')
        return 1

    info: Callable[[str], None] = (lambda m: print(m, file=sys.stderr)) if args.tsv else print

    info(
        f'Resolution: {w}x{h} ({mp:.2f} MP/frame), '
        f'iters={args.iters}, warmup={args.warmup}, '
        f'pp threads={args.pp_threads}, '
        f'rec={rec.name}, range={color_range.name}'
    )

    name_w = max((len(c.name) for c in cases), default=20) + 2

    if args.tsv:
        print('\t'.join(['case', 'pp_mps', 'pp_fps']))
    else:
        print()
        header = f'{"case":<{name_w}} {"pp MP/s":>9} {"pp fps":>8}'
        print(header)
        print('-' * len(header))

    rng = np.random.default_rng(0)

    for case in cases:
        if case.kind == 'convert' and case.src_arr is not None:
            case.src_arr[...] = rng.integers(0, 256, size=case.src_arr.shape, dtype=np.uint8)

        run = _bind(case, args.pp_threads, rec=rec, color_range=color_range)
        gc.disable()
        try:
            for _ in range(args.warmup):
                run()
            t = _time_n(run, args.iters)
        except (pixpat.PixpatError, ValueError, TypeError) as e:
            info(f'{case.name:<{name_w}} skipped: {type(e).__name__}: {e}')
            continue
        finally:
            gc.enable()

        fps = 1.0 / t
        mps = fps * mp
        if args.tsv:
            print(f'{case.name}\t{mps:.0f}\t{fps:.0f}')
        else:
            print(f'{case.name:<{name_w}} {mps:>9.0f} {fps:>8.0f}')

    return 0


if __name__ == '__main__':
    raise SystemExit(main())
