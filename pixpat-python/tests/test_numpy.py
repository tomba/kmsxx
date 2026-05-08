"""Empirical check that pixpat works correctly with numpy.

pixpat does not depend on numpy. This file proves that ndarrays work
as ``Buffer`` planes through the buffer protocol, in the shapes a
caller would typically use. Skipped when numpy is missing.
"""

import pixpat
import pytest

np = pytest.importorskip('numpy')


def test_xrgb8888_into_uint8_3d_ndarray():
    w, h = 64, 32
    arr = np.zeros((h, w, 4), dtype=np.uint8)
    pixpat.draw_pattern(pixpat.Buffer([arr], 'XRGB8888', w, h, [arr.strides[0]]), 'smpte')
    assert arr.any()


def test_abgr16161616_into_uint16_3d_ndarray():
    w, h = 64, 32
    arr = np.zeros((h, w, 4), dtype=np.uint16)
    pixpat.draw_pattern(pixpat.Buffer([arr], 'ABGR16161616', w, h, [arr.strides[0]]), 'smpte')
    assert arr.any()
    assert arr.dtype == np.uint16


def test_nv12_into_two_ndarrays():
    w, h = 64, 32
    y = np.zeros((h, w), dtype=np.uint8)
    uv = np.zeros((h // 2, w), dtype=np.uint8)  # interleaved U,V at half-height
    pixpat.draw_pattern(
        pixpat.Buffer(
            planes=[y, uv],
            fmt='NV12',
            width=w,
            height=h,
            strides=[y.strides[0], uv.strides[0]],
        ),
        'smpte',
    )
    assert y.any()
    assert uv.any()


def test_convert_with_readonly_ndarray_src():
    """A numpy view with writeable=False must work as the convert source."""
    w, h = 64, 32
    src = np.zeros((h, w, 4), dtype=np.uint8)
    pixpat.draw_pattern(pixpat.Buffer([src], 'XRGB8888', w, h, [src.strides[0]]), 'smpte')
    src.flags.writeable = False

    dst = np.zeros((h, w, 4), dtype=np.uint16)
    pixpat.convert(
        pixpat.Buffer([dst], 'ABGR16161616', w, h, [dst.strides[0]]),
        pixpat.Buffer([src], 'XRGB8888', w, h, [src.strides[0]]),
    )
    assert dst.any()


def test_roundtrip_ndarrays_byte_for_byte():
    """End-to-end XRGB8888 -> ABGR16161616 -> XRGB8888 with ndarrays only."""
    w, h = 64, 32
    src = np.zeros((h, w, 4), dtype=np.uint8)
    mid = np.zeros((h, w, 4), dtype=np.uint16)
    dst = np.zeros((h, w, 4), dtype=np.uint8)

    pixpat.draw_pattern(pixpat.Buffer([src], 'XRGB8888', w, h, [src.strides[0]]), 'smpte')
    pixpat.convert(
        pixpat.Buffer([mid], 'ABGR16161616', w, h, [mid.strides[0]]),
        pixpat.Buffer([src], 'XRGB8888', w, h, [src.strides[0]]),
    )
    pixpat.convert(
        pixpat.Buffer([dst], 'XRGB8888', w, h, [dst.strides[0]]),
        pixpat.Buffer([mid], 'ABGR16161616', w, h, [mid.strides[0]]),
    )
    assert np.array_equal(src, dst)


def test_writable_view_via_view_method():
    """`arr.view()` shares memory and is writable by default — drawing into
    it must update the original."""
    w, h = 64, 32
    arr = np.zeros((h, w, 4), dtype=np.uint8)
    view = arr.view()
    pixpat.draw_pattern(pixpat.Buffer([view], 'XRGB8888', w, h, [view.strides[0]]), 'smpte')
    assert arr.any()  # the original sees the writes


def test_noncontiguous_source_smoke():
    """Discovery test: what happens if the caller hands us a non-C-contiguous
    array? pixpat reads ``stride`` bytes per row, so any row-contiguous
    layout where ``arr.strides[0]`` is the row stride should work; a
    transposed array (where rows aren't even contiguous) is misuse.

    This test passes a *valid* row-contiguous slice — the top half of a
    bigger image, taken via slicing — and expects success. It exists to
    document the contract: 'rows must be contiguous in memory; pass
    arr.strides[0] as the stride'.
    """
    big = np.zeros((64, 64, 4), dtype=np.uint8)
    top_half = big[:32]  # shape (32, 64, 4); rows still C-contiguous
    assert top_half.strides[0] == 64 * 4
    pixpat.draw_pattern(
        pixpat.Buffer([top_half], 'XRGB8888', 64, 32, [top_half.strides[0]]),
        'smpte',
    )
    assert big[:32].any()
    assert not big[32:].any()  # the other half stayed untouched
