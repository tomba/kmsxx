"""The format catalog as descriptors, and buffer geometry derived from it.

name -> (kind, h_sub, v_sub, [plane]), where a plane is a list of
(channel, bits, shift) components and shift is the absolute bit offset
in the little-endian storage word. A CSI-2 packed sample is split
across two fields, written as (channel, bits, shift, lo_bits,
lo_shift); only the channel and the total width matter for geometry.
'M' is the Bayer mosaic sample (one of R/G/B per the CFA phase).

These are transcribed from the C++ declarations (pixpat-native/src/
formats/*.h) by hand rather than generated from them, and the plane
geometry below is derived from them by the library's own rules. So a
build that disagrees with this table shows up as a stride or plane-size
mismatch (a failed coverage check, a non-zero rc) instead of silently
agreeing on a wrong shape.

kind also drives the color-spec matrix (cross-kind conversions
exercise every rec/range combination).
"""

from __future__ import annotations

import ctypes
import random
from dataclasses import dataclass

from .abi import Buffer, ConvertOpts, Lib, PatternOpts

# The CSI-2 packed Bayer plane shape, shared by the four phases.
_CSI2_10P_M = [
    ('M', 10, 0, 2, 38),
    ('M', 10, 8, 2, 36),
    ('M', 10, 16, 2, 34),
    ('M', 10, 24, 2, 32),
]
_CSI2_12P_M = [('M', 12, 0, 4, 20), ('M', 12, 8, 4, 16)]

