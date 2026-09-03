"""Building the library from a source tree through meson, each variant
(compiler, profile, arch, optimization knobs) described by a
BuildConfig that also names the subject in the run files.

A subject on the command line is `[NAME=]BASE[:PROFILE][,key=value...]`:

  BASE     gcc | gcc-14 | clang | clang-19 | cpp
  PROFILE  a name under pixpat-native/profiles/ (no_hotpath) or a path
  keys     cc cxx profile arch lto opt buildtype

so `gcc`, `clang:no_hotpath`, `gcc-14,arch=x86-64-v3`,
`NAME=cpp,cc=icx,cxx=icpx`.
"""

from __future__ import annotations

import dataclasses
import json
import os
import pathlib
import re
import shutil
from collections.abc import Callable
from dataclasses import dataclass

from .util import CommandError, file_sha256, first_line, git, git_toplevel, run, sha256_text

DEFAULT_PROFILE = 'pixpat-native/profiles/pixpat.toml'
KEYS = ('lang', 'cc', 'cxx', 'profile', 'arch', 'lto', 'opt', 'buildtype')

# The source subtrees a language's library is built from; a commit that
# leaves them untouched yields the same library.
LANG_PATHS = {
    'cpp': ('pixpat-native', 'meson.build', 'meson.options'),
}
LIB_NAME = {'cpp': 'libpixpat.so'}


class BuildError(Exception):
    pass


@dataclass(frozen=True)
class BuildConfig:
    lang: str
    cc: str | None = None
    cxx: str | None = None
    profile: str = DEFAULT_PROFILE
    arch: str | None = None  # -march
    lto: str | None = None
    opt: str | None = None  # meson optimization
    buildtype: str | None = None

    def as_dict(self) -> dict:
        return {k: v for k, v in dataclasses.asdict(self).items() if v is not None}

    def toolchain_label(self) -> str:
        return self.cc or 'cpp'


def _profile_path(text: str) -> str:
    if '/' in text or text.endswith('.toml'):
        return text
    return f'pixpat-native/profiles/{text}.toml'


def _cxx_for(cc: str) -> str:
    m = re.fullmatch(r'(gcc|clang)(-[\w.]+)?', cc)
    if m:
        return ('g++' if m.group(1) == 'gcc' else 'clang++') + (m.group(2) or '')
    raise ValueError(f'cannot derive a C++ compiler from cc={cc!r}; give cxx=')


def _base(base: str) -> dict:
    if base == 'cpp':
        return {'lang': 'cpp'}
    if re.fullmatch(r'(gcc|clang)(-[\w.]+)?', base):
        return {'lang': 'cpp', 'cc': base}
    raise ValueError(f'unknown subject base {base!r} (gcc, gcc-N, clang, clang-N, cpp)')


def default_name(cfg: BuildConfig) -> str:
    parts = [cfg.toolchain_label()]
    if cfg.profile != DEFAULT_PROFILE:
        parts.append(pathlib.Path(cfg.profile).stem)
    for key in ('arch', 'lto', 'opt', 'buildtype'):
        val = getattr(cfg, key)
        if val is not None:
            parts.append(f'{key}-{val}')
    if cfg.lang == 'cpp' and cfg.cc is None:
        parts.append(f'cc-{cfg.cxx}' if cfg.cxx else 'cpp')
    return re.sub(r'[^A-Za-z0-9_.+-]+', '-', '-'.join(parts))


def parse_subject(spec: str) -> tuple[str, BuildConfig]:
    tokens = [t.strip() for t in spec.split(',') if t.strip()]
    if not tokens:
        raise ValueError('empty subject')
    name = None
    key, eq, val = tokens[0].partition('=')
    if eq and key not in KEYS:
        name, tokens[0] = key, val
    cfg: dict = {}
    head = tokens[0]
    if '=' not in head or head.partition('=')[0] not in KEYS:
        base, colon, prof = head.partition(':')
        cfg.update(_base(base))
        if colon:
            cfg['profile'] = _profile_path(prof)
        tokens = tokens[1:]
    for tok in tokens:
        k, eq, v = tok.partition('=')
        if not eq or k not in KEYS:
            raise ValueError(f'bad subject token {tok!r}; keys are {", ".join(KEYS)}')
        cfg[k] = _profile_path(v) if k == 'profile' else v
    if 'lang' not in cfg:
        raise ValueError(f'subject {spec!r} names no language (gcc, clang, cpp, or lang=)')
    if cfg['lang'] not in LANG_PATHS:
        raise ValueError(f'unknown lang {cfg["lang"]!r}')
    if cfg['lang'] == 'cpp' and cfg.get('cc') and not cfg.get('cxx'):
        cfg['cxx'] = _cxx_for(cfg['cc'])
    bc = BuildConfig(**cfg)
    return (name or default_name(bc)), bc


def toolchain_versions(cfg: BuildConfig) -> dict[str, str]:
    def ver(cmd: list[str]) -> str:
        try:
            return first_line(run(cmd))
        except (CommandError, FileNotFoundError) as e:
            raise BuildError(f'{cmd[0]}: {e}') from e

    return {
        'cc': ver([cfg.cc or 'cc', '--version']),
        'cxx': ver([cfg.cxx or 'c++', '--version']),
        'meson': ver(['meson', '--version']),
        'ninja': ver(['ninja', '--version']),
    }


def config_hash(cfg: BuildConfig, profile_sha256: str, versions: dict[str, str]) -> str:
    payload = {'config': cfg.as_dict(), 'profile': profile_sha256, 'toolchain': versions}
    return sha256_text(json.dumps(payload, sort_keys=True))


