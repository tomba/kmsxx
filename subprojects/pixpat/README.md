# pixpat

A small C++ library for **pixel format conversion** and **test pattern
generation**, with a C API and Python bindings.

## Why pixpat

- **Templated C++ core.** Each pixel format is described once as a
  *layout* — component order, bit widths, plane shape — and the
  conversion code is generated from those descriptions by the C++
  compiler. Adding a new format or component order is a few lines of
  layout, not a new conversion routine.
- **16-bit normalized pivot.** Conversions don't go format-to-format.
  They unpack into a 16-bit RGB or YUV intermediate and pack from it,
  so cross-color-kind conversions (RGB ↔ YUV, with arbitrary matrix
  and range) cost the same as same-color-kind ones, precision is
  preserved when both endpoints are 8-bit, and the work scales as N+M
  instead of N×M.
- **Built to drop into pipelines.** Caller-owned buffers and a small
  C ABI mean pixpat fits inside the inner loop of a DRM/KMS, V4L2, or
  GPU-upload path without copies. Exceptions are used internally but
  never cross the C ABI. Beyond libc, the only runtime dependency is
  `libstdc++`.

## Why not pixpat

- **You need maximum throughput.** Hand-tuned conversion paths —
  OpenCV with its SIMD intrinsics, vendor-supplied codecs, ffmpeg's
  `swscale` — still outrun pixpat on the heavily-trafficked format
  pairs. pixpat aims for "fast enough" out of a small generated
  codebase, not for raw peak speed.
- **You want a pure-Python library.** The Python package is a thin
  `ctypes` wrapper around `libpixpat.so`. You need a native wheel
  matching your architecture; there is no pure-Python fallback.
- **You need GPU conversion.** pixpat is CPU-only.

## What it offers

- **Test patterns** — `kmstest`, SMPTE RP 219-1 bars, `plain` solid
  fill, R/G/B/gray ramps, checkerboard, color-bar overlays, zone
  plate. All take a single per-pattern parameter string.
- **Format conversion** between packed / semiplanar / planar YUV,
  RGB, raw Bayer, and grayscale. Cross-color-kind conversions
  (RGB ↔ YUV) honor BT.601 / BT.709 / BT.2020 matrices and
  limited / full quantization range.
- **Caller-owned pixel memory.** Callers pass plane pointers and
  strides; pixpat never allocates pixel buffers. The only internal
  allocation is a small per-thread normalized line buffer reused
  across cold-path calls.
