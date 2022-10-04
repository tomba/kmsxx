#!/usr/bin/python3

import sys
import selectors
import pykms
import pyv4l2 as v4l2
import argparse
import time

parser = argparse.ArgumentParser()
parser.add_argument("width", type=int)
parser.add_argument("height", type=int)
parser.add_argument("fourcc", type=str, nargs="?", default="UYVY")
parser.add_argument("-t", "--type", type=str, default="drm", help="buffer type (drm/v4l2)")
args = parser.parse_args()

if not args.type in ["drm", "v4l2"]:
    print("Bad buffer type", args.type)
    exit(-1)

w = args.width
h = args.height
fmt = pykms.fourcc_to_pixelformat(args.fourcc)
vfmt = v4l2.drm_fourcc_to_pixelformat(pykms.pixelformat_to_fourcc(fmt))

print("Capturing in {}x{}, drm fmt {} ({}), v4l2 fmt {} ({})".format(w, h,
    fmt, pykms.pixelformat_to_fourcc(fmt), vfmt, v4l2.pixelformat_to_fourcc(vfmt)))

card = pykms.Card()
res = pykms.ResourceManager(card)
conn = res.reserve_connector("hdmi")
crtc = res.reserve_crtc(conn)
plane = res.reserve_overlay_plane(crtc, fmt)

mode = conn.get_default_mode()
modeb = mode.to_blob(card)

req = pykms.AtomicReq(card)
req.add(conn, "CRTC_ID", crtc.id)
req.add(crtc, {"ACTIVE": 1,
        "MODE_ID": modeb.id})
req.commit_sync(allow_modeset = True)

NUM_BUFS = 5

vidpath = v4l2.VideoDevice.get_capture_devices()[0]

vid = v4l2.VideoDevice(vidpath)
cap = vid.capture_streamer
cap.set_port(0)
cap.set_format(vfmt, w, h)
cap.set_queue_size(NUM_BUFS, v4l2.VideoMemoryType.DMABUF if args.type == "drm" else v4l2.VideoMemoryType.MMAP)

fbs = []

def export_v4l2_buf_to_drm(card, i, vfmt):
    finfo = pykms.get_pixel_format_info(fmt)

    if fmt == pykms.PixelFormat.NV12:
        fd = cap.export_buffer(i)
        buf_fds = [fd, fd]
        pitches = [w * finfo.plane(0).bitspp // 8, w * finfo.plane(1).bitspp // 8]
        offsets = [0, w * h * finfo.plane(0).bitspp // 8]
    else:
        buf_fds = [cap.export_buffer(i)]
        pitches = [w * finfo.plane(0).bitspp // 8]
        offsets = [0]

    fb = pykms.DmabufFramebuffer(card, w, h, fmt,
                                 buf_fds, pitches, offsets)

    return fb


for i in range(NUM_BUFS):
    if args.type == "drm":
        fb = pykms.DumbFramebuffer(card, w, h, fmt)
    else:
        fb = export_v4l2_buf_to_drm(card, i, vfmt)

    print(fb)

    fbs.append(fb)

for i in range(NUM_BUFS):
    if args.type == "drm":
        vbuf = v4l2.create_dmabuffer(fbs[i].fd(0))
    else:
        vbuf = v4l2.create_mmapbuffer()
    cap.queue(vbuf)

cap.stream_on()


def readvid(conn, mask):
    vbuf = cap.dequeue()

    fb = fbs[vbuf.index]
    assert(fb != None)

    if card.has_atomic:
        plane.set_props({
            "FB_ID": fb.id,
            "CRTC_ID": crtc.id,
            "SRC_W": fb.width << 16,
            "SRC_H": fb.height << 16,
            "CRTC_W": fb.width,
            "CRTC_H": fb.height,
        })
    else:
        crtc.set_plane(plane, fb, 0, 0, fb.width, fb.height,
            0, 0, fb.width, fb.height)

    cap.queue(vbuf)

def readkey(conn, mask):
    #print("KEY EVENT");
    sys.stdin.readline()
    exit(0)

sel = selectors.DefaultSelector()
sel.register(cap.fd, selectors.EVENT_READ, readvid)
sel.register(sys.stdin, selectors.EVENT_READ, readkey)

while True:
    events = sel.select()
    for key, mask in events:
        callback = key.data
        callback(key.fileobj, mask)
