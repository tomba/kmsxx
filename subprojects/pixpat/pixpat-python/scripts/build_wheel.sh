#!/usr/bin/env bash
# Build a pixpat wheel for the given target architecture.
#
# Usage: pixpat-python/scripts/build_wheel.sh <x86_64|aarch64>
#
# Meson is invoked from setup.py during the wheel build; this script just
# selects the target arch and hands off to `python -m build`. The meson
# build dir lands at pixpat-python/build-<arch>/native/, so cross-compiling
# both arches in turn keeps each arch's compile incremental.
#
# Prerequisites:
#   - meson, ninja in PATH (e.g. `pip install meson ninja`)
#   - For aarch64: gcc-aarch64-linux-gnu, g++-aarch64-linux-gnu
#     (sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu)
#   - Python with `build` package: pip install build

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

# --no-isolation: reuse the host environment so meson's per-arch build dir
# (pixpat-python/build-<arch>/native/) survives across invocations and stays
# incremental. Build isolation copies the source to a temp dir, so the meson
# build dir would be thrown away each run.
PIXPAT_TARGET_ARCH="$ARCH" python -m build --wheel --no-isolation

echo
echo "Wheel(s) in dist/:"
ls -1 dist/*.whl
