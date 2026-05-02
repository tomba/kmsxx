from __future__ import annotations

from enum import Enum
from typing import Any, Iterator, Mapping, Sequence, TypeAlias

from .pykms import *
from .pykms import (
    AtomicReq as _AtomicReq,
    Blob,
    Card as _Card,
    Connector,
    Crtc,
    DrmObject as _DrmObject,
    DrmPropObject,
    Framebuffer,
    Plane,
)

PropValue: TypeAlias = int
PropName: TypeAlias = str
Rect: TypeAlias = Sequence[float | int]

class Rotation(int, Enum):
    ROTATE_0: int
    ROTATE_90: int
    ROTATE_180: int
    ROTATE_270: int
    ROTATE_MASK: int
    REFLECT_X: int
    REFLECT_Y: int
    REFLECT_MASK: int

class DrmEventType(Enum):
    VBLANK: int
    FLIP_COMPLETE: int

class DrmEvent:
    type: DrmEventType
    seq: int
    time: float
    data: int
    def __init__(self, type: DrmEventType, seq: int, time: float, data: int) -> None: ...

class DrmObject(_DrmObject):
    def set_prop(self, prop: PropName | int, value: PropValue) -> None: ...
    def set_props(self, map: Mapping[PropName | int, PropValue]) -> None: ...

class Card(_Card):
    def disable_planes(self) -> None: ...
    def read_events(self) -> Iterator[DrmEvent]: ...

class AtomicReq(_AtomicReq):
    def add_connector(self, conn: Connector, crtc: Crtc | None) -> None: ...
    def add_crtc(self, crtc: Crtc, mode_blob: Blob | None) -> None: ...
    def add_plane(
        self,
        plane: Plane,
        fb: Framebuffer | None,
        crtc: Crtc | None,
        src: Rect | None = ...,
        dst: Rect | None = ...,
        zpos: int | None = ...,
        params: Mapping[PropName | int, PropValue] = ...,
    ) -> None: ...
