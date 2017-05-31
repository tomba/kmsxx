#!/usr/bin/python3

import sys
import selectors
import pykms

w = 640
h = 480
fmt = pykms.PixelFormat.YUYV

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

vidpath = pykms.VideoDevice.get_capture_devices()[0]

vid = pykms.VideoDevice(vidpath)
cap = vid.capture_streamer
cap.set_port(0)
cap.set_format(fmt, w, h)
cap.set_queue_size(NUM_BUFS)

for fb in fbs:
    cap.queue(fb)

cap.stream_on()


def readvid(conn, mask):
    fb = cap.dequeue()

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

    cap.queue(fb)

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
