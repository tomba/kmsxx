"""Python bindings for libpixpat.

Loads the bundled shared library via ctypes — no CPython extension. Works on
any CPython >= 3.9 for the wheel's target architecture.

The library draws test patterns and converts pixel data directly into
caller-owned pixel buffers, in a wide range of pixel formats
(planar/semi-planar/packed YUV, RGB, raw, …). The caller is responsible for
allocating each plane with the correct size and stride for the chosen format.

Pixel data is described by :class:`Buffer`, a passive struct mirroring the
C ``pixpat_buffer``: format name, width, height, one writable buffer per
plane, and one row stride per plane. Use :func:`draw_pattern` to fill a
buffer with a test pattern, and :func:`convert` to convert pixel data
between formats.

Format names follow the convention used by `kms++` and `pixutils`
(e.g. ``"XRGB8888"``, ``"NV12"``, ``"YUYV"``) — not the DRM/V4L2
four-character codes (``"XR24"``, etc.).

Example:
    >>> import pixpat
    >>> width, height = 1920, 1080
    >>> stride = width * 4  # XRGB8888: 4 bytes per pixel
    >>> data = bytearray(stride * height)
    >>> buf = pixpat.Buffer(planes=[data], fmt="XRGB8888",
    ...                     width=width, height=height, strides=[stride])
    >>> pixpat.draw_pattern(buf, "smpte")

Use :func:`supported_formats` to enumerate the format names accepted by
``Buffer.fmt``, and :func:`is_supported` to test a single format.

Using numpy buffers
-------------------

pixpat does not import numpy and does not depend on it, but any
C-contiguous ``numpy.ndarray`` works as a plane via Python's buffer
protocol. The caller is responsible for matching the array's dtype and
shape to the pixel format and for passing the correct row stride:

    >>> import numpy as np
    >>> arr = np.zeros((height, width, 4), dtype=np.uint8)
    >>> stride = arr.strides[0]               # bytes per row
    >>> buf = pixpat.Buffer(planes=[arr], fmt="XRGB8888",
    ...                     width=width, height=height, strides=[stride])
    >>> pixpat.draw_pattern(buf, "smpte")

For multi-plane formats like ``"NV12"`` pass one ndarray per plane (e.g.
``(h, w)`` uint8 for Y and ``(h//2, w)`` uint8 for the interleaved UV
plane).

If you already hold all planes in a single contiguous buffer — the layout
OpenCV uses for NV12, an ``(h * 3 // 2, w)`` uint8 array with Y on top
and the interleaved UV plane below — slice it into per-plane views and
pass those::

    >>> nv12 = np.zeros((h * 3 // 2, w), dtype=np.uint8)
    >>> y, uv = nv12[:h], nv12[h:].reshape(h // 2, w)
    >>> pixpat.Buffer([y, uv], "NV12", w, h,
    ...               [y.strides[0], uv.strides[0]])

The source side of :func:`convert` accepts read-only buffers (``bytes``,
``arr.view()`` with ``writeable=False``, mmap'd files, …). The
destination side must always be writable.
"""

import ctypes
from dataclasses import dataclass, field
from enum import IntEnum
from typing import Mapping, Optional, Sequence, Union

from ._native import (
    _Buffer,
    _ConvertOpts,
    _PatternOpts,
    _PinnedBuffers,
    _fill_buffer,
    _lib,
)

_VALID_PATTERNS = frozenset(
    {
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
    }
)


class Rec(IntEnum):
    """YCbCr color encoding standard used for YUV output formats.

    Selects the matrix used to convert from internal RGB to YUV. Has no
    effect when drawing into an RGB or raw format.

    Attributes:
        BT601: ITU-R BT.601 (standard definition).
        BT709: ITU-R BT.709 (HD).
        BT2020: ITU-R BT.2020 (UHD / HDR, non-constant luminance).
    """

    BT601 = 0
    BT709 = 1
    BT2020 = 2


class Range(IntEnum):
    """Quantization range used for YUV output formats.

    Attributes:
        LIMITED: "TV" / studio range — Y in [16, 235], C in [16, 240]
            (scaled to bit depth). What most video pipelines expect.
        FULL: "PC" / full range — every component uses the full code range
            (e.g. [0, 255] for 8-bit).
    """

    LIMITED = 0
    FULL = 1


class PixpatError(RuntimeError):
    """Raised when the underlying ``pixpat_*`` call fails.

    Most commonly indicates an unknown format name or buffer dimensions /
    plane layout that the format does not allow (e.g. odd width for a
    horizontally-subsampled YUV format).
    """