- **DRM / kms++ / pixutils format names** (`XRGB8888`, `NV12`, …)
  rather than DRM/V4L2 four-character codes (`XR24`, …). See
  [Format names and byte order](#format-names-and-byte-order) — the
  convention disagrees with OpenCV's.
- **Optional multi-threading** via a single `num_threads` knob on
  both entry points.
- **Build-time tailoring.** A small TOML config selects which formats
  and patterns are compiled in, which formats are read-only /
  write-only, and which formats get a fully-fused fast path.

## Supported formats

The default build ships every format in the catalog. Each one works as
a `pixpat_draw_pattern` target, a `pixpat_convert` source, and a
`pixpat_convert` destination.

- **RGB packed** — `RGB332`, `RGB565`, `BGR565`, the four 1555
  permutations `{XRGB,ARGB,XBGR,ABGR}1555`, the six 4444 permutations
  `{XRGB,ARGB,XBGR,ABGR,RGBX,RGBA}4444`, `RGB888`, `BGR888`, the
  eight 8888 permutations `{XRGB,ARGB,XBGR,ABGR,RGBX,RGBA,BGRX,BGRA}8888`,
  the eight 10-bit permutations `{XRGB,ARGB,XBGR,ABGR}2101010` and
  `{RGBX,RGBA,BGRX,BGRA}1010102`, and `ABGR16161616`. `R8` is a
  single-channel form; on read the unspecified channels are
  synthesized as G=B=R.
- **YUV packed** — `YUYV`, `YVYU`, `UYVY`, `VYUY`, `Y210`, `Y212`,
  `Y216`, `VUY888`, `XVUY8888`, `XVUY2101010`, `AVUY16161616`.
- **YUV semiplanar** — `NV12`, `NV21`, `NV16`, `NV61`, `P030`, `P230`.
- **YUV planar** — `YUV444`, `YVU444`, `YUV422`, `YVU422`, `YUV420`,
  `YVU420`, `T430`.
- **Grayscale** — `Y8`, `Y10`, `Y12`, `Y16`, `XYYY2101010`, `Y10P`,
  `Y12P`.
- **Bayer unpacked** — `SRGGB` / `SBGGR` / `SGRBG` / `SGBRG` at
  8 / 10 / 12 / 16 bit. Reads use a bilinear demosaic.
- **Bayer MIPI-packed** — the same four phases at 10P / 12P.

### Format names and byte order

Format names follow the DRM / kms++ / pixutils convention:
components are listed **MSB-first** inside the storage word. So
`XRGB8888` means a 32-bit word with X in the highest byte and B in
the lowest, and `BGR888` is a 24-bit format with B at the highest
byte and R at byte 0.

OpenCV uses the **opposite** convention — its `BGR` is byte-order, so
OpenCV `BGR` and pixpat `RGB888` describe the same in-memory layout.
Keep this in mind when comparing pipelines.

## Quickstart

### C

```c
#include <pixpat/pixpat.h>

uint8_t pixels[1920 * 1080 * 4];
pixpat_buffer buf = {
    .format     = "XRGB8888",
    .width      = 1920,
    .height     = 1080,
    .num_planes = 1,
    .planes     = { pixels },
    .strides    = { 1920 * 4 },
};

pixpat_draw_pattern(&buf, "smpte", NULL);  /* NULL opts → BT.601 / limited / auto threads */
```

A `pixpat_convert(dst, src, opts)` call has the same shape: two
`pixpat_buffer`s plus a small options struct (also nullable). See the
public header at `pixpat-native/inc/pixpat/pixpat.h`.

### Python

```python
import pixpat

w, h = 1920, 1080
data = bytearray(w * h * 4)
buf = pixpat.Buffer(planes=[data], fmt='XRGB8888', width=w, height=h, strides=[w * 4])

pixpat.draw_pattern(buf, 'smpte')
```

The Python `Buffer` accepts anything that supports the buffer protocol
— `bytearray`, `array.array`, `numpy.ndarray`, `mmap.mmap`,
`memoryview`. Source buffers may be read-only; destination buffers
must be writable.

## Components

- **`libpixpat`** — the C++ implementation, exposed through a small
  C ABI in [`pixpat-native/inc/pixpat/pixpat.h`](pixpat-native/inc/pixpat/pixpat.h).
  Built with Meson; produces both shared and static libraries plus a
  pkg-config file.
- **`pixpat` (Python)** — thin `ctypes` bindings over the C ABI. No
  CPython extension, so a single wheel works on any CPython ≥ 3.9 for
  a given architecture.
- **`pixbench`** — the measurement tooling in
  [`pixpat-bench/`](pixpat-bench/README.md): builds the library from
  any tree or commit, digests every output byte, records throughput
  and size into JSON run files, and compares them across commits,
  compilers and build options.

## Building

`libpixpat` is built with [Meson](https://mesonbuild.com/). A C++20
compiler and Python 3 (used by the build-time codegen) are required;
there are no third-party runtime dependencies.

```sh
meson setup build
meson compile -C build
```

This produces `build/libpixpat.so` (and `.a`), the public header at
`pixpat-native/inc/pixpat/pixpat.h`, and a `pixpat.pc` pkg-config
file. To install system-wide:

```sh
meson install -C build
```

### Cross-compiling

A cross file for aarch64 Linux ships in the tree:

```sh
meson setup build-aarch64 --cross-file pixpat-native/cross/aarch64-linux-gnu.txt
meson compile -C build-aarch64
```

### Native tests

```sh
meson test -C build
```

These are smoke tests that exercise the public ABI from C and C++.
Behavioral coverage (matrix correctness, threading, subsampling,
Bayer demosaic, …) lives in the Python test suite.

### Selecting a build profile

The default profile compiles every format and pattern. To pick a
different one, point Meson at an alternate TOML file via the `config`
option:

```sh
meson setup build-min -Dconfig=pixpat-native/profiles/no_hotpath.toml
```

A few example profiles ship in `pixpat-native/profiles/`. See
[Build-time configuration and codegen](#build-time-configuration-and-codegen)
below for what the TOML controls.

### Python install

The Python package wraps the C ABI via `ctypes`, so installing it
just means compiling `libpixpat.so` and bundling it as package data.
For a native install:

```sh
pip install .
```

`setup.py` invokes meson during the wheel build (into a temporary
directory), copies the resulting `.so` into the package, and stamps the
wheel for the host architecture. Requires `meson`, `ninja`, and a C++
compiler on the host.

To cross-compile a wheel for another architecture, use the helper:

```sh
sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
pixpat-python/scripts/build_wheel.sh aarch64    # or x86_64
```

Beyond the cross toolchain this needs only `python3` and its `venv`
module: the script bootstraps a build environment in
`pixpat-python/build-venv/` and lets PEP 517 build isolation provision
`setuptools`, `meson` and `ninja` from `pyproject.toml`. No `pip
install` into your own environment is required, and the distro's
`setuptools` is not used (`setup.py` needs >= 70.1, which is newer than
some distros ship).

The resulting wheel lands in `dist/`, tagged for the chosen
architecture. Wheel builds are always clean: meson runs in a temporary
directory and nothing is cached between runs, so repeated builds are
reproducible. Only the repo-root `build/` of the editable workflow below
is persistent.

The wheel is tagged `linux_<arch>`, not `manylinux`, so the `.so`
carries whatever glibc symbol versions your cross toolchain emits. If
the target's glibc is older, the wheel installs but fails to load —
check with `readelf -V` before deploying. The `.so` also links
`libstdc++.so.6`, which must be present on the target.

### Editable Python install for development

```sh
meson setup build
meson compile -C build
pip install -e .
```

The editable install symlinks `build/libpixpat.so` into the
package, so rebuilding the native side is picked up without
re-installing.

### Python tests

From the repo root, after an editable install:

```sh
pytest pixpat-python/tests
```

For throughput, library size and bit-exactness measurements across
builds, compilers and commits, see [`pixpat-bench/`](pixpat-bench/README.md)
(`scripts/pixbench`). This is a development tool, not part of the
supported API surface.

## Architecture

The rest of this document covers how `libpixpat` is put together
internally: how a conversion is structured, how formats are
described, how the build is configured, and the supported compiler
and runtime.

### Conversion pipeline

A conversion is the composition of three stages:

```
Source  →  ColorXfm  →  Sink
```

- A **source** unpacks caller-memory pixels into a **normalized
  pixel** — `RGB16` or `YUV16`, four `uint16_t` components.
- A **ColorXfm** maps one normalized pixel type to another: identity
  for same-color-kind conversions, the selected matrix/range for
  cross-color-kind ones. Template-specialized, so the identity case
  vanishes at compile time.
- A **sink** packs normalized pixels into destination memory.

Each sink declares a `block_h × block_w` block matching its chroma
subsampling (1×1 for unsubsampled, e.g. 2×2 for `NV12`). The
converter materializes one block on the stack per iteration; under
`-O3` it stays in registers for most sinks.

#### Hot path vs cold path

The normalized pixel type does double duty:

- On the **hot path**, with `-O3`, the compiler keeps it in registers
  across the source / ColorXfm / sink boundary — no per-line buffer
  is involved.
- On the **cold path**, two short legs — *unpack to norm* and
  *pack from norm* — share a per-thread normalized line buffer.
  Each leg is a templated function: one body per source, one per
  sink. Cross-color-kind conversions add an in-place ColorXfm pass over
  the buffer between the two legs.

Whether a particular conversion runs on the hot or cold path is
decided by the dispatch tier described in [Two-tier
dispatch](#two-tier-dispatch).

### Layout descriptor

A pixel format is described once, declaratively, as a C++20
non-type-template-parameter (NTTP) value. Three small types are
enough to describe any format:

```cpp
enum class C : uint8_t { X, A, R, G, B, Y, U, V };

struct Comp { C c; uint8_t bits; uint8_t shift; };

template <typename Storage, Comp... Cs>
struct Plane;

template <ColorKind Kind, size_t Hsub, size_t Vsub, typename... Planes>
struct Layout;
```

A `Plane` describes one storage word (`uint32_t`, `uint16_t`, …) and
the components packed into it at given bit offsets. A `Layout` lists
the planes, the color kind (RGB or YUV), and the chroma subsampling
factors. Two named formats for comparison:

```cpp
using XRGB8888 = Layout<ColorKind::RGB, 1, 1,
    Plane<uint32_t, Comp{C::B,8,0}, Comp{C::G,8,8},
                    Comp{C::R,8,16}, Comp{C::X,8,24}>>;

using NV12 = Layout<ColorKind::YUV, 2, 2,
    Plane<uint8_t,  Comp{C::Y,8,0}>,
    Plane<uint16_t, Comp{C::U,8,0}, Comp{C::V,8,8}>>;
```

`Plane` exposes `constexpr` helpers — `find_pos<C>`, `pack(values)`,
`unpack(word)`, `bytes_per_pixel`, … — that the I/O templates use to
emit per-format read and write code.

### Patterns

A pattern is a synthetic source: a small C++ struct exposing
`sample(x, y, W, H)` that returns one normalized pixel. The list of
supported names and their parameters is in
[`pixpat-native/inc/pixpat/pixpat.h`](pixpat-native/inc/pixpat/pixpat.h).

Dispatch is intentionally simple: every (pattern, sink) pair takes
one normalized-pivot path. Per-pattern fill writes the normalized
line buffer in the destination's color kind — folding the
cross-color-kind `ColorXfm` into the per-pixel fill so that constant
patterns collapse to `memset` under `-O3`. Per-sink pack encodes the
line into the destination memory layout. Total cost is *O(N + M)*:
adding a pattern is one fill specialization, adding a format is
automatic.

The SMPTE pattern's pixel values are spec-defined in BT.709 /
Limited. Other rec/range settings are accepted but produce
visibly-wrong colors — pixpat does not silently override the caller's
color spec.

`pixpat_pattern_opts::params` is parsed once at the C entry point
into a `Params` instance, then handed to the pattern constructor;
patterns query keys by name and never see raw strings. The Python
wrapper accepts either the wire string or a `Mapping[str, Any]`.

### Two-tier dispatch

The naïve approach — instantiate `Converter<Source, Sink>` for every
source/sink pair — produces an N×N matrix of fully-fused inner loops.
With this many formats the binary balloons quickly. So pixpat splits
the conversion table into two tiers.

**Hot pivot.** A small set of pivot formats gets the fully-fused
treatment. Real-world conversion paths almost always have a common
interchange format on at least one endpoint — OpenCV, Qt,
framebuffers, and video pipelines all gravitate toward a single
8-bit RGB form. Picking that form as the pivot makes the typical
user path fully inlined; the rarer hardware-to-hardware path stays
on the cold path. The default profile uses **`BGR888`** as its
single pivot, the format OpenCV, Qt, and framebuffers all use. Each
pivot covers itself as both source and destination, paired with
every other format.

**Cold path.** Every other source/sink pair walks each row group
through the per-thread normalized line buffer:

```
for each row group:
    unpack_to_norm<Source>(buf, src, ...)
    if RGB ↔ YUV:
        in-place ColorXfm pass over buf
    pack_from_norm<Sink>(dst, buf, ...)
```

`unpack_to_norm<Source>` and `pack_from_norm<Sink>` are plain
templated functions — one body per source, one per sink, not per
pair. With ~2N legs plus two cross-color-kind helpers, the cold path fits
in a small fixed code budget independent of the number of pairs.

Adding a hot pivot is mechanical (one entry in the build config —
see the next section). Whether a pivot is worth the code size
depends on whether real workloads actually use that format on one
endpoint, so the choice is workload-driven.

### Source / sink shapes

Internally the I/O templates are grouped by *iteration shape*. Every
format reuses one of these template shapes, plus its own `Layout`:

- **Packed** (RGB or YUV) — `XRGB8888`, `BGR888`, …
- **Packed-YUV** — `YUYV` group.
- **Semiplanar** — `NV12` group.
- **Multi-pixel semiplanar** — `P030`, `P230`. Sink uses the
  streaming entry point.
- **Planar** — `YUV420`, `YUV444`, …
- **Multi-pixel planar** — `T430`.
- **Gray** — single-component YUV; chroma synthesized at neutral on
  read.
- **Mono RGB** — RGB counterpart of Gray (`R8`); G=B=R synthesized on
  read.
- **Multi-pixel gray** — `XYYY2101010`.
- **Gray MIPI-packed** — `Y10P`, `Y12P`.
- **Bayer** — phase-aware R/G/B selection. Reads use a 3×3 bilinear
  demosaic; edges clamp.
- **Bayer MIPI-packed** — the 10P / 12P byte layout, hand-rolled
  because the bit packing doesn't fit `Plane<Storage, Comp...>`.

Adding a new format usually means one of: writing a new `Layout` and
reusing one of these templates verbatim; adding a new layout shape to
an existing template group; or, rarely, adding a new template group.

### Threading

A worker fan-out helper splits the image into row stripes — one
disjoint `[start, end)` row range per worker — and runs the same
converter body on each stripe. Stripe boundaries are aligned to the
destination's vertical subsampling so chroma blocks aren't split
across workers. Workers are joined before the call returns.

`num_threads = 0` selects a sensible default (one per online CPU,
capped); `1` runs the conversion inline on the calling thread with
no thread-spawn overhead; `N > 1` uses exactly `N` workers.

### Color math and bit depth

Color math runs in `float`. Normalized components are `uint16_t`;
N-bit stored values bit-replicate to 16 bits on decode (so `0xFF →
0xFFFF`) and truncate via `norm >> (16 - N)` on encode. Per-component
decode→encode at the same bit depth is exact; full conversions are
not — they go through float color math and, where bit depths differ,
truncation.

Alpha rules:

- Source without A → `a = 0`.
- Same-color-kind ColorXfm → `a` unchanged.
- Cross-color-kind ColorXfm → `a` reset to `0xFFFF`.
- Sinks with A encode `a`; sinks with X write zero.

### Build-time configuration and codegen

Which formats and patterns are compiled in, which formats are
read-only / write-only, and which formats get the fully-fused
hot-pivot treatment are all decided at build time by a small TOML
config. The default lives at `pixpat-native/profiles/pixpat.toml`; pick
a different one with Meson's `-Dconfig=…` option.

The TOML options:

```toml
hot_pivots         = ["BGR888"]                # which formats get fully-fused arms
patterns           = ["kmstest", "smpte"]      # which patterns to compile in
[features]
pattern            = true                       # toggle the draw-pattern entry point
convert            = true                       # toggle the convert entry point
default_format_caps = "rw"                      # default per-format read/write
[formats]
# RGB888 = "r"          # readable only
# YUV420 = "off"        # not in this build
```

At configure time, a Python codegen step reads the TOML and the format
and pattern catalogs and emits two generated files:

- `pixpat_config.h` — the C-side feature flags.
- `pixpat_caps.inc` — two parallel arrays, `FormatCaps[]` and
  `PatternCaps[]`. Each entry carries booleans like `readable`,
  `writable`, `hot_src`, `hot_dst` (formats) or `enabled` (patterns).

The dispatch code reads those caps inside `if constexpr` guards, so a
disabled format / pattern / hot arm produces no template
instantiation at all — the corresponding code is never generated.
This is how a constrained target shrinks the binary: turn off the
patterns and formats it doesn't need; the rest disappears from the
output.

Three example profiles ship in `pixpat-native/profiles/`
illustrating the knobs (no hot path, pattern-only, hot pivot moved
to a different format).

### Compiler

Hot-path performance depends heavily on the inlined inner loop being
auto-vectorized. CI builds and tests under both gcc and clang; both
work, but performance differs:

- Under clang 18 (built with `-O3 -march=native`) the templated inner
  loops vectorize cleanly, the normalized pixel stays register-resident,
  and constant patterns collapse to `memset`.
- Under gcc 13 the same loops vectorize for some shapes but not others —
  RGB→RGB in particular drops considerably.
