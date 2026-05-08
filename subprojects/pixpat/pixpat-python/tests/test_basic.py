"""Smoke tests for the pixpat Python bindings."""

import pytest

import pixpat


def test_supported_formats_nonempty():
    formats = pixpat.supported_formats()
    assert isinstance(formats, list)
    assert len(formats) > 0
    assert all(isinstance(f, str) for f in formats)
    assert 'XRGB8888' in formats
    assert 'NV12' in formats


def test_is_supported():
    assert pixpat.is_supported('XRGB8888')
    assert pixpat.is_supported('NV12')
    assert not pixpat.is_supported('NOT_A_REAL_FORMAT')


def test_draw_pattern_xrgb8888():
    w, h = 64, 32
    stride = w * 4
    data = bytearray(stride * h)
    buf = pixpat.Buffer([data], 'XRGB8888', w, h, [stride])
    pixpat.draw_pattern(buf, 'smpte')
    assert any(b != 0 for b in data)


def test_draw_pattern_unknown_pattern_raises():
    data = bytearray(64 * 32 * 4)
    buf = pixpat.Buffer([data], 'XRGB8888', 64, 32, [64 * 4])
    with pytest.raises(ValueError):
        pixpat.draw_pattern(buf, 'bogus')


def test_draw_pattern_unknown_format_raises():
    data = bytearray(64 * 32 * 4)
    buf = pixpat.Buffer([data], 'NOT_A_REAL_FORMAT', 64, 32, [64 * 4])
    with pytest.raises(pixpat.PixpatError):
        pixpat.draw_pattern(buf)


def test_draw_pattern_readonly_buffer_raises():
    data = bytes(64 * 32 * 4)
    buf = pixpat.Buffer([data], 'XRGB8888', 64, 32, [64 * 4])
    with pytest.raises((TypeError, BufferError)):
        pixpat.draw_pattern(buf)


def _alloc_xrgb(w, h):
    stride = w * 4
    return bytearray(stride * h), stride


def _first_pixel_bgr(data):
    # BGR888-style byte order: pixpat XRGB8888 stores B at byte 0,
    # G at byte 1, R at byte 2, X at byte 3. See
    # README "Format names and byte order".
    return data[0], data[1], data[2]


def test_draw_pattern_plain_red_str_params():
    w, h = 32, 16
    data, stride = _alloc_xrgb(w, h)
    buf = pixpat.Buffer([data], 'XRGB8888', w, h, [stride])
    pixpat.draw_pattern(buf, 'plain', params='color=ff0000')
    b, g, r = _first_pixel_bgr(data)
    assert (b, g, r) == (0, 0, 0xFF)


def test_draw_pattern_plain_red_dict_params():
    w, h = 32, 16
    data, stride = _alloc_xrgb(w, h)
    buf = pixpat.Buffer([data], 'XRGB8888', w, h, [stride])
    pixpat.draw_pattern(buf, 'plain', params={'color': 'ff0000'})
    b, g, r = _first_pixel_bgr(data)
    assert (b, g, r) == (0, 0, 0xFF)


def test_draw_pattern_plain_alpha_passes_through_to_argb():
    w, h = 16, 8
    stride = w * 4
    data = bytearray(stride * h)
    buf = pixpat.Buffer([data], 'ARGB8888', w, h, [stride])
    # 8-char form is alpha-first: AARRGGBB.
    pixpat.draw_pattern(buf, 'plain', params={'color': '80112233'})
    # ARGB8888 byte order (LSB-first): B, G, R, A
    assert (data[0], data[1], data[2], data[3]) == (0x33, 0x22, 0x11, 0x80)


def test_draw_pattern_plain_accepts_0x_prefix():
    w, h = 16, 8
    stride = w * 4
    data = bytearray(stride * h)
    buf = pixpat.Buffer([data], 'XRGB8888', w, h, [stride])
    pixpat.draw_pattern(buf, 'plain', params={'color': '0xff0000'})
    assert (data[0], data[1], data[2]) == (0, 0, 0xFF)


