#!/usr/bin/env python3
"""pixpat code generator.

Reads a small user TOML config plus the hand-written catalogs
(format_catalog.h, pattern_catalog.h — both X-macro lists) and emits
the per-build capability tables consumed by pixpat.cpp /
pixpat_convert.cpp / pixpat_pattern.cpp.

Outputs:
  pixpat_config.h  — PIXPAT_FEATURE_PATTERN / _CONVERT defines
  pixpat_caps.inc  — s_format_caps[] indexed by FormatId
                     s_pattern_caps[] indexed by PatternId

The convert dispatch (dispatch_dst_convert / dispatch_src_convert /
dispatch_convert) and pattern dispatch (try_pattern / try_default
arms) are hand-written and consume the capability arrays via
`if constexpr`.

A --query mode prints 0/1 to stdout for use from meson.
"""

import argparse
import re
import sys
import tomllib
from pathlib import Path


# === Catalog parsers =============================================================
#
# Both catalogs are hand-written X-macro lists; we parse them here so
# that adding a format/pattern is a single-file edit. The macro form
# is rigid enough that a simple regex over the body works.


def _parse_x_macro(path: Path, macro_name: str, row_re: re.Pattern) -> list:
    """Find `#define <macro_name>(X) ...` and return row_re's captures.

    The macro body extends from the `#define` line through the first
    line that does not end with a backslash.
    """
    lines = path.read_text().splitlines()
    header_re = re.compile(rf'#define\s+{macro_name}\s*\(\s*X\s*\)')
    i = 0
    while i < len(lines) and not header_re.search(lines[i]):
        i += 1
    if i == len(lines):
        raise SystemExit(f'{macro_name} not found in {path}')
    body_lines: list[str] = []
    while True:
        body_lines.append(lines[i])
        if not lines[i].rstrip().endswith('\\'):
            break
        i += 1
        if i == len(lines):
            break
    rows = row_re.findall('\n'.join(body_lines))
    if not rows:
        raise SystemExit(f'{macro_name} is empty in {path}')
    return rows


_FORMAT_ROW = re.compile(r'X\(\s*(\w+)\s*\)')
_PATTERN_ROW = re.compile(r'X\(\s*(\w+)\s*,\s*(\w+)\s*,\s*(\w+)\s*,\s*"([^"]+)"\s*\)')


def parse_format_catalog(path: Path) -> list[str]:
    return _parse_x_macro(path, 'PIXPAT_FORMAT_LIST', _FORMAT_ROW)


def parse_pattern_catalog(path: Path) -> list[tuple[str, str, str, str]]:
    """Return [(label, rgb_type, yuv_type, name), …] from pattern_catalog.h.

    `label` is the C++ identifier doubling as the PatternId enum value
    and the s_pattern_caps[] index. `rgb_type` and `yuv_type` are the
    C++ classes implementing the pattern in each color kind, or the
    sentinel `void` if the pattern has no variant in that kind.
    `name` is the lowercase string exposed via the C ABI.
    """
    return _parse_x_macro(path, 'PIXPAT_PATTERN_LIST', _PATTERN_ROW)


VALID_CAPS = {'rw', 'r', 'w', 'off'}


# === Resolution =================================================================


def resolve(
    cfg: dict, format_catalog: list[str], pattern_catalog: list[tuple[str, str, str, str]]
) -> dict:
    """Combine catalogs + user config; return concrete settings."""
    features = cfg.get('features', {})
    have_pattern = bool(features.get('pattern', True))
    have_convert = bool(features.get('convert', True))
    default_caps = features.get('default_format_caps', 'rw')
    if default_caps not in VALID_CAPS:
        raise SystemExit(f'invalid default_format_caps: {default_caps!r}')

    catalog_names = set(format_catalog)
    overrides = cfg.get('formats', {}) or {}
    for name in overrides:
        if name not in catalog_names:
            raise SystemExit(f'[formats] override for unknown format: {name!r}')
        if overrides[name] not in VALID_CAPS:
            raise SystemExit(f'invalid caps for {name!r}: {overrides[name]!r}')

    formats = []
    for name in format_catalog:
        caps = overrides.get(name, default_caps)
        read = 'r' in caps and caps != 'off'
        write = 'w' in caps and caps != 'off'
        # Reading only matters for the convert path; if convert is off
        # we suppress all reads so unpack_for<Read=false, ...> is the
        # only instantiation seen and unpack_to_norm never gets
        # referenced.
        if not have_convert:
            read = False
        formats.append(
            {
                'name': name,
                'read': read,
                'write': write,
            }
        )

    enabled_names = {f['name'] for f in formats if f['read'] or f['write']}
    hot_pivots = cfg.get('hot_pivots', []) or []
    for p in hot_pivots:
        if p not in enabled_names:
            raise SystemExit(
                f'hot_pivot {p!r} is not an enabled format (must have read or write enabled)'
            )

    catalog_names = {n for (_lbl, _rgb, _yuv, n) in pattern_catalog}
    patterns = cfg.get('patterns', []) or []
    for p in patterns:
        if p not in catalog_names:
            raise SystemExit(f'unknown pattern: {p!r} (known: {sorted(catalog_names)})')

    # An empty pattern list collapses pattern feature to 'off'.
    have_pattern = have_pattern and bool(patterns)

    return {
        'have_pattern': have_pattern,
        'have_convert': have_convert,
        'hot_pivots': hot_pivots,
        'patterns': patterns,
        'formats': formats,
        'pattern_catalog': pattern_catalog,
    }