FORMATS = {
    # --- RGB packed (pixpat-native/src/formats/rgb.h)
    'XRGB8888': ('rgb', 1, 1, [[('B', 8, 0), ('G', 8, 8), ('R', 8, 16), ('X', 8, 24)]]),
    'ARGB8888': ('rgb', 1, 1, [[('B', 8, 0), ('G', 8, 8), ('R', 8, 16), ('A', 8, 24)]]),
    'XBGR8888': ('rgb', 1, 1, [[('R', 8, 0), ('G', 8, 8), ('B', 8, 16), ('X', 8, 24)]]),
    'ABGR8888': ('rgb', 1, 1, [[('R', 8, 0), ('G', 8, 8), ('B', 8, 16), ('A', 8, 24)]]),
    'RGBX8888': ('rgb', 1, 1, [[('X', 8, 0), ('B', 8, 8), ('G', 8, 16), ('R', 8, 24)]]),
    'RGBA8888': ('rgb', 1, 1, [[('A', 8, 0), ('B', 8, 8), ('G', 8, 16), ('R', 8, 24)]]),
    'BGRX8888': ('rgb', 1, 1, [[('X', 8, 0), ('R', 8, 8), ('G', 8, 16), ('B', 8, 24)]]),
    'BGRA8888': ('rgb', 1, 1, [[('A', 8, 0), ('R', 8, 8), ('G', 8, 16), ('B', 8, 24)]]),
    'RGB888': ('rgb', 1, 1, [[('B', 8, 0), ('G', 8, 8), ('R', 8, 16)]]),
    'BGR888': ('rgb', 1, 1, [[('R', 8, 0), ('G', 8, 8), ('B', 8, 16)]]),
    'RGB332': ('rgb', 1, 1, [[('B', 2, 0), ('G', 3, 2), ('R', 3, 5)]]),
    'RGB565': ('rgb', 1, 1, [[('B', 5, 0), ('G', 6, 5), ('R', 5, 11)]]),
    'BGR565': ('rgb', 1, 1, [[('R', 5, 0), ('G', 6, 5), ('B', 5, 11)]]),
    'XRGB1555': ('rgb', 1, 1, [[('B', 5, 0), ('G', 5, 5), ('R', 5, 10), ('X', 1, 15)]]),
    'ARGB1555': ('rgb', 1, 1, [[('B', 5, 0), ('G', 5, 5), ('R', 5, 10), ('A', 1, 15)]]),
    'XBGR1555': ('rgb', 1, 1, [[('R', 5, 0), ('G', 5, 5), ('B', 5, 10), ('X', 1, 15)]]),
    'ABGR1555': ('rgb', 1, 1, [[('R', 5, 0), ('G', 5, 5), ('B', 5, 10), ('A', 1, 15)]]),
    'XRGB4444': ('rgb', 1, 1, [[('B', 4, 0), ('G', 4, 4), ('R', 4, 8), ('X', 4, 12)]]),
    'ARGB4444': ('rgb', 1, 1, [[('B', 4, 0), ('G', 4, 4), ('R', 4, 8), ('A', 4, 12)]]),
    'XBGR4444': ('rgb', 1, 1, [[('R', 4, 0), ('G', 4, 4), ('B', 4, 8), ('X', 4, 12)]]),
    'ABGR4444': ('rgb', 1, 1, [[('R', 4, 0), ('G', 4, 4), ('B', 4, 8), ('A', 4, 12)]]),
    'RGBX4444': ('rgb', 1, 1, [[('X', 4, 0), ('B', 4, 4), ('G', 4, 8), ('R', 4, 12)]]),
    'RGBA4444': ('rgb', 1, 1, [[('A', 4, 0), ('B', 4, 4), ('G', 4, 8), ('R', 4, 12)]]),
    'XRGB2101010': ('rgb', 1, 1, [[('B', 10, 0), ('G', 10, 10), ('R', 10, 20), ('X', 2, 30)]]),
    'ARGB2101010': ('rgb', 1, 1, [[('B', 10, 0), ('G', 10, 10), ('R', 10, 20), ('A', 2, 30)]]),
    'XBGR2101010': ('rgb', 1, 1, [[('R', 10, 0), ('G', 10, 10), ('B', 10, 20), ('X', 2, 30)]]),
    'ABGR2101010': ('rgb', 1, 1, [[('R', 10, 0), ('G', 10, 10), ('B', 10, 20), ('A', 2, 30)]]),
    'RGBX1010102': ('rgb', 1, 1, [[('X', 2, 0), ('B', 10, 2), ('G', 10, 12), ('R', 10, 22)]]),
    'RGBA1010102': ('rgb', 1, 1, [[('A', 2, 0), ('B', 10, 2), ('G', 10, 12), ('R', 10, 22)]]),
    'BGRX1010102': ('rgb', 1, 1, [[('X', 2, 0), ('R', 10, 2), ('G', 10, 12), ('B', 10, 22)]]),
    'BGRA1010102': ('rgb', 1, 1, [[('A', 2, 0), ('R', 10, 2), ('G', 10, 12), ('B', 10, 22)]]),
    'XRGB16161616': ('rgb', 1, 1, [[('B', 16, 0), ('G', 16, 16), ('R', 16, 32), ('X', 16, 48)]]),
    'XBGR16161616': ('rgb', 1, 1, [[('R', 16, 0), ('G', 16, 16), ('B', 16, 32), ('X', 16, 48)]]),
    'ARGB16161616': ('rgb', 1, 1, [[('B', 16, 0), ('G', 16, 16), ('R', 16, 32), ('A', 16, 48)]]),
    'ABGR16161616': ('rgb', 1, 1, [[('R', 16, 0), ('G', 16, 16), ('B', 16, 32), ('A', 16, 48)]]),
    # --- YUV semiplanar (formats/yuv_semiplanar.h)
    'NV12': ('yuv', 2, 2, [[('Y', 8, 0)], [('U', 8, 0), ('V', 8, 8)]]),
    'NV21': ('yuv', 2, 2, [[('Y', 8, 0)], [('V', 8, 0), ('U', 8, 8)]]),
    'NV16': ('yuv', 2, 1, [[('Y', 8, 0)], [('U', 8, 0), ('V', 8, 8)]]),
    'NV61': ('yuv', 2, 1, [[('Y', 8, 0)], [('V', 8, 0), ('U', 8, 8)]]),
    'P010': (
        'yuv',
        2,
        2,
        [
            [('X', 6, 0), ('Y', 10, 6)],
            [('X', 6, 0), ('U', 10, 6), ('X', 6, 16), ('V', 10, 22)],
        ],
    ),
    'P012': (
        'yuv',
        2,
        2,
        [
            [('X', 4, 0), ('Y', 12, 4)],
            [('X', 4, 0), ('U', 12, 4), ('X', 4, 16), ('V', 12, 20)],
        ],
    ),
    'P016': ('yuv', 2, 2, [[('Y', 16, 0)], [('U', 16, 0), ('V', 16, 16)]]),
    # 3 Y per 32-bit word, 3 (Cb,Cr) pairs per 64-bit word; the 2-bit
    # gaps at bits 30-31 and 62-63 are implicit, as in the C++ decl.
    'P030': (
        'yuv',
        2,
        2,
        [
            [('Y', 10, 0), ('Y', 10, 10), ('Y', 10, 20)],
            [
                ('U', 10, 0),
                ('V', 10, 10),
                ('U', 10, 20),
                ('V', 10, 32),
                ('U', 10, 42),
                ('V', 10, 52),
            ],
        ],
    ),
    'P230': (
        'yuv',
        2,
        1,
        [
            [('Y', 10, 0), ('Y', 10, 10), ('Y', 10, 20)],
            [
                ('U', 10, 0),
                ('V', 10, 10),
                ('U', 10, 20),
                ('V', 10, 32),
                ('U', 10, 42),
                ('V', 10, 52),
            ],
        ],
    ),
    # --- YUV planar (formats/yuv_planar.h)
    'YUV420': ('yuv', 2, 2, [[('Y', 8, 0)], [('U', 8, 0)], [('V', 8, 0)]]),
    'YVU420': ('yuv', 2, 2, [[('Y', 8, 0)], [('V', 8, 0)], [('U', 8, 0)]]),
    'YUV422': ('yuv', 2, 1, [[('Y', 8, 0)], [('U', 8, 0)], [('V', 8, 0)]]),
    'YVU422': ('yuv', 2, 1, [[('Y', 8, 0)], [('V', 8, 0)], [('U', 8, 0)]]),
    'YUV444': ('yuv', 1, 1, [[('Y', 8, 0)], [('U', 8, 0)], [('V', 8, 0)]]),
    'YVU444': ('yuv', 1, 1, [[('Y', 8, 0)], [('V', 8, 0)], [('U', 8, 0)]]),
    'T430': (
        'yuv',
        1,
        1,
        [
            [('Y', 10, 0), ('Y', 10, 10), ('Y', 10, 20), ('X', 2, 30)],
            [('U', 10, 0), ('U', 10, 10), ('U', 10, 20), ('X', 2, 30)],
            [('V', 10, 0), ('V', 10, 10), ('V', 10, 20), ('X', 2, 30)],
        ],
    ),
    # --- YUV packed (formats/yuv_packed.h)
    'VUY888': ('yuv', 1, 1, [[('Y', 8, 0), ('U', 8, 8), ('V', 8, 16)]]),
    'XVUY8888': ('yuv', 1, 1, [[('Y', 8, 0), ('U', 8, 8), ('V', 8, 16), ('X', 8, 24)]]),
    'XVUY2101010': ('yuv', 1, 1, [[('Y', 10, 0), ('U', 10, 10), ('V', 10, 20), ('X', 2, 30)]]),
    'AVUY16161616': ('yuv', 1, 1, [[('Y', 16, 0), ('U', 16, 16), ('V', 16, 32), ('A', 16, 48)]]),
    'XYUV8888': ('yuv', 1, 1, [[('V', 8, 0), ('U', 8, 8), ('Y', 8, 16), ('X', 8, 24)]]),
    'XVYU2101010': ('yuv', 1, 1, [[('U', 10, 0), ('Y', 10, 10), ('V', 10, 20), ('X', 2, 30)]]),
    'XVYU12_16161616': (
        'yuv',
        1,
        1,
        [
            [
                ('X', 4, 0),
                ('U', 12, 4),
                ('X', 4, 16),
                ('Y', 12, 20),
                ('X', 4, 32),
                ('V', 12, 36),
                ('X', 16, 48),
            ]
        ],
    ),
    'XVYU16161616': ('yuv', 1, 1, [[('U', 16, 0), ('Y', 16, 16), ('V', 16, 32), ('X', 16, 48)]]),
    'YUYV': ('yuv', 2, 1, [[('Y', 8, 0), ('U', 8, 8), ('Y', 8, 16), ('V', 8, 24)]]),
    'YVYU': ('yuv', 2, 1, [[('Y', 8, 0), ('V', 8, 8), ('Y', 8, 16), ('U', 8, 24)]]),
    'UYVY': ('yuv', 2, 1, [[('U', 8, 0), ('Y', 8, 8), ('V', 8, 16), ('Y', 8, 24)]]),
    'VYUY': ('yuv', 2, 1, [[('V', 8, 0), ('Y', 8, 8), ('U', 8, 16), ('Y', 8, 24)]]),
    'Y210': (
        'yuv',
        2,
        1,
        [
            [
                ('X', 6, 0),
                ('Y', 10, 6),
                ('X', 6, 16),
                ('U', 10, 22),
                ('X', 6, 32),
                ('Y', 10, 38),
                ('X', 6, 48),
                ('V', 10, 54),
            ]
        ],
    ),
    'Y212': (
        'yuv',
        2,
        1,
        [
            [
                ('X', 4, 0),
                ('Y', 12, 4),
                ('X', 4, 16),
                ('U', 12, 20),
                ('X', 4, 32),
                ('Y', 12, 36),
                ('X', 4, 48),
                ('V', 12, 52),
            ]
        ],
    ),
    'Y216': ('yuv', 2, 1, [[('Y', 16, 0), ('U', 16, 16), ('Y', 16, 32), ('V', 16, 48)]]),
    # --- grayscale + mono RGB (formats/grayscale.h)
    'Y8': ('yuv', 1, 1, [[('Y', 8, 0)]]),
    'Y10': ('yuv', 1, 1, [[('Y', 10, 0), ('X', 6, 10)]]),
    'Y12': ('yuv', 1, 1, [[('Y', 12, 0), ('X', 4, 12)]]),
    'Y16': ('yuv', 1, 1, [[('Y', 16, 0)]]),
    'R8': ('rgb', 1, 1, [[('R', 8, 0)]]),
    'XYYY2101010': ('yuv', 1, 1, [[('Y', 10, 0), ('Y', 10, 10), ('Y', 10, 20), ('X', 2, 30)]]),
    # --- MIPI CSI-2 packed grayscale (formats/grayscale.h + io/csi2.h)
    # 10P: 4 samples in 5 bytes, byte i holding sample i's high 8 bits
    # and byte 4 holding the four 2-bit remainders, sample 0 highest.
    # 12P: 2 samples in 3 bytes, byte 2 holding the two 4-bit
    # remainders.
    'Y10P': (
        'yuv',
        1,
        1,
        [
            [
                ('Y', 10, 0, 2, 38),
                ('Y', 10, 8, 2, 36),
                ('Y', 10, 16, 2, 34),
                ('Y', 10, 24, 2, 32),
            ]
        ],
    ),
    'Y12P': ('yuv', 1, 1, [[('Y', 12, 0, 4, 20), ('Y', 12, 8, 4, 16)]]),
    # --- Bayer raw (formats/bayer.h). RGB-kind: the source demosaics
    # into RGB, the sink keeps the phase's channel and drops the rest.
    'SRGGB8': ('rgb', 1, 1, [[('M', 8, 0)]]),
    'SBGGR8': ('rgb', 1, 1, [[('M', 8, 0)]]),
    'SGRBG8': ('rgb', 1, 1, [[('M', 8, 0)]]),
    'SGBRG8': ('rgb', 1, 1, [[('M', 8, 0)]]),
    'SRGGB10': ('rgb', 1, 1, [[('M', 10, 0), ('X', 6, 10)]]),
    'SBGGR10': ('rgb', 1, 1, [[('M', 10, 0), ('X', 6, 10)]]),
    'SGRBG10': ('rgb', 1, 1, [[('M', 10, 0), ('X', 6, 10)]]),
    'SGBRG10': ('rgb', 1, 1, [[('M', 10, 0), ('X', 6, 10)]]),
    'SRGGB12': ('rgb', 1, 1, [[('M', 12, 0), ('X', 4, 12)]]),
    'SBGGR12': ('rgb', 1, 1, [[('M', 12, 0), ('X', 4, 12)]]),
    'SGRBG12': ('rgb', 1, 1, [[('M', 12, 0), ('X', 4, 12)]]),
    'SGBRG12': ('rgb', 1, 1, [[('M', 12, 0), ('X', 4, 12)]]),
    'SRGGB16': ('rgb', 1, 1, [[('M', 16, 0)]]),
    'SBGGR16': ('rgb', 1, 1, [[('M', 16, 0)]]),
    'SGRBG16': ('rgb', 1, 1, [[('M', 16, 0)]]),
    'SGBRG16': ('rgb', 1, 1, [[('M', 16, 0)]]),
    # CSI-2 packed Bayer: the Y10P / Y12P word shape carrying mosaic
    # samples, so the four phases share one plane description.
    'SRGGB10P': ('rgb', 1, 1, [_CSI2_10P_M]),
    'SBGGR10P': ('rgb', 1, 1, [_CSI2_10P_M]),
    'SGRBG10P': ('rgb', 1, 1, [_CSI2_10P_M]),
    'SGBRG10P': ('rgb', 1, 1, [_CSI2_10P_M]),
    'SRGGB12P': ('rgb', 1, 1, [_CSI2_12P_M]),
    'SBGGR12P': ('rgb', 1, 1, [_CSI2_12P_M]),
    'SGRBG12P': ('rgb', 1, 1, [_CSI2_12P_M]),
    'SGBRG12P': ('rgb', 1, 1, [_CSI2_12P_M]),
}

