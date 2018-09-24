from .pykms import *
from enum import Enum
import os
import struct

#
# Common RGB colours
#

red = RGB(255, 0, 0)
green = RGB(0, 255, 0)
blue = RGB(0, 0, 255)
yellow = RGB(255, 255, 0)
purple = RGB(255, 0, 255)
white = RGB(255, 255, 255)
cyan = RGB(0, 255, 255)

#
# Rotation enum
#

class Rotation(int, Enum):
    ROTATE_0 = 1 << 0
    ROTATE_90 = 1 << 1
    ROTATE_180 = 1 << 2
    ROTATE_270 = 1 << 3
    ROTATE_MASK = ROTATE_0 | ROTATE_90 | ROTATE_180 | ROTATE_270
    REFLECT_X = 1 << 4
    REFLECT_Y = 1 << 5
    REFLECT_MASK = REFLECT_X | REFLECT_Y

#
# DrmObject API extensions
#

def __obj_set_prop(self, prop, value):
    if self.card.has_atomic:
        areq = AtomicReq(self.card)
        areq.add(self, prop, value)
        if areq.commit_sync() != 0:
            print("commit failed")
    else:
        if self.set_prop_value(prop, value) != 0:
            print("setting property failed")

def __obj_set_props(self, map):
    if self.card.has_atomic:
        areq = AtomicReq(self.card)

        for key, value in map.items():
            areq.add(self, key, value)

        if areq.commit_sync() != 0:
            print("commit failed")
    else:
        for propid,propval in map.items():
            if self.set_prop_value(propid, propval) != 0:
                print("setting property failed")

DrmObject.set_prop = __obj_set_prop
DrmObject.set_props = __obj_set_props

#
# Card API extensions
#

def __card_disable_planes(self):
    areq = AtomicReq(self)

    for p in self.planes:
        areq.add(p, "FB_ID", 0)
        areq.add(p, "CRTC_ID", 0)

    if areq.commit_sync() != 0:
        print("disabling planes failed")

Card.disable_planes = __card_disable_planes

class DrmEventType(Enum):
    VBLANK = 0x01
    FLIP_COMPLETE = 0x02

#
# AtomicReq API extensions
#

def __atomic_req_add_connector(req, conn, crtc):
    req.add(conn, "CRTC_ID", crtc.id if crtc else 0)

def __atomic_req_add_crtc(req, crtc, mode_blob):
    if mode_blob:
        req.add(crtc, {"ACTIVE": 1, "MODE_ID": mode_blob.id})
    else:
        req.add(crtc, {"ACTIVE": 0, "MODE_ID": 0})

def __atomic_req_add_plane(req, plane, fb, crtc,
                           src=None, dst=None, zpos=None,
                           params={}):
    if not src and fb:
        src = (0, 0, fb.width, fb.height)

    if not dst:
        dst = src

    m = {"FB_ID": fb.id if fb else 0,
         "CRTC_ID": crtc.id if crtc else 0}

    if src is not None:
        src_x = int(round(src[0] * 65536))
        src_y = int(round(src[1] * 65536))
        src_w = int(round(src[2] * 65536))
        src_h = int(round(src[3] * 65536))

        m["SRC_X"] = src_x
        m["SRC_Y"] = src_y
        m["SRC_W"] = src_w
        m["SRC_H"] = src_h

    if dst is not None:
        crtc_x = int(round(dst[0]))
        crtc_y = int(round(dst[1]))
        crtc_w = int(round(dst[2]))
        crtc_h = int(round(dst[3]))

        m["CRTC_X"] = crtc_x
        m["CRTC_Y"] = crtc_y
        m["CRTC_W"] = crtc_w
        m["CRTC_H"] = crtc_h

    if zpos is not None:
        m["zpos"] = zpos

    m.update(params)

    req.add(plane, m)

pykms.AtomicReq.add_connector = __atomic_req_add_connector
pykms.AtomicReq.add_crtc = __atomic_req_add_crtc
pykms.AtomicReq.add_plane = __atomic_req_add_plane

# struct drm_event {
#   __u32 type;
#   __u32 length;
#};
#

_drm_ev = struct.Struct("II")

#struct drm_event_vblank {
#   struct drm_event base;
#   __u64 user_data;
#   __u32 tv_sec;
#   __u32 tv_usec;
#   __u32 sequence;
#   __u32 reserved;
#};

_drm_ev_vbl = struct.Struct("QIIII") # Note: doesn't contain drm_event

class DrmEvent:
    def __init__(self, type, seq, time, data):
        self.type = type
        self.seq = seq
        self.time = time
        self.data = data

# Return DrmEvents. Note: blocks if there's nothing to read
def __card_read_events(self):
    buf = os.read(self.fd, _drm_ev_vbl.size * 20)

    if len(buf) == 0:
        return

    if len(buf) < _drm_ev.size:
        raise RuntimeError("Partial DRM event")

    idx = 0

    while idx < len(buf):
        ev_tuple = _drm_ev.unpack_from(buf, idx)

        type = DrmEventType(ev_tuple[0])

        if type != DrmEventType.VBLANK and type != DrmEventType.FLIP_COMPLETE:
            raise RuntimeError("Illegal DRM event type")

        vbl_tuple = _drm_ev_vbl.unpack_from(buf, idx + _drm_ev.size)

        seq = vbl_tuple[3]
        time = vbl_tuple[1] + vbl_tuple[2] / 1000000.0;
        udata = vbl_tuple[0]

        yield DrmEvent(type, seq, time, udata)

        idx += ev_tuple[1]

Card.read_events = __card_read_events
