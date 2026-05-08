"""Multi-threaded == single-threaded parity tests.

The chosen ``num_threads`` is supposed to be transparent: the output
bytes must match the single-threaded output exactly. Anything else is
a threading bug.

These tests parametrize across one format per writer template so that
every code path in the dispatcher is covered.
"""

import pytest

import pixpat


# Width must be divisible by 3 (T430/XYYY2101010 pack 3-per-32-bit), 4
# (Bayer 10P packs 4 pixels per group), and 2 (h_sub).
# Height must be divisible by 2 (v_sub for 4:2:0 formats).
#
# Two sizes: a small one for fast format-coverage parametrization, and
# a larger one (~HD-width) where worker-stripe alignment can interact
# with the writers' chroma logic in non-obvious ways.
SMALL = (192, 64)
LARGE = (1920, 64)


def _alloc(fmt, w, h):
    """Return (planes, strides) sized for `fmt` at (w, h)."""
    if fmt in ('NV12', 'NV21'):  # semi-planar 4:2:0
        return [bytearray(w * h), bytearray(w * (h // 2))], [w, w]
    if fmt in ('NV16', 'NV61'):  # semi-planar 4:2:2
        return [bytearray(w * h), bytearray(w * h)], [w, w]
    if fmt == 'P030':  # semi-planar 4:2:0, 10-bit packed 3-per-32
        ys, uvs = (w // 3) * 4, (w // 3 // 2) * 8
        return [bytearray(ys * h), bytearray(uvs * (h // 2))], [ys, uvs]
    if fmt == 'P230':  # semi-planar 4:2:2, 10-bit packed 3-per-32
        ys, uvs = (w // 3) * 4, (w // 3 // 2) * 8
        return [bytearray(ys * h), bytearray(uvs * h)], [ys, uvs]
    if fmt in ('YUV420', 'YVU420'):  # planar 4:2:0
        return (
            [bytearray(w * h), bytearray((w // 2) * (h // 2)), bytearray((w // 2) * (h // 2))],
            [w, w // 2, w // 2],
        )
    if fmt in ('YUV422', 'YVU422'):  # planar 4:2:2
        return (
            [bytearray(w * h), bytearray((w // 2) * h), bytearray((w // 2) * h)],
            [w, w // 2, w // 2],
        )
    if fmt in ('YUV444', 'YVU444'):  # planar 4:4:4
        return [bytearray(w * h)] * 3, [w] * 3
    if fmt == 'T430':  # planar packed 4:4:4, 10-bit
        s = (w // 3) * 4
        return [bytearray(s * h)] * 3, [s] * 3
    if fmt in ('Y8', 'R8', 'RGB332'):
        return [bytearray(w * h)], [w]
    if fmt == 'XYYY2101010':
        s = (w // 3) * 4
        return [bytearray(s * h)], [s]
    if fmt in ('YUYV', 'YVYU', 'UYVY', 'VYUY'):
        return [bytearray(w * h * 2)], [w * 2]
    if fmt in ('Y210', 'Y212', 'Y216'):
        # 4:2:2, two pixels per 64-bit word.
        return [bytearray(w * h * 4)], [w * 4]
    if fmt == 'VUY888':
        # 24-bit packed YUV, 3 bytes per pixel (storage uint32_t).
        return [bytearray(w * h * 3)], [w * 3]
    if fmt in ('XVUY2101010', 'XVUY8888'):
        return [bytearray(w * h * 4)], [w * 4]
    if fmt in ('AVUY16161616', 'ABGR16161616'):
        return [bytearray(w * h * 8)], [w * 8]
    # MIPI CSI-2 byte packing (Bayer SXXXX10P/12P and grayscale Y10P/Y12P).
    if fmt.endswith('10P'):
        s = (w // 4) * 5
        return [bytearray(s * h)], [s]
    if fmt.endswith('12P'):
        s = (w // 2) * 3
        return [bytearray(s * h)], [s]
    # Plain Bayer
    if fmt.startswith('S') and fmt[-1] == '8':
        return [bytearray(w * h)], [w]
    if fmt.startswith('S'):
        return [bytearray(w * h * 2)], [w * 2]
    # 16-bit RGB
    if fmt.endswith('565') or fmt.endswith('1555') or fmt.endswith('4444'):
        return [bytearray(w * h * 2)], [w * 2]
    # 32-bit RGB (8888 / 2101010 / 1010102 / 888 in 32-bit storage)
    return [bytearray(w * h * 4)], [w * 4]


# One format per writer template in pixpat.cpp.
DRAW_FORMATS = [
    'XRGB8888',  # ARGB_Writer
    'RGB565',  # ARGB_Writer (16-bit)
    'ABGR16161616',  # ARGB_Writer (normalized wide)
    'XVUY2101010',  # YUV_Writer
    'AVUY16161616',  # YUV_Writer (normalized wide)
    'YUYV',  # YUVPackedWriter
    'NV12',  # YUVSemiPlanarWriter v_sub=2
    'NV16',  # YUVSemiPlanarWriter v_sub=1
    'P030',  # YUVSemiPlanarWriter v_sub=2, 10-bit packed
    'P230',  # YUVSemiPlanarWriter v_sub=1, 10-bit packed
    'YUV420',  # YUVPlanarWriter v_sub=2
    'YUV422',  # YUVPlanarWriter v_sub=1
    'YUV444',  # YUVPlanarWriter no subsampling
    'T430',  # YUVPlanarPackedWriter
    'Y8',  # Y_Writer (1 sample per byte)
    'XYYY2101010',  # Y_Writer (3 samples per word)
    'Y10P',  # GrayPacked_Writer (CSI-2 byte packing, Y-only)
    'Y210',  # YUVPackedWriter with X-padding entries (4:2:2 in 64-bit word)
    'R8',  # MonoRGB_Writer (single R channel)
    'SRGGB8',  # Bayer_Writer
    'SRGGB10',  # Bayer_Writer
    'SRGGB10P',  # BayerPacked_Writer
    'SRGGB12P',  # BayerPacked_Writer
]

# Convert sources: same minus Bayer (Bayer formats have no read path).
CONVERT_FORMATS = [f for f in DRAW_FORMATS if not f.startswith('S')]


@pytest.mark.parametrize('fmt', DRAW_FORMATS)
@pytest.mark.parametrize(
    'pattern',
    ['smpte', 'kmstest', 'checker', 'hramp', 'vramp', 'dramp', 'zoneplate'],
)
@pytest.mark.parametrize('size', [SMALL, LARGE], ids=['small', 'large'])
def test_draw_pattern_threaded_matches_single(fmt, pattern, size):
    """draw_pattern with num_threads=4 must produce identical bytes
    to num_threads=1 for every supported format."""
    w, h = size
    s_planes, strides = _alloc(fmt, w, h)
    pixpat.draw_pattern(pixpat.Buffer(s_planes, fmt, w, h, strides), pattern, num_threads=1)

    t_planes, _ = _alloc(fmt, w, h)
    pixpat.draw_pattern(pixpat.Buffer(t_planes, fmt, w, h, strides), pattern, num_threads=4)

    for i, (s, t) in enumerate(zip(s_planes, t_planes)):
        assert s == t, f'{fmt} ({pattern}, {w}x{h}): plane {i} differs threaded vs single'


@pytest.mark.parametrize('fmt', ['XRGB8888', 'NV12', 'YUV420', 'BGR888'])
@pytest.mark.parametrize('size', [SMALL, LARGE], ids=['small', 'large'])
def test_draw_pattern_plain_threaded_matches_single(fmt, size):
    """draw_pattern with params= must also be bit-identical across
    thread counts. Spot-check a few representative formats."""
    w, h = size
    params = {'color': '12ab34'}
    s_planes, strides = _alloc(fmt, w, h)
    pixpat.draw_pattern(
        pixpat.Buffer(s_planes, fmt, w, h, strides), 'plain', num_threads=1, params=params
    )

    t_planes, _ = _alloc(fmt, w, h)
    pixpat.draw_pattern(
        pixpat.Buffer(t_planes, fmt, w, h, strides), 'plain', num_threads=4, params=params
    )

    for i, (s, t) in enumerate(zip(s_planes, t_planes)):
        assert s == t, f'{fmt} (plain, {w}x{h}): plane {i} differs threaded vs single'


@pytest.mark.parametrize('src_fmt', CONVERT_FORMATS)
@pytest.mark.parametrize('dst_fmt', ['ABGR16161616', 'AVUY16161616', 'XRGB8888', 'NV12', 'YUV420'])
@pytest.mark.parametrize('size', [SMALL, LARGE], ids=['small', 'large'])
def test_convert_threaded_matches_single(src_fmt, dst_fmt, size):
    """convert with num_threads=4 must produce identical bytes
    to num_threads=1."""
    w, h = size
    src_planes, src_strides = _alloc(src_fmt, w, h)
    pixpat.draw_pattern(pixpat.Buffer(src_planes, src_fmt, w, h, src_strides), 'smpte')

    s_dst_planes, dst_strides = _alloc(dst_fmt, w, h)
    pixpat.convert(
        pixpat.Buffer(s_dst_planes, dst_fmt, w, h, dst_strides),
        pixpat.Buffer(src_planes, src_fmt, w, h, src_strides),
        num_threads=1,
    )

    t_dst_planes, _ = _alloc(dst_fmt, w, h)
    pixpat.convert(
        pixpat.Buffer(t_dst_planes, dst_fmt, w, h, dst_strides),
        pixpat.Buffer(src_planes, src_fmt, w, h, src_strides),
        num_threads=4,
    )

    for i, (s, t) in enumerate(zip(s_dst_planes, t_dst_planes)):
        assert s == t, f'{src_fmt}->{dst_fmt} ({w}x{h}): plane {i} differs threaded vs single'