# The I/O family of a format: which Source/Sink template pair the
# library instantiates for it (pixpat-native/src/formats/*.h picks one
# per format, the templates live in src/io/), i.e. which read and
# write loop a case runs. Transcribed like the table above, and checked
# below to cover it exactly. PackedSource serves both packed RGB and
# packed 4:4:4 YUV; they are split here because the color path between
# them differs. The report groups cases by these, one group per side.
FAMILIES: dict[str, tuple[str, tuple[str, ...]]] = {
    # family: (io/ template, formats)
    'rgb': (
        'Packed',
        (
            'XRGB8888',
            'ARGB8888',
            'XBGR8888',
            'ABGR8888',
            'RGBX8888',
            'RGBA8888',
            'BGRX8888',
            'BGRA8888',
            'RGB888',
            'BGR888',
            'RGB332',
            'RGB565',
            'BGR565',
            'XRGB1555',
            'ARGB1555',
            'XBGR1555',
            'ABGR1555',
            'XRGB4444',
            'ARGB4444',
            'XBGR4444',
            'ABGR4444',
            'RGBX4444',
            'RGBA4444',
            'XRGB2101010',
            'ARGB2101010',
            'XBGR2101010',
            'ABGR2101010',
            'RGBX1010102',
            'RGBA1010102',
            'BGRX1010102',
            'BGRA1010102',
            'XRGB16161616',
            'XBGR16161616',
            'ARGB16161616',
            'ABGR16161616',
        ),
    ),
    'yuv444': (
        'Packed',
        (
            'VUY888',
            'XVUY8888',
            'XVUY2101010',
            'AVUY16161616',
            'XYUV8888',
            'XVYU2101010',
            'XVYU12_16161616',
            'XVYU16161616',
        ),
    ),
    'yuv422': ('PackedYUV', ('YUYV', 'YVYU', 'UYVY', 'VYUY', 'Y210', 'Y212', 'Y216')),
    'semiplanar': ('Semiplanar', ('NV12', 'NV21', 'NV16', 'NV61', 'P010', 'P012', 'P016')),
    'semiplanar-mp': ('MultiPixelSemiplanar', ('P030', 'P230')),
    'planar': ('Planar', ('YUV420', 'YVU420', 'YUV422', 'YVU422', 'YUV444', 'YVU444')),
    'planar-mp': ('MultiPixelPlanar', ('T430',)),
    'gray': ('Gray', ('Y8', 'Y10', 'Y12', 'Y16')),
    'gray-mp': ('MultiPixelGray', ('XYYY2101010',)),
    'gray-csi2': ('GrayPacked', ('Y10P', 'Y12P')),
    'mono-rgb': ('MonoRGB', ('R8',)),
    'bayer': (
        'Bayer',
        (
            'SRGGB8',
            'SBGGR8',
            'SGRBG8',
            'SGBRG8',
            'SRGGB10',
            'SBGGR10',
            'SGRBG10',
            'SGBRG10',
            'SRGGB12',
            'SBGGR12',
            'SGRBG12',
            'SGBRG12',
            'SRGGB16',
            'SBGGR16',
            'SGRBG16',
            'SGBRG16',
        ),
    ),
    'bayer-csi2': (
        'BayerPacked',
        (
            'SRGGB10P',
            'SBGGR10P',
            'SGRBG10P',
            'SGBRG10P',
            'SRGGB12P',
            'SBGGR12P',
            'SGRBG12P',
            'SGBRG12P',
        ),
    ),
}

