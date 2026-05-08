"""setup.py overrides several setuptools commands to integrate the meson-built
libpixpat.so into the Python wheel.

Layout (where things land for each workflow):

  meson setup build / meson compile -C build       — repo-root build/, the
      C++ developer's meson dir. Python tooling never touches this.

  pip install -e .                                  — DevEditableWheel
      symlinks build/libpixpat.so into pixpat-python/pixpat/_lib/.
      Requires the user to have built `build/` themselves.

  pip install .                                     — MesonBuildPy invokes
      meson into pixpat-python/build/native/, copies the .so straight into
      setuptools' build_lib (pixpat-python/build/lib/pixpat/_lib/), and
      bdist_wheel zips that into the wheel. The source tree's _lib/ is
      never touched on this path, so wheel builds don't depend on (or
      pollute) it.

  scripts/build_wheel.sh <arch>                     — sets PIXPAT_TARGET_ARCH
      and runs `python -m build`; meson lands in pixpat-python/build-<arch>/
      native/, setuptools alongside.

bdist_wheel: setuptools' default produces a `py3-none-any` (pure-Python) wheel
when there's no compiled extension. We bundle a `.so` as package data, so we
MUST tag the wheel for a specific platform — otherwise pip would
install our x86_64 wheel on an arm64 box. Target arch is read from
PIXPAT_TARGET_ARCH and falls back to the running machine's arch — important
because setuptools' editable_wheel delegates wheel construction to whatever
bdist_wheel is registered, so this class is invoked during `pip install -e .`
too.
"""

import os
import platform
import shutil
import subprocess
from pathlib import Path

from setuptools import setup
from setuptools.command.bdist_wheel import bdist_wheel
from setuptools.command.build import build
from setuptools.command.build_py import build_py
from setuptools.command.editable_wheel import editable_wheel

REPO_ROOT = Path(__file__).resolve().parent
LIB_DIR = REPO_ROOT / 'pixpat-python' / 'pixpat' / '_lib'


def _target_arch() -> str:
    return os.environ.get('PIXPAT_TARGET_ARCH') or platform.machine()


def _wheel_build_root() -> Path:
    """Top-level dir where setuptools and meson stage wheel-build output.

    Plain `pip install .` uses pixpat-python/build/. Cross-compile via
    PIXPAT_TARGET_ARCH gets a per-arch sibling so meson can keep arches
    incrementally compiled side by side.
    """
    arch = os.environ.get('PIXPAT_TARGET_ARCH')
    suffix = f'-{arch}' if arch else ''
    return REPO_ROOT / 'pixpat-python' / f'build{suffix}'


class ImpureBdistWheel(bdist_wheel):
    def finalize_options(self):
        super().finalize_options()
        self.root_is_pure = False

    def get_tag(self):
        return ('py3', 'none', f'linux_{_target_arch()}')


class WheelBuild(build):
    """Direct setuptools' staging into pixpat-python/build[-<arch>]/."""

    def initialize_options(self):
        super().initialize_options()
        self.build_base = str(_wheel_build_root())


class MesonBuildPy(build_py):
    """Compile libpixpat.so via meson and stage it into build_lib for the wheel."""

    def run(self):
        super().run()
        # editable_mode is set by setuptools' editable_wheel command. In that
        # path DevEditableWheel has already symlinked the user's repo-root
        # build/ into the source _lib/, so meson must not run.
        if not self.editable_mode:
            self._build_native()

    def _build_native(self):
        meson_dir = _wheel_build_root() / 'native'
        arch = _target_arch()
        cross_args = []
        if arch != platform.machine():
            cross_file = REPO_ROOT / 'pixpat-native' / 'cross' / f'{arch}-linux-gnu.txt'
            if not cross_file.exists():
                raise SystemExit(f'no cross-file for {arch}: {cross_file}')
            cross_args = ['--cross-file', str(cross_file)]

        if not (meson_dir / 'meson-info').exists():
            meson_dir.parent.mkdir(parents=True, exist_ok=True)
            subprocess.check_call(
                ['meson', 'setup', str(meson_dir), '--buildtype=release', *cross_args],
                cwd=REPO_ROOT,
            )
        subprocess.check_call(['meson', 'compile', '-C', str(meson_dir)], cwd=REPO_ROOT)

        out_dir = Path(self.build_lib) / 'pixpat' / '_lib'
        out_dir.mkdir(parents=True, exist_ok=True)
        # follow_symlinks resolves meson's libpixpat.so -> .so.<maj> -> .so.<maj>.<min>.<patch>
        # chain into a regular file. The wheel ships only the unversioned name, so we
        # don't have to track meson's project version here.
        shutil.copy2(meson_dir / 'libpixpat.so', out_dir / 'libpixpat.so')


class DevEditableWheel(editable_wheel):
    """Symlink the user's repo-root meson build into _lib/ before installing."""

    def run(self):
        build_so = REPO_ROOT / 'build' / 'libpixpat.so'

        if not build_so.exists():
            raise SystemExit(
                f'editable install needs {build_so}.\n'
                'Configure and build first:\n'
                '  meson setup build\n'
                '  meson compile -C build'
            )

        LIB_DIR.mkdir(parents=True, exist_ok=True)
        for old in LIB_DIR.glob('libpixpat.so*'):
            old.unlink()
        # Relative path so the symlink survives moves of the source tree.
        (LIB_DIR / 'libpixpat.so').symlink_to(
            Path('..') / '..' / '..' / 'build' / 'libpixpat.so'
        )

        super().run()


setup(
    cmdclass={
        'bdist_wheel': ImpureBdistWheel,
        'build': WheelBuild,
        'build_py': MesonBuildPy,
        'editable_wheel': DevEditableWheel,
    }
)
