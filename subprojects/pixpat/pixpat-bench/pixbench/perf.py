"""Throughput: time each case through every library in turn.

One frame pair per case, shared by all libraries; per round, per
library, `iters` timed calls, so a drift in clock or thermals lands on
every library alike instead of on whichever ran last. The recorded
number is the minimum over all samples (the median goes along for the
noise picture).
"""

from __future__ import annotations

import argparse
import ctypes
import gc
import os
import random
import statistics
import time
from collections.abc import Callable
from dataclasses import dataclass

from .abi import ConvertOpts, Lib, PatternOpts
from .cases import Case
from .formats import RANGE_NAMES, REC_NAMES, aligned, make_frame


@dataclass(frozen=True)
class PerfOpts:
    w: int = 1920
    h: int = 1080
    threads: tuple[int, ...] = (1,)
    rec: int = 1  # BT709
    rng: int = 0  # limited
    iters: int = 10
    warmup: int = 2
    rounds: int = 3
    seed: int = 0
    cpus: tuple[int, ...] | None = None

    @staticmethod
    def add_arguments(parser: argparse.ArgumentParser) -> None:
        g = parser.add_argument_group('timing')
        g.add_argument('--size', default='1920x1080', metavar='WxH')
        g.add_argument('--threads', default='1', metavar='N[,N...]', help='pixpat thread counts')
        g.add_argument('--rec', default='BT709', choices=REC_NAMES)
        g.add_argument('--range', default='LIMITED', choices=RANGE_NAMES)
        g.add_argument('--iters', type=int, default=10, help='timed calls per round')
        g.add_argument('--warmup', type=int, default=2, help='untimed calls before the first')
        g.add_argument('--rounds', type=int, default=3, help='passes over the libraries')
        g.add_argument('--seed', type=int, default=0, help='source fill seed')
        g.add_argument(
            '--cpu',
            metavar='N[-M][,N...]',
            default=os.environ.get('PIXBENCH_CPU'),
            help='pin to these CPUs (default $PIXBENCH_CPU; unpinned when unset)',
        )

    @classmethod
    def from_args(cls, args: argparse.Namespace) -> PerfOpts:
        w, _, h = args.size.lower().partition('x')
        return cls(
            w=int(w),
            h=int(h),
            threads=tuple(int(t) for t in args.threads.split(',') if t.strip()),
            rec=REC_NAMES.index(args.rec),
            rng=RANGE_NAMES.index(args.range),
            iters=args.iters,
            warmup=args.warmup,
            rounds=args.rounds,
            seed=args.seed,
            cpus=parse_cpus(args.cpu) if args.cpu else None,
        )


def parse_cpus(text: str) -> tuple[int, ...]:
    cpus: list[int] = []
    for item in text.split(','):
        part = item.strip()
        if not part:
            continue
        lo, _, hi = part.partition('-')
        cpus.extend(range(int(lo), int(hi or lo) + 1))
    return tuple(cpus)


def pin(cpus: tuple[int, ...] | None) -> None:
    if cpus:
        os.sched_setaffinity(0, set(cpus))


def _call(case: Case, src, dst, opts) -> Callable[[Lib], int]:
    if case.kind == 'convert':
        return lambda lib: lib.convert(
            ctypes.byref(dst.buf), ctypes.byref(src.buf), ctypes.byref(opts)
        )
    pattern = case.src.encode('ascii')
    return lambda lib: lib.draw_pattern(ctypes.byref(dst.buf), pattern, ctypes.byref(opts))


def run(
    libs: list[Lib],
    cases: list[Case],
    opts: PerfOpts,
    on_result: Callable[[dict], None] | None = None,
) -> tuple[list[dict], list[str]]:
    """Returns (records, notes). A record's 'lib' is the index into
    `libs`; the caller names it."""
    pin(opts.cpus)
    w, h = opts.w, opts.h
    records: list[dict] = []
    notes: list[str] = []
    for case in cases:
        fmts = [case.dst] if case.kind == 'pattern' else [case.src, case.dst]
        if not all(aligned(f, w, h) for f in fmts):
            notes.append(f'{case.name}: {w}x{h} is not a whole number of blocks, skipped')
            continue
        src = None
        if case.kind == 'convert':
            src = make_frame(case.src, w, h, fill=random.Random(repr((opts.seed, case.name))))
        dst = make_frame(case.dst, w, h)
        for threads in opts.threads:
            if case.kind == 'convert':
                copts = ConvertOpts(rec=opts.rec, range=opts.rng, num_threads=threads)
            else:
                copts = PatternOpts(
                    rec=opts.rec, range=opts.rng, num_threads=threads, params=case.params
                )
            call = _call(case, src, dst, copts)
            samples: dict[int, list[int]] = {}
            for i, lib in enumerate(libs):
                rc = call(lib)
                for _ in range(opts.warmup - 1):
                    rc = call(lib)
                if rc != 0:
                    notes.append(f'{case.name} t={threads}: {lib.path.name} rc={rc}, skipped')
                    continue
                samples[i] = []
            gc.disable()
            try:
                for _ in range(opts.rounds):
                    for i, lib in enumerate(libs):
                        if i not in samples:
                            continue
                        out = samples[i]
                        for _ in range(opts.iters):
                            t0 = time.perf_counter_ns()
                            call(lib)
                            out.append(time.perf_counter_ns() - t0)
            finally:
                gc.enable()
            for i, s in samples.items():
                mn = min(s)
                rec = {
                    'lib': i,
                    'case': case.name,
                    'kind': case.kind,
                    'w': w,
                    'h': h,
                    'threads': threads,
                    'rec': REC_NAMES[opts.rec],
                    'range': RANGE_NAMES[opts.rng],
                    'iters': opts.iters,
                    'warmup': opts.warmup,
                    'rounds': opts.rounds,
                    'samples': len(s),
                    'min_ns': mn,
                    'median_ns': int(statistics.median(s)),
                    'mpx_s': round(w * h * 1e3 / mn, 1),
                }
                records.append(rec)
                if on_result:
                    on_result(rec)
    return records, notes


def record_key(rec: dict) -> tuple:
    """What identifies a measurement apart from who was measured."""
    return (rec['case'], rec['w'], rec['h'], rec['threads'], rec['rec'], rec['range'])