def subtree_hashes(repo: pathlib.Path, rev: str, paths: tuple[str, ...]) -> dict[str, str]:
    out = {}
    for p in paths:
        try:
            out[p] = git(repo, 'rev-parse', '--verify', '-q', f'{rev}:{p}')
        except CommandError:
            pass
    return out


def source_info(src: pathlib.Path, lang: str) -> dict:
    """What a work tree is: its commit, whether it is dirty, and the
    tree hashes of the subtrees this language builds from. An exported
    or otherwise non-git tree has none of that."""
    top = git_toplevel(src)
    if top is None:
        return {'commit': None, 'dirty': None, 'describe': None, 'subtrees': {}}
    return {
        'commit': git(src, 'rev-parse', 'HEAD'),
        'dirty': bool(git(src, 'status', '--porcelain')),
        'describe': git(src, 'describe', '--always', '--dirty'),
        'subtrees': subtree_hashes(src, 'HEAD', LANG_PATHS[lang]),
    }


def resolve_profile(cfg: BuildConfig, src: pathlib.Path) -> pathlib.Path:
    p = pathlib.Path(cfg.profile)
    return p if p.is_absolute() else src / p


def build(
    cfg: BuildConfig,
    src: pathlib.Path,
    out: pathlib.Path,
    *,
    source: dict | None = None,
    log: Callable[[str], None] = lambda _msg: None,
) -> dict:
    """Build `src` into `out`; returns the build record (also written
    to out/build.json). Raises BuildError with the log's tail."""
    src = src.resolve()
    out.mkdir(parents=True, exist_ok=True)
    profile = resolve_profile(cfg, src)
    if not profile.is_file():
        raise BuildError(f'profile not found: {profile}')
    profile_sha = file_sha256(profile)
    versions = toolchain_versions(cfg)
    logfile = out / 'build.log'
    log(f'building {cfg.toolchain_label()} from {src} into {out}')
    try:
        with logfile.open('w') as lf:
            lib = _build_cpp(cfg, src, out, profile, lf)
    except CommandError as e:
        tail = logfile.read_text().splitlines()[-30:]
        raise BuildError(f'{e}\n' + '\n'.join(tail)) from e
    dest = out / LIB_NAME[cfg.lang]
    shutil.copy2(lib, dest)
    info = {
        'source': source or source_info(src, cfg.lang),
        'config': cfg.as_dict(),
        'profile_sha256': profile_sha,
        'toolchain_versions': versions,
        'config_hash': config_hash(cfg, profile_sha, versions),
        'lib': {'path': str(dest), 'sha256': file_sha256(dest)},
        'log': str(logfile),
    }
    (out / 'build.json').write_text(json.dumps(info, indent=1) + '\n')
    return info


def _build_cpp(cfg, src, out, profile, lf) -> pathlib.Path:
    bdir = out / 'meson'
    env = dict(os.environ)
    if cfg.cc:
        env['CC'] = cfg.cc
    if cfg.cxx:
        env['CXX'] = cfg.cxx
    args = [
        'meson',
        'setup',
        str(bdir),
        str(src),
        f'--buildtype={cfg.buildtype or "release"}',
        f'-Dconfig={os.path.relpath(profile, src)}',
    ]
    if (bdir / 'meson-info').is_dir():
        args.append('--reconfigure')
    if cfg.arch:
        args.append(f'-Dcpp_args=-march={cfg.arch}')
    if cfg.lto:
        lto = 'true' if cfg.lto.lower() in ('on', 'true', 'fat', 'thin') else 'false'
        args.append(f'-Db_lto={lto}')
    if cfg.opt:
        args.append(f'-Doptimization={cfg.opt}')
    lf.write(f'$ {" ".join(args)}\n')
    lf.flush()
    lf.write(run(args, env=env, log=lf))
    lf.write(f'$ ninja -C {bdir}\n')
    lf.flush()
    lf.write(run(['ninja', '-C', str(bdir)], env=env, log=lf))
    return (bdir / 'libpixpat.so').resolve()


def build_key(source: dict, chash: str) -> str:
    """Cache key: the built subtrees plus the whole build config."""
    return sha256_text(json.dumps({'subtrees': source['subtrees'], 'config': chash}))[:16]


def ensure_built(
    cfg: BuildConfig,
    *,
    cache: pathlib.Path,
    source: dict,
    profile_sha256: str,
    get_src: Callable[[], pathlib.Path],
    force: bool = False,
    log: Callable[[str], None] = lambda _msg: None,
) -> dict:
    """Build unless the cache holds this exact (subtrees, config) pair.
    A dirty or non-git tree builds incrementally in a per-config work
    dir instead."""
    versions = toolchain_versions(cfg)
    chash = config_hash(cfg, profile_sha256, versions)
    if source.get('commit') and not source.get('dirty') and source.get('subtrees'):
        out = cache / 'build' / build_key(source, chash)
        bj = out / 'build.json'
        if bj.is_file() and not force:
            info = json.loads(bj.read_text())
            if pathlib.Path(info['lib']['path']).is_file():
                first = (info.get('source') or {}).get('commit') or '?'
                log(f'cached: {out} (built at {first[:10]})')
                # The library is the same; what it stands for is not.
                return {**info, 'source': source}
    else:
        out = cache / 'work' / f'{cfg.toolchain_label()}-{chash[:12]}'
    return build(cfg, get_src(), out, source=source, log=log)