FAMILY: dict[str, str] = {}
for _fam, (_, _fmts) in FAMILIES.items():
    for _fmt in _fmts:
        if _fmt in FAMILY:
            raise AssertionError(f'{_fmt} is in two families: {FAMILY[_fmt]}, {_fam}')
        FAMILY[_fmt] = _fam
if set(FAMILY) != set(FORMATS):
    raise AssertionError(f'families and formats disagree: {sorted(set(FAMILY) ^ set(FORMATS))}')


def family(fmt: str) -> str:
    return FAMILY[fmt]


# One format per I/O shape family; the fingerprint's depth pass sweeps
# these over every size x spec x thread count.
DEPTH = [
    'XRGB8888',  # packed RGB, 32-bit word
    'RGB332',  # packed RGB, sub-byte components
    'RGB565',  # packed RGB, sub-byte with an odd (6-bit) component
    'BGR888',  # packed RGB, 3-byte word (the hot pivot)
    'RGBA1010102',  # packed RGB, 10-bit + low alpha
    'ABGR16161616',  # packed RGB, 64-bit word
    'R8',  # mono RGB (G=B=R synthesized on read)
    'AVUY16161616',  # packed 4:4:4 YUV, 64-bit word with alpha
    'YUYV',  # packed 4:2:2
    'Y210',  # packed 4:2:2, MSB-aligned in 16-bit slots
    'NV12',  # semiplanar 4:2:0
    'NV16',  # semiplanar 4:2:2
    'P010',  # semiplanar, MSB-aligned 10-bit
    'P030',  # multi-pixel-per-word semiplanar (6x2 block)
    'YUV420',  # planar 4:2:0
    'YUV444',  # planar 4:4:4
    'T430',  # multi-pixel-per-word planar (3x1 block)
    'Y8',  # grayscale
    'Y10',  # grayscale with explicit padding
    'XYYY2101010',  # multi-pixel-per-word grayscale
    'Y10P',  # CSI-2 packed grayscale, 4 samples in 5 bytes
    'Y12P',  # CSI-2 packed grayscale, 2 samples in 3 bytes
    'SRGGB8',  # Bayer, one phase per bit depth below
    'SGBRG10',  # Bayer with explicit padding, G-first phase
    'SBGGR12',  # Bayer, 12-bit
    'SGRBG16',  # Bayer, full-width samples
    'SGBRG10P',  # CSI-2 packed Bayer, 10-bit
    'SBGGR12P',  # CSI-2 packed Bayer, 12-bit
]

