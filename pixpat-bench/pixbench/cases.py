"""Cases — one `<source> -> <destination>` operation each — and the
filter that narrows a run to whatever is being worked on.

A case's left-hand side is a source format (a conversion) or a pattern
name (a draw); the right-hand side is always the destination format.
Filters use the same two sides, so `--only NV12` keeps every case that
reads or writes NV12, patterns into NV12 included, and `--src smpte`
keeps the smpte draws.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass

from .formats import FAMILIES, FORMATS, family

PATTERNS = (
    'kmstest',
    'smpte',
    'plain',
    'checker',
    'hramp',
    'vramp',
    'hbar',
    'vbar',
    'dramp',
    'zoneplate',
)

# `plain` needs a color; `vbar`/`hbar` need a bar position.
PATTERN_PARAMS: dict[str, bytes] = {
    'plain': b'color=ff0000',
    'hbar': b'pos=100',
    'vbar': b'pos=100',
}

# The curated throughput set: one row per I/O shape that matters, both
# directions where both exist, the hot pivot (BGR888) on one side of
# most rows and a few cold-path pairs without it.
CURATED_PAIRS: list[tuple[str, str]] = [
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

# Sinks every pattern is drawn into in the curated set.
PATTERN_SINKS = [
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


@dataclass(frozen=True)
class Case:
    kind: str  # 'convert' | 'pattern'
    src: str  # source format, or pattern name
    dst: str

    @property
    def name(self) -> str:
        return f'{self.src} -> {self.dst}'

    @property
    def params(self) -> bytes | None:
        return PATTERN_PARAMS.get(self.src) if self.kind == 'pattern' else None


def groups(case: Case) -> tuple[str, ...]:
    """The groups a case is summarized under: its kind, and one per
    side. A conversion is in the I/O family it reads (`src=semiplanar`)
    and the one it writes (`dst=bayer-csi2`); a draw is in its pattern
    (`pattern=smpte`) and the family it writes (`sink=bayer-csi2`).
    The two kinds keep separate destination groups because the write
    loop is a different share of each: the same sink regression is
    0.2x on the conversions and 0.9x on the draws, and one group would
    average them into nothing. The groups overlap on purpose — a case
    is in one per axis — so a change to one family's loop shows on
    that axis whatever is on the other side; they are for seeing which
    loop moved, not for accounting."""
    if case.kind == 'pattern':
        return (case.kind, f'pattern={case.src}', f'sink={family(case.dst)}')
    return (case.kind, f'src={family(case.src)}', f'dst={family(case.dst)}')


_GROUP_ORDER = {
    g: i
    for i, g in enumerate(
        ['convert', 'pattern']
        + [f'src={f}' for f in FAMILIES]
        + [f'dst={f}' for f in FAMILIES]
        + [f'pattern={p}' for p in PATTERNS]
        + [f'sink={f}' for f in FAMILIES]
    )
}


def group_order(label: str) -> tuple:
    """Kinds, then the conversion groups (sources, destinations), then
    the draw groups (patterns, sinks), families in table order —
    stable across runs."""
    return (_GROUP_ORDER.get(label, len(_GROUP_ORDER)), label)


def convert_case(src: str, dst: str) -> Case:
    return Case('convert', src, dst)


def pattern_case(pattern: str, dst: str) -> Case:
    return Case('pattern', pattern, dst)


def curated() -> list[Case]:
    cases = [pattern_case(p, f) for f in PATTERN_SINKS for p in PATTERNS]
    cases += [convert_case(s, d) for s, d in CURATED_PAIRS]
    return cases


def everything() -> list[Case]:
    """Every pattern into every format, every format pair."""
    cases = [pattern_case(p, f) for f in FORMATS for p in PATTERNS]
    cases += [convert_case(s, d) for s in FORMATS for d in FORMATS]
    return cases


def normalize_name(token: str) -> str:
    """Pattern names are lower-case, format names upper-case."""
    t = token.strip()
    return t.lower() if t.lower() in PATTERNS else t.upper()


def parse_case(text: str) -> Case:
    lhs, sep, rhs = text.partition('->')
    if not sep:
        raise ValueError(f'not a case: {text!r} (expected "SRC -> DST")')
    src, dst = normalize_name(lhs), normalize_name(rhs)
    return Case('pattern' if src in PATTERNS else 'convert', src, dst)


def _name_set(text: str | None) -> frozenset[str] | None:
    if text is None:
        return None
    return frozenset(normalize_name(t) for t in text.split(',') if t.strip())


@dataclass(frozen=True)
class Filter:
    """What to look at. Every given criterion must hold."""

    only: frozenset[str] | None = None  # either side
    src: frozenset[str] | None = None  # source format or pattern name
    dst: frozenset[str] | None = None  # destination format
    cases: frozenset[str] | None = None  # exact case names
    patterns: frozenset[str] | None = None  # pattern cases restricted to these
    kind: str | None = None  # 'convert' | 'pattern'

    @staticmethod
    def add_arguments(parser: argparse.ArgumentParser) -> None:
        g = parser.add_argument_group(
            'case filters',
            'narrow the run to the formats/patterns being worked on; '
            'names are case-insensitive, lists comma-separated',
        )
        g.add_argument('--only', metavar='NAMES', help='formats or patterns on either side')
        g.add_argument('--src', metavar='NAMES', help='source formats or pattern names')
        g.add_argument('--dst', metavar='FORMATS', help='destination formats')
        g.add_argument(
            '--cases', metavar='CASES', help='exact cases, e.g. "NV12 -> BGR888,smpte -> NV12"'
        )
        g.add_argument('--patterns', metavar='NAMES', help='keep only these pattern draws')
        g.add_argument('--kind', choices=('convert', 'pattern'))

    @classmethod
    def from_args(cls, args: argparse.Namespace) -> Filter:
        cases = None
        if args.cases is not None:
            cases = frozenset(parse_case(c).name for c in args.cases.split(',') if c.strip())
        return cls(
            only=_name_set(args.only),
            src=_name_set(args.src),
            dst=_name_set(args.dst),
            cases=cases,
            patterns=_name_set(args.patterns),
            kind=args.kind,
        )

    def is_empty(self) -> bool:
        return self == Filter()

    def matches(self, case: Case) -> bool:
        if self.kind is not None and case.kind != self.kind:
            return False
        if self.only is not None and case.src not in self.only and case.dst not in self.only:
            return False
        if self.src is not None and case.src not in self.src:
            return False
        if self.dst is not None and case.dst not in self.dst:
            return False
        if self.cases is not None and case.name not in self.cases:
            return False
        return not (
            self.patterns is not None and case.kind == 'pattern' and case.src not in self.patterns
        )

    def formats(self) -> frozenset[str] | None:
        """The formats named anywhere in the filter, or None when no
        format is named (no restriction on per-format checks)."""
        if self.only is None and self.src is None and self.dst is None and self.cases is None:
            return None
        named: set[str] = set()
        for group in (self.only, self.src, self.dst):
            if group:
                named |= {n for n in group if n not in PATTERNS}
        for name in self.cases or ():
            case = parse_case(name)
            named.add(case.dst)
            if case.kind == 'convert':
                named.add(case.src)
        return frozenset(named)

    def describe(self) -> str | None:
        """Canonical text form, stored with the measurement."""
        parts = []
        for key in ('only', 'src', 'dst', 'cases', 'patterns'):
            val = getattr(self, key)
            if val is not None:
                parts.append(f'{key}={",".join(sorted(val))}')
        if self.kind is not None:
            parts.append(f'kind={self.kind}')
        return ' '.join(parts) or None
