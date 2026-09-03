"""Shared-library size: the file, the file stripped, and the ELF
sections that get mapped — `.text` is the number that tracks code."""

from __future__ import annotations

import pathlib
import shutil
import struct
import subprocess
import tempfile

SHF_ALLOC = 0x2

# Reported first, in this order; any other allocated section follows.
HEADLINE = ('.text', '.rodata', '.data.rel.ro', '.data', '.bss', '.eh_frame')


def sections(path: pathlib.Path) -> dict[str, int]:
    """Sizes of the SHF_ALLOC sections, by name."""
    data = path.read_bytes()
    if data[:4] != b'\x7fELF':
        raise ValueError(f'{path}: not an ELF file')
    endian = '<' if data[5] == 1 else '>'
    if data[4] == 2:
        (e_shoff,) = struct.unpack_from(endian + 'Q', data, 0x28)
        e_shentsize, e_shnum, e_shstrndx = struct.unpack_from(endian + 'HHH', data, 0x3A)
        shdr = endian + 'IIQQQQIIQQ'
    else:
        (e_shoff,) = struct.unpack_from(endian + 'I', data, 0x20)
        e_shentsize, e_shnum, e_shstrndx = struct.unpack_from(endian + 'HHH', data, 0x2E)
        shdr = endian + 'IIIIIIIIII'
    hdrs = [struct.unpack_from(shdr, data, e_shoff + i * e_shentsize) for i in range(e_shnum)]
    str_off = hdrs[e_shstrndx][4]
    out: dict[str, int] = {}
    for name_off, _typ, flags, _addr, _off, size, *_rest in hdrs:
        if not flags & SHF_ALLOC:
            continue
        end = data.index(b'\0', str_off + name_off)
        out[data[str_off + name_off : end].decode('ascii')] = size
    return out


def stripped_bytes(path: pathlib.Path) -> int | None:
    """Size after `strip --strip-all`; None when strip is unavailable."""
    strip = shutil.which('strip')
    if strip is None:
        return None
    with tempfile.TemporaryDirectory() as tmp:
        out = pathlib.Path(tmp) / path.name
        proc = subprocess.run(
            [strip, '--strip-all', '-o', str(out), str(path)], capture_output=True, check=False
        )
        if proc.returncode != 0:
            return None
        return out.stat().st_size


def lib_size(path: str | pathlib.Path) -> dict:
    p = pathlib.Path(path).resolve()
    secs = sections(p)
    return {
        'bytes': p.stat().st_size,
        'stripped_bytes': stripped_bytes(p),
        'alloc_bytes': sum(secs.values()),
        'sections': secs,
    }


def ordered_sections(secs: dict[str, int]) -> list[tuple[str, int]]:
    head = [(n, secs[n]) for n in HEADLINE if n in secs]
    rest = sorted((n, s) for n, s in secs.items() if n not in HEADLINE)
    return head + rest
