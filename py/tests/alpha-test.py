#!/usr/bin/python3

import pykms
import time

# This hack makes drm initialize the fbcon, setting up the default connector
card = pykms.Card()
card = 0

card = pykms.Card()
res = pykms.ResourceManager(card)
conn = res.reserve_connector()
crtc = res.reserve_crtc(conn)
mode = conn.get_default_mode()

planes = []
for p in card.planes:
    if p.supports_crtc(crtc) == False:
        continue
    planes.append(p)

if len(planes) != 3:
    print("Need 3 planes!")
    exit(1)

card.disable_planes()

w = mode.hdisplay
h = mode.vdisplay

fbs=[]

for i in range(len(planes)):
    fbs.append(pykms.DumbFramebuffer(card, w, h, "AR24"))

pykms.draw_rect(fbs[0], 50, 50, 200, 200, pykms.RGB(128, 255, 0, 0))
pykms.draw_rect(fbs[1], 150, 50, 200, 200, pykms.RGB(128, 0, 255, 0))
pykms.draw_rect(fbs[2], 50, 150, 200, 200, pykms.RGB(128, 0, 0, 255))


crtc.set_props({
    "trans-key-mode": 0,
    "trans-key": 0,
    "background": 0,
    "alpha_blender": 1,
})

for i in range(len(planes)):
    plane = planes[i]
    fb = fbs[i]

    print("set crtc {}, plane {}, fb {}".format(crtc.id, p.id, fbs[i].id))

    plane.set_props({
        "FB_ID": fb.id,
        "CRTC_ID": crtc.id,
        "SRC_W": fb.width << 16,
        "SRC_H": fb.height << 16,
        "CRTC_W": fb.width,
        "CRTC_H": fb.height,
        "zorder": i,
    })

    time.sleep(1)

input("press enter to exit\n")