def test_draw_pattern_plain_16bpc_rgb():
    """16-bpc form: 12 hex chars, RRRRGGGGBBBB. ABGR16161616 names
    A-B-G-R MSB-first in a 64-bit word; stored little-endian, the
    bytes are R lo, R hi, G lo, G hi, B lo, B hi, A lo, A hi."""
    w, h = 4, 2
    stride = w * 8
    data = bytearray(stride * h)
    buf = pixpat.Buffer([data], 'ABGR16161616', w, h, [stride])
    pixpat.draw_pattern(buf, 'plain', params={'color': '0xfedc00000000'})
    # R=0xFEDC, G=0x0000, B=0x0000, A=0xFFFF (default opaque).
    pix = bytes(data[:8])
    assert pix == b'\xdc\xfe\x00\x00\x00\x00\xff\xff'


def test_draw_pattern_plain_16bpc_argb():
    w, h = 4, 2
    stride = w * 8
    data = bytearray(stride * h)
    buf = pixpat.Buffer([data], 'ABGR16161616', w, h, [stride])
    # 16-char form is alpha-first: AAAARRRRGGGGBBBB.
    pixpat.draw_pattern(buf, 'plain', params={'color': '0x80007f000000ffff'})
    pix = bytes(data[:8])
    # R=0x7F00, G=0x0000, B=0xFFFF, A=0x8000.
    assert pix == b'\x00\x7f\x00\x00\xff\xff\x00\x80'


def test_draw_pattern_plain_missing_color_raises():
    w, h = 32, 16
    data, stride = _alloc_xrgb(w, h)
    buf = pixpat.Buffer([data], 'XRGB8888', w, h, [stride])
    with pytest.raises(pixpat.PixpatError):
        pixpat.draw_pattern(buf, 'plain')
    with pytest.raises(pixpat.PixpatError):
        pixpat.draw_pattern(buf, 'plain', params='')


def test_draw_pattern_plain_malformed_color_raises():
    w, h = 32, 16
    data, stride = _alloc_xrgb(w, h)
    buf = pixpat.Buffer([data], 'XRGB8888', w, h, [stride])
    for bad in ('color=zzzzzz', 'color=abc', 'color=', 'color=1234567'):
        with pytest.raises(pixpat.PixpatError):
            pixpat.draw_pattern(buf, 'plain', params=bad)


def test_draw_pattern_malformed_params_string_raises():
    """Top-level params parse failure (not pattern-specific)."""
    w, h = 32, 16
    data, stride = _alloc_xrgb(w, h)
    buf = pixpat.Buffer([data], 'XRGB8888', w, h, [stride])
    for bad in ('foo,bar', '=ff0000', ',color=ff0000'):
        with pytest.raises(pixpat.PixpatError):
            pixpat.draw_pattern(buf, 'plain', params=bad)


def test_draw_pattern_dict_params_with_separators_rejected_by_python():
    w, h = 32, 16
    data, stride = _alloc_xrgb(w, h)
    buf = pixpat.Buffer([data], 'XRGB8888', w, h, [stride])
    with pytest.raises(ValueError):
        pixpat.draw_pattern(buf, 'plain', params={'color': 'ff,00,00'})
    with pytest.raises(ValueError):
        pixpat.draw_pattern(buf, 'plain', params={'col=or': 'ff0000'})


def test_draw_pattern_unknown_params_keys_ignored():
    """Patterns silently ignore keys they don't read; unknown keys
    alongside the recognised one still let the call succeed."""
    w, h = 32, 16
    data, stride = _alloc_xrgb(w, h)
    buf = pixpat.Buffer([data], 'XRGB8888', w, h, [stride])
    pixpat.draw_pattern(buf, 'plain', params={'color': 'ff0000', 'unknown_key': '42'})
    b, g, r = _first_pixel_bgr(data)
    assert (b, g, r) == (0, 0, 0xFF)


def test_draw_pattern_params_ignored_by_kmstest():
    """Patterns that don't read params accept arbitrary params strings."""
    w, h = 32, 16
    data, stride = _alloc_xrgb(w, h)
    buf = pixpat.Buffer([data], 'XRGB8888', w, h, [stride])
    pixpat.draw_pattern(buf, 'kmstest', params={'whatever': 'foo'})
    assert any(b != 0 for b in data)