@dataclass
class Buffer:
    """Plane data + format + dimensions, mirroring the C ``pixpat_buffer``.

    A passive struct: no allocation, no format knowledge, no
    numpy/cv2 awareness. The caller decides plane shapes and strides
    based on its own format knowledge (e.g. via ``pixutils``).

    Attributes:
        planes: One buffer-protocol object per plane (e.g. ``bytearray``,
            ``array.array``, ``numpy.ndarray``, ``mmap.mmap``,
            ``memoryview``). Up to 4 planes are supported. Each plane
            must hold at least ``strides[i] * plane_height`` bytes,
            where ``plane_height`` is ``height`` for the main plane and
            ``height // vertical_subsampling`` for chroma planes (e.g.
            ``height // 2`` for the UV plane of ``NV12``). For
            destination buffers each plane must be writable; for the
            source side of :func:`convert` read-only objects (such as
            ``bytes``) are accepted.
        fmt: Pixel-format name; see :func:`supported_formats`.
        width: Image width in pixels. Some formats constrain this — for
            chroma-subsampled YUV formats it must be a multiple of the
            horizontal subsampling factor (e.g. 2 for ``NV12``), and
            packed formats add a further multiple from their
            pixel-group size (e.g. 6 for ``P030``, 4 for ``SBGGR10P``).
        height: Image height in pixels. For vertically-subsampled
            formats (e.g. ``NV12``, ``YUV420``) it must be a multiple
            of the vertical subsampling factor.
        strides: Per-plane row stride in bytes. Must have the same
            length as ``planes``. Strides larger than the minimum row
            size are allowed.
    """

    planes: Sequence
    fmt: str
    width: int
    height: int
    strides: Sequence[int] = field(default_factory=list)


def supported_formats() -> list[str]:
    """Return the list of pixel-format names accepted by :class:`Buffer`.

    Format names follow the convention used by `kms++` and `pixutils`
    (e.g. ``"XRGB8888"``, ``"NV12"``, ``"YUYV"``) — not the DRM/V4L2
    four-character codes.
    """
    n = _lib.pixpat_format_count()
    return [_lib.pixpat_format_name(i).decode('ascii') for i in range(n)]


def is_supported(fmt: str) -> bool:
    """Return whether ``fmt`` is a known pixel-format name.

    Equivalent to ``fmt in supported_formats()`` but cheaper — it does not
    materialize the full list.
    """
    return bool(_lib.pixpat_format_supported(fmt.encode('ascii')))


def _serialize_pattern_params(
    params: Optional[Union[str, Mapping[str, object]]],
) -> Optional[bytes]:
    """Encode `params` for the C ABI.

    Accepts a ready-made string (passed through verbatim) or a mapping of
    string keys to stringifiable values, which is serialized to the
    ``"key=val,key=val"`` form pixpat parses on the C side. Returns None
    if there is nothing to pass.
    """
    if params is None:
        return None
    if isinstance(params, str):
        return params.encode('ascii')
    if isinstance(params, Mapping):
        items = []
        for k, v in params.items():
            if not isinstance(k, str):
                raise TypeError(f'pattern params keys must be str, got {type(k).__name__}')
            sv = str(v)
            if ',' in k or '=' in k:
                raise ValueError(f'pattern params key contains , or =: {k!r}')
            if ',' in sv or '=' in sv:
                raise ValueError(f'pattern params value contains , or =: {sv!r}')
            items.append(f'{k}={sv}')
        return ','.join(items).encode('ascii')
    raise TypeError(f'pattern params must be None, str, or a Mapping, got {type(params).__name__}')


