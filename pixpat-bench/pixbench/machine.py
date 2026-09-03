"""Where a measurement was taken."""

from __future__ import annotations

import os
import pathlib
import platform
import socket


def _cpu_model() -> str | None:
    try:
        with open('/proc/cpuinfo') as f:
            for line in f:
                if line.startswith('model name'):
                    return line.partition(':')[2].strip()
    except OSError:
        pass
    return None


def _governor(cpu: int) -> str | None:
    p = pathlib.Path(f'/sys/devices/system/cpu/cpu{cpu}/cpufreq/scaling_governor')
    try:
        return p.read_text().strip()
    except OSError:
        return None


def info(cpus: tuple[int, ...] | None = None) -> dict:
    return {
        'hostname': socket.gethostname(),
        'cpu_model': _cpu_model(),
        'cpus': os.cpu_count(),
        'arch': platform.machine(),
        'kernel': platform.release(),
        'python': platform.python_version(),
        'libc': ' '.join(platform.libc_ver()).strip() or None,
        'cpu_pinned': list(cpus) if cpus else None,
        'governor': _governor(cpus[0] if cpus else 0),
    }
