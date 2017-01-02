#!/usr/bin/python3 -i

# This is a base script for interactive kms++ python environment

import pykms
from time import sleep
from math import sin
from math import cos

card = pykms.Card()
res = pykms.ResourceManager(card)
conn = res.reserve_connector()
crtc = res.reserve_crtc(conn)

mode = conn.get_default_mode()

fb = pykms.DumbFramebuffer(card, 200, 200, "XR24");
pykms.draw_test_pattern(fb);

#crtc.set_mode(conn, fb, mode)

i = 0
for p in card.planes:
    globals()["plane"+str(i)] = p
    i=i+1

i = 0
for c in card.crtcs:
    globals()["crtc"+str(i)] = c
    i=i+1

for p in crtc.possible_planes:
    if p.plane_type == pykms.PlaneType.Overlay:
        plane = p
        break

def set_plane(x, y):
    crtc.set_plane(plane, fb, x, y, fb.width, fb.height, 0, 0, fb.width, fb.height)

set_plane(0, 0)

# for x in range(0, crtc.width() - fb.width()): set_plane(x, int((sin(x/50) + 1) * 100)); sleep(0.01)
