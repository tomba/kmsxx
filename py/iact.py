#!/usr/bin/python3 -i

# This is a base script for interactive libkms python environment

import pykms
from time import sleep
from math import sin
from math import cos

card = pykms.Card()

conn = card.get_first_connected_connector()

mode = conn.get_default_mode()

fb = pykms.DumbFramebuffer(card, 200, 200, "XR24");
pykms.draw_test_pattern(fb);

crtc = conn.get_current_crtc()

#crtc.set_mode(conn, fb, mode)

i = 0
for p in card.get_planes():
    globals()["plane"+str(i)] = p
    i=i+1

i = 0
for c in card.get_crtcs():
    globals()["crtc"+str(i)] = c
    i=i+1

for p in crtc.get_possible_planes():
    if p.plane_type() == 0:
        plane = p
        break

def set_plane(x, y):
    crtc.set_plane(plane, fb, x, y, fb.width(), fb.height(), 0, 0, fb.width(), fb.height())

def props(o):
    o.refresh_props()
    map = o.get_prop_map()
    for propid in map:
        prop = card.get_prop(propid)
        print("%-15s %d (%#x)" % (prop.name(), map[propid], map[propid]))

set_plane(0, 0)

# for x in range(0, crtc.width() - fb.width()): set_plane(x, int((sin(x/50) + 1) * 100)); sleep(0.01)
