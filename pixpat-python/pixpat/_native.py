"""ctypes plumbing for libpixpat.

Private module — public API lives in :mod:`pixpat`. Loads the bundled
shared library, declares the C structs and function signatures, and
provides :func:`_fill_buffer` to translate Python plane sequences into
the C ``pixpat_buffer`` layout while pinning the underlying memory via
the buffer protocol.

The buffer-protocol path uses ``PyObject_GetBuffer`` / ``PyBuffer_Release``
directly (rather than ctypes ``from_buffer``) so that read-only inputs
work for the ``src`` side of :func:`pixpat.convert`. ``from_buffer``
unconditionally requires a writable buffer.
"""

import ctypes
import os
import pathlib
from typing import Sequence

_PIXPAT_MAX_PLANES = 4

_lib_override = os.environ.get('PIXPAT_LIB')
if _lib_override:
    if not pathlib.Path(_lib_override).exists():
        raise ImportError(f'pixpat: PIXPAT_LIB={_lib_override} does not exist')
    _lib = ctypes.CDLL(_lib_override)
else:
    _lib_dir = pathlib.Path(__file__).parent / '_lib'
    _so_candidates = sorted(_lib_dir.glob('libpixpat.so*'))
    if not _so_candidates:
        raise ImportError(f'pixpat: no libpixpat.so* found in {_lib_dir}')
    _lib = ctypes.CDLL(str(_so_candidates[0]))


class _Buffer(ctypes.Structure):
    _fields_ = [
        ('format', ctypes.c_char_p),
        ('width', ctypes.c_uint32),
        ('height', ctypes.c_uint32),
        ('num_planes', ctypes.c_uint32),
        ('planes', ctypes.c_void_p * _PIXPAT_MAX_PLANES),
        ('strides', ctypes.c_uint32 * _PIXPAT_MAX_PLANES),
    ]


class _PatternOpts(ctypes.Structure):
    _fields_ = [
        ('rec', ctypes.c_int),
        ('range', ctypes.c_int),
        ('num_threads', ctypes.c_int),
        ('params', ctypes.c_char_p),
    ]


class _ConvertOpts(ctypes.Structure):
    _fields_ = [
        ('rec', ctypes.c_int),
        ('range', ctypes.c_int),
        ('num_threads', ctypes.c_int),
    ]


_lib.pixpat_draw_pattern.argtypes = [
    ctypes.POINTER(_Buffer),
    ctypes.c_char_p,
    ctypes.POINTER(_PatternOpts),
]
_lib.pixpat_draw_pattern.restype = ctypes.c_int

_lib.pixpat_convert.argtypes = [
    ctypes.POINTER(_Buffer),
    ctypes.POINTER(_Buffer),
    ctypes.POINTER(_ConvertOpts),
]
_lib.pixpat_convert.restype = ctypes.c_int

_lib.pixpat_format_supported.argtypes = [ctypes.c_char_p]
_lib.pixpat_format_supported.restype = ctypes.c_int

_lib.pixpat_format_count.argtypes = []
_lib.pixpat_format_count.restype = ctypes.c_size_t

_lib.pixpat_format_name.argtypes = [ctypes.c_size_t]
_lib.pixpat_format_name.restype = ctypes.c_char_p


class _Py_buffer(ctypes.Structure):
    _fields_ = [
        ('buf', ctypes.c_void_p),
        ('obj', ctypes.py_object),
        ('len', ctypes.c_ssize_t),
        ('itemsize', ctypes.c_ssize_t),
        ('readonly', ctypes.c_int),
        ('ndim', ctypes.c_int),
        ('format', ctypes.c_char_p),
        ('shape', ctypes.POINTER(ctypes.c_ssize_t)),
        ('strides', ctypes.POINTER(ctypes.c_ssize_t)),
        ('suboffsets', ctypes.POINTER(ctypes.c_ssize_t)),
        ('internal', ctypes.c_void_p),
    ]


_PyObject_GetBuffer = ctypes.pythonapi.PyObject_GetBuffer
_PyObject_GetBuffer.argtypes = [
    ctypes.py_object,
    ctypes.POINTER(_Py_buffer),
    ctypes.c_int,
]
_PyObject_GetBuffer.restype = ctypes.c_int

_PyBuffer_Release = ctypes.pythonapi.PyBuffer_Release
_PyBuffer_Release.argtypes = [ctypes.POINTER(_Py_buffer)]
_PyBuffer_Release.restype = None

_PyBUF_SIMPLE = 0
_PyBUF_WRITABLE = 0x0001


class _PinnedBuffers:
    """Holds Py_buffer views for the lifetime of a pixpat call.

    Releases each view in ``__exit__`` (or when garbage-collected) so
    the underlying objects' buffer-export count drops back to zero.
    """

    def __init__(self) -> None:
        self._views: list[_Py_buffer] = []

    def acquire(self, obj, *, writable: bool) -> int:
        view = _Py_buffer()
        flags = _PyBUF_WRITABLE if writable else _PyBUF_SIMPLE
        # ctypes.pythonapi propagates the set exception (BufferError /
        # TypeError) automatically on failure, so no rc check is needed.
        _PyObject_GetBuffer(obj, ctypes.byref(view), flags)
        self._views.append(view)
        return view.buf or 0

    def release(self) -> None:
        while self._views:
            view = self._views.pop()
            _PyBuffer_Release(ctypes.byref(view))

    def __enter__(self) -> '_PinnedBuffers':
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        self.release()

    def __del__(self) -> None:
        self.release()


def _fill_buffer(
    buf: _Buffer,
    pins: _PinnedBuffers,
    planes: Sequence,
    fmt: str,
    width: int,
    height: int,
    strides: Sequence[int],
    *,
    writable: bool,
    role: str = '',
) -> None:
    """Populate ``buf`` from a Python plane sequence, pinning each plane.

    ``writable`` selects whether each plane must be writable: True for
    destination buffers (the C library writes into them), False for
    source buffers (the C library only reads).
    """
    label = f'{role} ' if role else ''
    if len(planes) > _PIXPAT_MAX_PLANES:
        raise ValueError(f'too many {label}planes: {len(planes)} (max {_PIXPAT_MAX_PLANES})')
    if len(strides) != len(planes):
        raise ValueError(f'{label}strides has {len(strides)} entries, expected {len(planes)}')

    plane_ptrs = (ctypes.c_void_p * _PIXPAT_MAX_PLANES)()
    stride_arr = (ctypes.c_uint32 * _PIXPAT_MAX_PLANES)()
    for i, plane in enumerate(planes):
        try:
            addr = pins.acquire(plane, writable=writable)
        except BufferError as e:
            kind = 'writable' if writable else 'buffer-protocol'
            raise TypeError(f'{label}plane {i} does not support the {kind} interface: {e}') from e
        plane_ptrs[i] = addr
        stride_arr[i] = strides[i]

    buf.format = fmt.encode('ascii')
    buf.width = width
    buf.height = height
    buf.num_planes = len(planes)
    buf.planes = plane_ptrs
    buf.strides = stride_arr


__all__ = [
    '_Buffer',
    '_PatternOpts',
    '_ConvertOpts',
    '_PinnedBuffers',
    '_lib',
    '_fill_buffer',
]
