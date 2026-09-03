#!/usr/bin/env bash
# Build a pixpat wheel for the given target architecture.
#
# Usage: pixpat-python/scripts/build_wheel.sh <x86_64|aarch64>
#
# Meson is invoked from setup.py during the wheel build; this script just
# selects the target arch and hands off to a PEP 517 build frontend.
# PIXPAT_TARGET_ARCH tells setup.py which meson cross file to use and which
# platform tag to stamp on the wheel. The wheel is built clean every time --
# see setup.py for why nothing is cached between runs.
#
# Nothing needs to be installed system-wide beyond python3 (with the venv
# module) and a toolchain for the target:
#
#   - For aarch64: sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
#   - Network access on the first run.
#
# Everything else is provisioned automatically, in two layers:
#
#   1. This script creates pixpat-python/build-venv/ holding just the build
#      frontend. That is a tool environment, not build output; delete it to
#      re-provision.
#   2. The frontend then applies PEP 517 build isolation: it reads
#      [build-system] requires from pyproject.toml and installs setuptools,
#      meson and ninja into a throwaway environment of its own. Those versions
#      are therefore never duplicated here, and a too-old distro setuptools
#      (setup.py needs >= 70.1) cannot interfere.

set -euo pipefail

ARCH="${1:-}"
case "$ARCH" in
    x86_64|aarch64) ;;
    *)
        echo "usage: $0 <x86_64|aarch64>" >&2
        exit 1
        ;;
esac

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$REPO_ROOT"

VENV_DIR="$REPO_ROOT/pixpat-python/build-venv"

# Absolute paths throughout, so an unrelated venv being active in the caller's
# shell makes no difference.
if [ ! -x "$VENV_DIR/bin/python" ]; then
    echo "Creating build environment in $VENV_DIR"
    "${PYTHON:-python3}" -m venv "$VENV_DIR"
    "$VENV_DIR/bin/python" -m pip install --quiet --upgrade pip build
fi

PIXPAT_TARGET_ARCH="$ARCH" "$VENV_DIR/bin/python" -m build --wheel --outdir dist

echo
echo "Wheel(s) in dist/:"
ls -1 dist/*.whl