def _xrgb_pixel_at(data, stride, x, y):
    off = y * stride + x * 4
    return data[off], data[off + 1], data[off + 2]  # B, G, R


@pytest.mark.parametrize(
    'pattern,extra',
    [
        ('hramp', {}),
        ('vramp', {}),
        ('dramp', {}),
        ('zoneplate', {}),
        ('checker', {}),
        ('checker', {'params': 'cell=1'}),
        ('checker', {'params': {'cell': '32'}}),
    ],
)
def test_draw_pattern_phase3_smoke(pattern, extra):
    w, h = 64, 32
    data, stride = _alloc_xrgb(w, h)
    buf = pixpat.Buffer([data], 'XRGB8888', w, h, [stride])
    pixpat.draw_pattern(buf, pattern, **extra)
    assert any(b != 0 for b in data)


def test_draw_pattern_checker_default_cell_first_pixel_white():
    w, h = 64, 32
    data, stride = _alloc_xrgb(w, h)
    buf = pixpat.Buffer([data], 'XRGB8888', w, h, [stride])
    pixpat.draw_pattern(buf, 'checker')
    # (0,0) is in the first cell -> white.
    assert _xrgb_pixel_at(data, stride, 0, 0) == (0xFF, 0xFF, 0xFF)
    # Default cell=8: (8,0) is the next cell horizontally -> black.
    assert _xrgb_pixel_at(data, stride, 8, 0) == (0, 0, 0)


def test_draw_pattern_checker_cell1_alternates():
    w, h = 16, 8
    data, stride = _alloc_xrgb(w, h)
    buf = pixpat.Buffer([data], 'XRGB8888', w, h, [stride])
    pixpat.draw_pattern(buf, 'checker', params={'cell': '1'})
    p00 = _xrgb_pixel_at(data, stride, 0, 0)
    p10 = _xrgb_pixel_at(data, stride, 1, 0)
    p01 = _xrgb_pixel_at(data, stride, 0, 1)
    p11 = _xrgb_pixel_at(data, stride, 1, 1)
    assert p00 != p10
    assert p00 != p01
    assert p00 == p11  # diagonal repeats


@pytest.mark.parametrize('cell', ['0', '-1', 'oops', '1.5'])
def test_draw_pattern_checker_invalid_cell_raises(cell):
    w, h = 16, 8
    data, stride = _alloc_xrgb(w, h)
    buf = pixpat.Buffer([data], 'XRGB8888', w, h, [stride])
    with pytest.raises(pixpat.PixpatError):
        pixpat.draw_pattern(buf, 'checker', params={'cell': cell})


def test_draw_pattern_hramp_stripes():
    """hramp: 4 horizontal stripes (R, G, B, gray), each ramping along x.
    The right edge (x=W-1) hits 0xFF in the active channel(s)."""
    w, h = 32, 16  # h=16 → stripes at rows [0,3], [4,7], [8,11], [12,15]
    data, stride = _alloc_xrgb(w, h)
    buf = pixpat.Buffer([data], 'XRGB8888', w, h, [stride])
    pixpat.draw_pattern(buf, 'hramp')
    assert _xrgb_pixel_at(data, stride, w - 1, 0) == (0, 0, 0xFF)  # R
    assert _xrgb_pixel_at(data, stride, w - 1, 4) == (0, 0xFF, 0)  # G
    assert _xrgb_pixel_at(data, stride, w - 1, 8) == (0xFF, 0, 0)  # B
    assert _xrgb_pixel_at(data, stride, w - 1, h - 1) == (0xFF, 0xFF, 0xFF)  # gray
    # Left edge is the 0 end of every ramp.
    for y in (0, 4, 8, 12):
        assert _xrgb_pixel_at(data, stride, 0, y) == (0, 0, 0)


