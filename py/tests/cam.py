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
parser.add_argument("fourcc", type=str, nargs="?", default="YUVY")
args = parser.parse_args()

w = args.width
h = args.height
fmt = pykms.fourcc_to_pixelformat(args.fourcc)

print("Capturing in {}x{}".format(w, h))

card = pykms.Card()
res = pykms.ResourceManager(card)
conn = res.reserve_connector()
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

fbs = []
for i in range(NUM_BUFS):
    fb = pykms.DumbFramebuffer(card, w, h, fmt)
    fbs.append(fb)

vidpath = v4l2.VideoDevice.get_capture_devices()[0]

vid = v4l2.VideoDevice(vidpath)
cap = vid.capture_streamer
cap.set_port(0)
cap.set_format(v4l2.PixelFormat(fmt), w, h)
cap.set_queue_size(NUM_BUFS, v4l2.VideoMemoryType.DMABUF)

for fb in fbs:
    vbuf = v4l2.create_dmabuffer(fb.fd(0))
    cap.queue(vbuf)

cap.stream_on()


def readvid(conn, mask):
    vbuf = cap.dequeue()

    fb = next((fb for fb in fbs if fb.fd(0) == vbuf.fd), None)
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