# === Emitters ===================================================================

HEADER = '// Auto-generated by gen_pixpat.py. Do not edit by hand.'


def emit_config_h(r: dict) -> str:
    return '\n'.join(
        [
            HEADER,
            '#pragma once',
            '',
            f'#define PIXPAT_FEATURE_PATTERN {1 if r["have_pattern"] else 0}',
            f'#define PIXPAT_FEATURE_CONVERT {1 if r["have_convert"] else 0}',
            '',
        ]
    )


def _caps_row(f: dict, hot: set[str]) -> str:
    cells = [
        'true,' if f['read'] else 'false,',
        'true,' if f['write'] else 'false,',
        'true,' if (f['name'] in hot) and f['read'] else 'false,',
        'true' if (f['name'] in hot) and f['write'] else 'false',
    ]
    return f'\t{{ {cells[0]:<7}{cells[1]:<7}{cells[2]:<7}{cells[3]:<6}}}, // {f["name"]}'


def emit_caps_inc(r: dict) -> str:
    out = [HEADER, '']

    # Per-format build capabilities, one row per FormatId (catalog
    # order). The FormatCaps schema and the size sanity-check live in
    # static source (pixpat_internal.h / pixpat.cpp); this file is pure
    # data. Consumers pull individual bools via .readable / .writable /
    # .hot_src / .hot_dst — member accesses on a constexpr array
    # element are themselves constant expressions, so they work as
    # `if constexpr` conditions and as non-type template arguments.
    out += [
        'inline constexpr FormatCaps s_format_caps[] = {',
        '\t//  readable  writable  hot_src  hot_dst',
    ]
    hot = set(r['hot_pivots'])
    for f in r['formats']:
        out.append(_caps_row(f, hot))
    out.append('};')
    out.append('')

    # Per-pattern build capabilities, indexed by PatternId. An unknown
    # or disabled pattern name in pixpat_draw_pattern is an error, so
    # there's no default-fallback row.
    user_patterns = set(r['patterns'])
    out += [
        'inline constexpr PatternCaps s_pattern_caps[] = {',
        '\t//  enabled',
    ]
    for _label, _rgb, _yuv, str_name in r['pattern_catalog']:
        enabled = 'true' if str_name in user_patterns else 'false'
        out.append(f'\t{{ {enabled:<5} }}, // {str_name}')
    out.append('};')
    out.append('')
    return '\n'.join(out)


# === CLI ========================================================================


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('--config', type=Path, required=True)
    ap.add_argument('--format-catalog', type=Path, required=True, help='Path to format_catalog.h.')
    ap.add_argument(
        '--pattern-catalog', type=Path, required=True, help='Path to pattern_catalog.h.'
    )
    ap.add_argument('--out-config-h', type=Path)
    ap.add_argument('--out-caps-inc', type=Path)
    ap.add_argument(
        '--query',
        type=str,
        default=None,
        help='Print 0/1 for: feature_pattern, feature_convert, have_both (= pattern && convert).',
    )
    args = ap.parse_args()

    format_catalog = parse_format_catalog(args.format_catalog)
    pattern_catalog = parse_pattern_catalog(args.pattern_catalog)
    with args.config.open('rb') as fh:
        cfg = tomllib.load(fh)
    r = resolve(cfg, format_catalog, pattern_catalog)

    if args.query:
        flag = {
            'feature_pattern': r['have_pattern'],
            'feature_convert': r['have_convert'],
            'have_both': r['have_pattern'] and r['have_convert'],
        }.get(args.query)
        if flag is None:
            raise SystemExit(f'unknown query: {args.query!r}')
        print(1 if flag else 0)
        return 0

    outs = [
        (args.out_config_h, emit_config_h(r)),
        (args.out_caps_inc, emit_caps_inc(r)),
    ]
    if any(o[0] is None for o in outs):
        raise SystemExit('all --out-* flags are required when not in --query mode')
    for path, content in outs:
        path.write_text(content)
    return 0


if __name__ == '__main__':
    sys.exit(main())
