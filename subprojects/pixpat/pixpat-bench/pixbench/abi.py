"""ctypes plumbing for a pixpat native library.

The C structs of pixpat.h, the five entry points, and loading several
libraries into one process so they can be compared or timed side by
side.
"""

from __future__ import annotations

import ctypes
import pathlib

MAX_PLANES = 4


class Buffer(ctypes.Structure):
    _fields_ = [
        ('format', ctypes.c_char_p),
        ('width', ctypes.c_uint32),
        ('height', ctypes.c_uint32),
        ('num_planes', ctypes.c_uint32),
        ('planes', ctypes.c_void_p * MAX_PLANES),
        ('strides', ctypes.c_uint32 * MAX_PLANES),
    ]


class ConvertOpts(ctypes.Structure):
    _fields_ = [
        ('rec', ctypes.c_int),
        ('range', ctypes.c_int),
        ('num_threads', ctypes.c_int),
    ]


class PatternOpts(ctypes.Structure):
    _fields_ = [
        ('rec', ctypes.c_int),
        ('range', ctypes.c_int),
        ('num_threads', ctypes.c_int),
        ('params', ctypes.c_char_p),
    ]


class Lib:
    """One loaded pixpat library and its typed entry points."""

    def __init__(self, path: str | pathlib.Path) -> None:
        self.path = pathlib.Path(path).resolve()
        if not self.path.is_file():
            raise FileNotFoundError(self.path)
        c = ctypes.CDLL(str(self.path))
        c.pixpat_convert.argtypes = [
            ctypes.POINTER(Buffer),
            ctypes.POINTER(Buffer),
            ctypes.POINTER(ConvertOpts),
        ]
        c.pixpat_convert.restype = ctypes.c_int
        c.pixpat_draw_pattern.argtypes = [
            ctypes.POINTER(Buffer),
            ctypes.c_char_p,
            ctypes.POINTER(PatternOpts),
        ]
        c.pixpat_draw_pattern.restype = ctypes.c_int
        c.pixpat_format_supported.argtypes = [ctypes.c_char_p]
        c.pixpat_format_supported.restype = ctypes.c_int
        c.pixpat_format_count.argtypes = []
        c.pixpat_format_count.restype = ctypes.c_size_t
        c.pixpat_format_name.argtypes = [ctypes.c_size_t]
        c.pixpat_format_name.restype = ctypes.c_char_p
        self._c = c
        self.convert = c.pixpat_convert
        self.draw_pattern = c.pixpat_draw_pattern
        self.format_supported = c.pixpat_format_supported
        self.format_count = c.pixpat_format_count
        self.format_name = c.pixpat_format_name
        # Where the code lives. Two paths that dlopen resolved to one
        # object (a hard link, a symlink) share this.
        self.code_addr = ctypes.cast(c.pixpat_convert, ctypes.c_void_p).value or 0

    def formats(self) -> list[str]:
        n = self.format_count()
        return [self.format_name(i).decode('ascii') for i in range(n)]

    def supports(self, name: str) -> bool:
        return self.format_supported(name.encode('ascii')) == 1


def load(paths) -> list[Lib]:
    """Load several libraries, refusing two that are the same object."""
    libs = [Lib(p) for p in paths]
    by_addr: dict[int, Lib] = {}
    for lib in libs:
        other = by_addr.get(lib.code_addr)
        if other is not None:
            raise RuntimeError(
                f'{lib.path} and {other.path} loaded as one object; '
                'two names for the same file cannot be compared'
            )
        by_addr[lib.code_addr] = lib
    return libs