RECS = [0, 1, 2]  # BT601, BT709, BT2020
RANGES = [0, 1]  # limited, full
REC_NAMES = ('BT601', 'BT709', 'BT2020')
RANGE_NAMES = ('LIMITED', 'FULL')


def kind(fmt: str) -> str:
    return FORMATS[fmt][0]


# --- geometry derived from the descriptors above, by the rules the
# library's layouts follow.


def _sub(knd, sub, ch):
    """Subsampling of a channel: sub for chroma in a YUV layout, else 1."""
    return sub if knd == 'yuv' and ch in ('U', 'V') else 1


def _bpp(comps):
    """Bytes per storage word: (sum of declared bits) rounded up.

    A split (CSI-2) component's `bits` already counts both its fields.
    """
    return (sum(c[1] for c in comps) + 7) // 8


def _word_span(knd, h_sub, comps):
    """Pixel columns covered by one storage word."""
    span = 0
    for comp in comps:
        ch = comp[0]
        if ch == 'X':
            continue
        k = sum(1 for c in comps if c[0] == ch)
        s = k * _sub(knd, h_sub, ch)
        if span == 0:
            span = s
        elif span != s:
            raise ValueError(f'channels span unequal columns: {comps}')
    if span == 0:
        raise ValueError(f'plane has no data channels: {comps}')
    return span