def test_draw_pattern_vramp_columns():
    """vramp: 4 vertical columns (R, G, B, gray), each ramping along y."""
    w, h = 16, 32
    data, stride = _alloc_xrgb(w, h)
    buf = pixpat.Buffer([data], 'XRGB8888', w, h, [stride])
    pixpat.draw_pattern(buf, 'vramp')
    # Bottom row hits max in the active channel(s).
    assert _xrgb_pixel_at(data, stride, 0, h - 1) == (0, 0, 0xFF)  # R
    assert _xrgb_pixel_at(data, stride, 4, h - 1) == (0, 0xFF, 0)  # G
    assert _xrgb_pixel_at(data, stride, 8, h - 1) == (0xFF, 0, 0)  # B
    assert _xrgb_pixel_at(data, stride, 12, h - 1) == (0xFF, 0xFF, 0xFF)  # gray
    # Top row is the 0 end.
    for x in (0, 4, 8, 12):
        assert _xrgb_pixel_at(data, stride, x, 0) == (0, 0, 0)
    # Within a column, luma is monotonic.
    prev = -1
    for y in range(h):
        b, g, r = _xrgb_pixel_at(data, stride, 12, y)  # gray column
        assert b == g == r
        assert b >= prev
        prev = b


def test_draw_pattern_zoneplate_center_white():
    """Even sizes don't sample exactly at the center; pick odd dimensions
    so (W//2, H//2) is the center pixel where cos(0)=1 → white."""
    w, h = 65, 33
    data, stride = _alloc_xrgb(w, h)
    buf = pixpat.Buffer([data], 'XRGB8888', w, h, [stride])
    pixpat.draw_pattern(buf, 'zoneplate')
    b, g, r = _xrgb_pixel_at(data, stride, w // 2, h // 2)
    # Center pixel: phase=0, gray ≈ 1.0.
    assert b == g == r and b >= 0xFE


def test_convert_roundtrip_xrgb8888():
    w, h = 64, 32
    src = bytearray(w * h * 4)
    mid = bytearray(w * h * 8)
    dst = bytearray(w * h * 4)
    pixpat.draw_pattern(pixpat.Buffer([src], 'XRGB8888', w, h, [w * 4]), 'smpte')
    pixpat.convert(
        pixpat.Buffer([mid], 'ABGR16161616', w, h, [w * 8]),
        pixpat.Buffer([src], 'XRGB8888', w, h, [w * 4]),
    )
    pixpat.convert(
        pixpat.Buffer([dst], 'XRGB8888', w, h, [w * 4]),
        pixpat.Buffer([mid], 'ABGR16161616', w, h, [w * 8]),
    )
    assert src == dst


def test_convert_readonly_bytes_src():
    """Source planes may be read-only (bytes); the C library only reads src."""
    w, h = 64, 32
    src_writable = bytearray(w * h * 4)
    pixpat.draw_pattern(pixpat.Buffer([src_writable], 'XRGB8888', w, h, [w * 4]), 'smpte')
    src_readonly = bytes(src_writable)
    dst = bytearray(w * h * 8)
    pixpat.convert(
        pixpat.Buffer([dst], 'ABGR16161616', w, h, [w * 8]),
        pixpat.Buffer([src_readonly], 'XRGB8888', w, h, [w * 4]),
    )
    assert any(b != 0 for b in dst)


def test_convert_dimension_mismatch_raises():
    w, h = 64, 32
    src = bytearray(w * h * 4)
    dst = bytearray((w * 2) * h * 8)
    with pytest.raises(ValueError, match='dimensions'):
        pixpat.convert(
            pixpat.Buffer([dst], 'ABGR16161616', w * 2, h, [w * 2 * 8]),
            pixpat.Buffer([src], 'XRGB8888', w, h, [w * 4]),
        )


def test_draw_pattern_nv12_semi_planar():
    """Semi-planar NV12: separate Y and interleaved-UV planes."""
    w, h = 64, 32
    y = bytearray(w * h)
    uv = bytearray(w * (h // 2))
    pixpat.draw_pattern(pixpat.Buffer([y, uv], 'NV12', w, h, [w, w]), 'smpte')
    assert any(b != 0 for b in y)
    assert any(b != 0 for b in uv)


def test_draw_pattern_yuv420_three_planes():
    """Fully planar YUV420: Y, U and V all at separate plane pointers."""
    w, h = 64, 32
    yp = bytearray(w * h)
    up = bytearray((w // 2) * (h // 2))
    vp = bytearray((w // 2) * (h // 2))
    pixpat.draw_pattern(
        pixpat.Buffer([yp, up, vp], 'YUV420', w, h, [w, w // 2, w // 2]),
        'smpte',
    )
    assert any(b != 0 for b in yp)
    assert any(b != 0 for b in up)
    assert any(b != 0 for b in vp)


def test_convert_xrgb_to_nv12_writes_both_planes():
    """Convert into a multi-plane destination must populate every plane."""
    w, h = 64, 32
    src = bytearray(w * h * 4)
    pixpat.draw_pattern(pixpat.Buffer([src], 'XRGB8888', w, h, [w * 4]), 'smpte')
    y = bytearray(w * h)
    uv = bytearray(w * (h // 2))
    pixpat.convert(
        pixpat.Buffer([y, uv], 'NV12', w, h, [w, w]),
        pixpat.Buffer([src], 'XRGB8888', w, h, [w * 4]),
    )
    assert any(b != 0 for b in y)
    assert any(b != 0 for b in uv)


def test_convert_yuv420_src_three_planes():
    """Planar YUV is a supported convert source — the C library reads
    all three plane pointers, so make sure the binding wires them up."""
    w, h = 64, 32
    yp = bytearray(w * h)
    up = bytearray((w // 2) * (h // 2))
    vp = bytearray((w // 2) * (h // 2))
    pixpat.draw_pattern(
        pixpat.Buffer([yp, up, vp], 'YUV420', w, h, [w, w // 2, w // 2]),
        'smpte',
    )
    dst = bytearray(w * h * 4)
    pixpat.convert(
        pixpat.Buffer([dst], 'XRGB8888', w, h, [w * 4]),
        pixpat.Buffer([yp, up, vp], 'YUV420', w, h, [w, w // 2, w // 2]),
    )
    assert any(b != 0 for b in dst)


def test_convert_bayer_src():
    """Bayer formats are supported as a convert source (decoded by
    nearest-neighbor demosaic per the public header docstring)."""
    w, h = 64, 32
    src = bytearray(w * h)
    pixpat.draw_pattern(pixpat.Buffer([src], 'SRGGB8', w, h, [w]), 'smpte')
    dst = bytearray(w * h * 4)
    pixpat.convert(
        pixpat.Buffer([dst], 'XRGB8888', w, h, [w * 4]),
        pixpat.Buffer([src], 'SRGGB8', w, h, [w]),
    )
    assert any(b != 0 for b in dst)


def test_convert_roundtrip_yuyv():
    """YUYV (packed 4:2:2) — chroma is averaged on write and replicated on
    read, so the second leg must not change the buffer."""
    w, h = 64, 32
    src = bytearray(w * h * 2)
    mid = bytearray(w * h * 8)
    dst = bytearray(w * h * 2)
    pixpat.draw_pattern(pixpat.Buffer([src], 'YUYV', w, h, [w * 2]), 'smpte')
    pixpat.convert(
        pixpat.Buffer([mid], 'AVUY16161616', w, h, [w * 8]),
        pixpat.Buffer([src], 'YUYV', w, h, [w * 2]),
    )
    pixpat.convert(
        pixpat.Buffer([dst], 'YUYV', w, h, [w * 2]),
        pixpat.Buffer([mid], 'AVUY16161616', w, h, [w * 8]),
    )
    assert src == dst


def test_convert_roundtrip_y8():
    """Y8 grayscale — chroma is discarded on write and synthesized on read,
    Y values themselves should roundtrip exactly."""
    w, h = 64, 32
    src = bytearray(w * h)
    mid = bytearray(w * h * 8)
    dst = bytearray(w * h)
    pixpat.draw_pattern(pixpat.Buffer([src], 'Y8', w, h, [w]), 'smpte')
    pixpat.convert(
        pixpat.Buffer([mid], 'AVUY16161616', w, h, [w * 8]),
        pixpat.Buffer([src], 'Y8', w, h, [w]),
    )
    pixpat.convert(
        pixpat.Buffer([dst], 'Y8', w, h, [w]),
        pixpat.Buffer([mid], 'AVUY16161616', w, h, [w * 8]),
    )
    assert src == dst


def test_convert_roundtrip_nv12():
    """NV12 semi-planar 4:2:0 — chroma averaged into a 2x2 block on write,
    replicated on read; second leg is a no-op."""
    w, h = 64, 32
    y_src = bytearray(w * h)
    uv_src = bytearray(w * (h // 2))
    mid = bytearray(w * h * 8)
    y_dst = bytearray(w * h)
    uv_dst = bytearray(w * (h // 2))
    pixpat.draw_pattern(pixpat.Buffer([y_src, uv_src], 'NV12', w, h, [w, w]), 'smpte')
    pixpat.convert(
        pixpat.Buffer([mid], 'AVUY16161616', w, h, [w * 8]),
        pixpat.Buffer([y_src, uv_src], 'NV12', w, h, [w, w]),
    )
    pixpat.convert(
        pixpat.Buffer([y_dst, uv_dst], 'NV12', w, h, [w, w]),
        pixpat.Buffer([mid], 'AVUY16161616', w, h, [w * 8]),
    )
    assert y_src == y_dst
    assert uv_src == uv_dst


def test_convert_roundtrip_xv15():
    """P030 semi-planar 4:2:0, 10 bits packed 3-per-32-bit. Width and height
    chosen so the Y plane fits 3 samples per word and chroma is on a 2x2 grid."""
    w, h = 96, 32  # multiples of 3 (Y group) and 2 (v_sub)
    y_words = (w // 3) * h
    uv_words = (w // 3 // 2) * (h // 2)
    y_src = bytearray(y_words * 4)
    uv_src = bytearray(uv_words * 8)
    mid = bytearray(w * h * 8)
    y_dst = bytearray(y_words * 4)
    uv_dst = bytearray(uv_words * 8)
    pixpat.draw_pattern(
        pixpat.Buffer([y_src, uv_src], 'P030', w, h, [(w // 3) * 4, (w // 3 // 2) * 8]),
        'smpte',
    )
    pixpat.convert(
        pixpat.Buffer([mid], 'AVUY16161616', w, h, [w * 8]),
        pixpat.Buffer([y_src, uv_src], 'P030', w, h, [(w // 3) * 4, (w // 3 // 2) * 8]),
    )
    pixpat.convert(
        pixpat.Buffer([y_dst, uv_dst], 'P030', w, h, [(w // 3) * 4, (w // 3 // 2) * 8]),
        pixpat.Buffer([mid], 'AVUY16161616', w, h, [w * 8]),
    )
    assert y_src == y_dst
    assert uv_src == uv_dst


def test_convert_roundtrip_x403():
    """T430 fully-planar 4:4:4 with 3-samples-per-32-bit packing on each
    plane; no chroma subsampling so first leg is already lossless."""
    w, h = 96, 32  # width must be a multiple of 3
    plane_words = (w // 3) * h
    y_src = bytearray(plane_words * 4)
    cb_src = bytearray(plane_words * 4)
    cr_src = bytearray(plane_words * 4)
    mid = bytearray(w * h * 8)
    y_dst = bytearray(plane_words * 4)
    cb_dst = bytearray(plane_words * 4)
    cr_dst = bytearray(plane_words * 4)
    strides = [(w // 3) * 4] * 3
    pixpat.draw_pattern(
        pixpat.Buffer([y_src, cb_src, cr_src], 'T430', w, h, strides),
        'smpte',
    )
    pixpat.convert(
        pixpat.Buffer([mid], 'AVUY16161616', w, h, [w * 8]),
        pixpat.Buffer([y_src, cb_src, cr_src], 'T430', w, h, strides),
    )
    pixpat.convert(
        pixpat.Buffer([y_dst, cb_dst, cr_dst], 'T430', w, h, strides),
        pixpat.Buffer([mid], 'AVUY16161616', w, h, [w * 8]),
    )
    assert y_src == y_dst
    assert cb_src == cb_dst
    assert cr_src == cr_dst


def test_strides_length_mismatch_multi_plane_raises():
    """Strides count must match planes count — caught in Python before C."""
    w, h = 64, 32
    y = bytearray(w * h)
    uv = bytearray(w * (h // 2))
    with pytest.raises(ValueError, match='strides'):
        pixpat.draw_pattern(
            pixpat.Buffer([y, uv], 'NV12', w, h, [w]),  # 1 stride, 2 planes
            'smpte',
        )