def draw_pattern(
    dst: Buffer,
    pattern: Optional[str] = None,
    *,
    rec: Rec = Rec.BT601,
    color_range: Range = Range.LIMITED,
    num_threads: int = 0,
    params: Optional[Union[str, Mapping[str, object]]] = None,
) -> None:
    """Draw a test pattern into ``dst``.

    Args:
        dst: Destination buffer; all planes must be writable.
        pattern: Name of the pattern to draw. ``None`` selects the default
            (equivalent to ``"kmstest"``). Recognized values:

            * ``"kmstest"`` — color gradients with ramps, in the style of
              the original ``kmstest`` tool. The default.
            * ``"smpte"`` — SMPTE RP 219-1 color bars (with PLUGE).
            * ``"plain"`` — solid color fill from ``params["color"]``.
              See `params` below.
            * ``"checker"`` — black/white checkerboard. Optional
              ``params["cell"]`` (decimal positive integer, default 8)
              sets the cell size in pixels. ``"cell": "1"`` is a
              1-pixel chroma-subsampling stress test.
            * ``"hramp"`` — four horizontal stripes (R, G, B, gray),
              each a 0..max ramp along x. Combined per-channel and
              luma quantization check.
            * ``"vramp"`` — same as ``"hramp"`` rotated 90°: four
              vertical columns ramping along y.
            * ``"hbar"`` — horizontal bar (full image width, narrow
              along y) over a black background. Required
              ``params["pos"]`` (signed integer, top edge in pixels);
              optional ``params["width"]`` (positive integer, default
              32). The bar is split into seven equal-width regions
              colored white/red/white/green/white/blue/white.
            * ``"vbar"`` — same as ``"hbar"`` rotated 90°: vertical
              bar with ``pos`` measured along x.
            * ``"dramp"`` — diagonal RGB ramp (R on x, G on y,
              B on x+y).
            * ``"zoneplate"`` — centered radial cosine pattern,
              frequency ramping from DC at the center to Nyquist at
              the longer edge. Useful for spotting scaling/aliasing.

            ``"smpte"`` is defined by the spec in BT.709 / Limited.
            Pass ``rec=Rec.BT709, color_range=Range.LIMITED`` for
            spec-correct output; other settings produce visibly-wrong
            colors when drawing into RGB sinks (the caller's matrix is
            applied to BT.709-encoded values). Callers are trusted —
            pixpat does not silently override the spec.
        rec: YCbCr matrix for YUV formats. Ignored for RGB / raw formats.
            Defaults to :attr:`Rec.BT601`.
        color_range: Quantization range for YUV formats. Ignored for
            RGB / raw formats. Defaults to :attr:`Range.LIMITED`.
        num_threads: Worker-thread count. ``0`` selects a sensible
            default (one per online CPU, capped to a sane maximum);
            ``1`` runs single-threaded with no thread-spawn overhead;
            ``N > 1`` uses exactly ``N`` workers. Defaults to ``0``.
            Output is bit-identical regardless of the chosen count.
        params: Optional pattern-specific parameters. Either a mapping
            (``{"color": "ff0000"}``) — serialized to ``"color=ff0000"``
            for the C ABI — or a raw ``"key=val,key=val"`` string.
            Unknown keys are silently ignored; patterns that don't read
            params (``kmstest``, ``smpte``) ignore this entirely.
            Per-pattern keys:

            * ``"plain"`` reads ``"color"`` as a hex RGB(A) string with
              an optional ``"0x"`` prefix. The hex-digit count selects
              the layout: 6 → 8-bit ``RRGGBB``, 8 → 8-bit ``AARRGGBB``
              (alpha first), 12 → 16-bit ``RRRRGGGGBBBB``, 16 → 16-bit
              ``AAAARRRRGGGGBBBB``. Missing or malformed ``"color"``
              raises :class:`PixpatError`.
            * ``"checker"`` reads optional ``"cell"`` as a positive
              decimal integer; default 8. A non-positive or non-numeric
              value raises :class:`PixpatError`.
            * ``"hbar"`` / ``"vbar"`` read required ``"pos"`` (signed
              decimal integer, top/left edge of the bar in pixels;
              negative values clip at the edge) and optional ``"width"``
              (positive decimal integer, bar thickness; default 32).
              Missing/non-numeric ``pos`` or non-positive ``width``
              raises :class:`PixpatError`.

    Raises:
        ValueError: ``dst.planes`` exceeds the maximum plane count,
            ``dst.strides`` length does not match ``dst.planes``,
            ``pattern`` is not one of the recognized names, or a
            ``params`` mapping value contains a forbidden ``,`` or ``=``.
        TypeError: A plane is read-only (e.g. ``bytes``, a read-only
            ``memoryview``), or ``params`` has an unsupported type.
        PixpatError: The underlying C call failed — typically an unknown
            ``fmt``, dimensions / strides incompatible with the format,
            or pattern parameters that the pattern rejected.

    Example:
        >>> w, h = 640, 480
        >>> stride = w * 4
        >>> data = bytearray(stride * h)
        >>> dst = Buffer([data], "XRGB8888", w, h, [stride])
        >>> draw_pattern(dst, "smpte")
        >>> draw_pattern(dst, "plain", params={"color": "ff0000"})
    """
    if pattern is None:
        pattern = 'kmstest'
    elif pattern not in _VALID_PATTERNS:
        raise ValueError(
            f'unknown pattern {pattern!r}; expected one of {sorted(_VALID_PATTERNS)} or None'
        )
    if num_threads < 0:
        raise ValueError(f'num_threads must be >= 0, got {num_threads}')

    params_bytes = _serialize_pattern_params(params)

    c_buf = _Buffer()
    opts = _PatternOpts()
    opts.rec = int(rec)
    opts.range = int(color_range)
    opts.num_threads = num_threads
    opts.params = params_bytes

    with _PinnedBuffers() as pins:
        _fill_buffer(
            c_buf,
            pins,
            dst.planes,
            dst.fmt,
            dst.width,
            dst.height,
            dst.strides,
            writable=True,
        )
        rc = _lib.pixpat_draw_pattern(
            ctypes.byref(c_buf), pattern.encode('ascii'), ctypes.byref(opts)
        )

    if rc != 0:
        raise PixpatError(f'pixpat_draw_pattern failed (rc={rc}); check format name and dimensions')