def _plane_sub_y(knd, v_sub, comps):
    """A plane carrying only chroma holds one row per v_sub pixel rows."""
    for comp in comps:
        if comp[0] != 'X' and _sub(knd, v_sub, comp[0]) == 1:
            return 1
    return v_sub


def geometry(fmt, w, h):
    """[(min_stride, rows)] per plane, tight."""
    knd, h_sub, v_sub, planes = FORMATS[fmt]
    return [
        (w // _word_span(knd, h_sub, p) * _bpp(p), h // _plane_sub_y(knd, v_sub, p)) for p in planes
    ]


def aligned(fmt, w, h) -> bool:
    """Whether w x h is a whole number of storage words and plane rows."""
    knd, h_sub, v_sub, planes = FORMATS[fmt]
    return all(
        w % _word_span(knd, h_sub, p) == 0 and h % _plane_sub_y(knd, v_sub, p) == 0 for p in planes
    )


@dataclass
class Frame:
    """A pixpat buffer plus the storage its plane pointers reference."""

    buf: Buffer
    planes: list[tuple[bytearray, ctypes.Array]]

    def bytes(self) -> list[bytes]:
        return [bytes(d) for d, _ in self.planes]


def make_frame(fmt, w, h, pad=0, fill: random.Random | None = None, prefill=0) -> Frame:
    """A tight frame (stride = min + pad), zero/prefilled or random."""
    geo = geometry(fmt, w, h)
    planes = []
    buf = Buffer()
    buf.format = fmt.encode('ascii')
    buf.width = w
    buf.height = h
    buf.num_planes = len(geo)
    for i, (min_stride, rows) in enumerate(geo):
        stride = min_stride + pad
        data = bytearray([prefill]) * (stride * rows)
        if fill is not None:
            data[:] = fill.randbytes(len(data))
        arr = (ctypes.c_ubyte * len(data)).from_buffer(data)
        planes.append((data, arr))
        buf.planes[i] = ctypes.addressof(arr)
        buf.strides[i] = stride
    return Frame(buf, planes)


def run_convert(lib: Lib, src_fmt, dst_fmt, w, h, rec, rng, threads, seed, pad=0, prefill=0):
    """Returns (rc, [dst plane bytes]). The source fill is a function
    of `seed` only, so two libraries see identical input."""
    fill = random.Random(repr(seed))
    src = make_frame(src_fmt, w, h, pad=pad, fill=fill)
    dst = make_frame(dst_fmt, w, h, pad=pad, prefill=prefill)
    opts = ConvertOpts(rec=rec, range=rng, num_threads=threads)
    rc = lib.convert(ctypes.byref(dst.buf), ctypes.byref(src.buf), ctypes.byref(opts))
    return rc, dst.bytes()


def run_pattern(lib: Lib, pattern: bytes | None, dst_fmt, w, h, rec, rng, threads, params):
    """Returns (rc, [dst plane bytes])."""
    dst = make_frame(dst_fmt, w, h)
    opts = PatternOpts(rec=rec, range=rng, num_threads=threads, params=params)
    rc = lib.draw_pattern(ctypes.byref(dst.buf), pattern, ctypes.byref(opts))
    return rc, dst.bytes()