def convert(
    dst: Buffer,
    src: Buffer,
    *,
    rec: Rec = Rec.BT601,
    color_range: Range = Range.LIMITED,
    num_threads: int = 0,
) -> None:
    """Convert pixel data from ``src`` into ``dst``.

    Both buffers must describe an image of the same width and height;
    only the pixel format may differ. Any format accepted by
    :func:`supported_formats` works as both source and destination in
    the default build (custom build profiles can mark individual formats
    read-only or write-only). Conversion is routed internally through a
    16-bit normalized RGB or YUV intermediate, so format-to-format
    conversions in either direction (e.g. ``NV12`` -> ``YUV420``,
    ``XRGB8888`` -> ``NV12``, ``SRGGB10`` -> ``BGR888``) are a single
    call. Bayer sources are decoded with a 3x3 bilinear demosaic.

    ``src.planes`` may hold read-only buffers (e.g. ``bytes``, mmap'd
    files, numpy arrays with ``writeable=False``); ``dst.planes`` must
    be writable.

    Args:
        dst: Destination buffer.
        src: Source buffer.
        rec: YCbCr matrix used when the conversion crosses the RGB/YUV
            boundary. Ignored when ``src.fmt`` and ``dst.fmt`` share
            the same color kind. Defaults to :attr:`Rec.BT601`.
        color_range: Quantization range used when the conversion crosses
            the RGB/YUV boundary. Ignored when ``src.fmt`` and
            ``dst.fmt`` share the same color kind. Defaults to
            :attr:`Range.LIMITED`.
        num_threads: Worker-thread count. ``0`` selects a sensible
            default (one per online CPU, capped to a sane maximum);
            ``1`` runs single-threaded with no thread-spawn overhead;
            ``N > 1`` uses exactly ``N`` workers. Defaults to ``0``.
            Output is bit-identical regardless of the chosen count.

    Raises:
        ValueError: ``dst`` and ``src`` have mismatched dimensions, a
            plane sequence exceeds the maximum plane count, or a
            strides length does not match its planes length.
        TypeError: A ``dst`` plane is read-only.
        PixpatError: The underlying C call failed — typically an
            unknown format name, a format disabled in the current build
            (or disabled in the requested direction), or dimensions /
            strides incompatible with one of the formats.

    Example:
        Cross-color-kind, multi-plane on both sides — paint an NV12
        source via :func:`draw_pattern`, then convert it to planar
        YUV420:

        >>> w, h = 64, 32
        >>> y_src = bytearray(w * h)
        >>> uv_src = bytearray(w * h // 2)
        >>> draw_pattern(Buffer([y_src, uv_src], "NV12", w, h, [w, w]),
        ...              "smpte")
        >>> y_dst = bytearray(w * h)
        >>> u_dst = bytearray(w * h // 4)
        >>> v_dst = bytearray(w * h // 4)
        >>> convert(
        ...     Buffer([y_dst, u_dst, v_dst], "YUV420", w, h,
        ...            [w, w // 2, w // 2]),
        ...     Buffer([y_src, uv_src], "NV12", w, h, [w, w]),
        ... )
    """
    if dst.width != src.width or dst.height != src.height:
        raise ValueError(
            f'dst dimensions {dst.width}x{dst.height} do not match '
            f'src dimensions {src.width}x{src.height}'
        )
    if num_threads < 0:
        raise ValueError(f'num_threads must be >= 0, got {num_threads}')

    c_dst = _Buffer()
    c_src = _Buffer()
    opts = _ConvertOpts()
    opts.rec = int(rec)
    opts.range = int(color_range)
    opts.num_threads = num_threads

    with _PinnedBuffers() as pins:
        _fill_buffer(
            c_dst,
            pins,
            dst.planes,
            dst.fmt,
            dst.width,
            dst.height,
            dst.strides,
            writable=True,
            role='dst',
        )
        _fill_buffer(
            c_src,
            pins,
            src.planes,
            src.fmt,
            src.width,
            src.height,
            src.strides,
            writable=False,
            role='src',
        )
        rc = _lib.pixpat_convert(ctypes.byref(c_dst), ctypes.byref(c_src), ctypes.byref(opts))

    if rc != 0:
        raise PixpatError(
            f'pixpat_convert failed (rc={rc}); check format names, dimensions, '
            f'and that src.fmt is a supported source format'
        )


__all__ = [
    'Buffer',
    'Rec',
    'Range',
    'PixpatError',
    'supported_formats',
    'is_supported',
    'draw_pattern',
    'convert',
]
