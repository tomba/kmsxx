#!/usr/bin/python3

import pykms
from helpers import *

# This hack makes drm initialize the fbcon, setting up the default connector
card = pykms.Card()
card = 0

card = pykms.Card()

conn = card.get_first_connected_connector()
mode = conn.get_default_mode()
crtc = conn.get_current_crtc()
mode = conn.get_default_mode()

fb = pykms.DumbFramebuffer(card, mode.hdisplay, mode.vdisplay, "XR24");
pykms.draw_test_pattern(fb);

crtc.set_mode(conn, fb, mode)

arr = bytearray(256*2*4)
view = memoryview(arr).cast("H")

for i in range(256):
    g = round(255 * pow(i / 255.0, 1 / 2.2))

    view[i * 4 + 0] = g << 8
    view[i * 4 + 1] = g << 8
    view[i * 4 + 2] = g << 8
    view[i * 4 + 3] = 0

gamma = pykms.Blob(card, arr);

set_prop(crtc, "GAMMA_LUT", gamma.id)

input("press enter to remove gamma\n")

set_prop(crtc, "GAMMA_LUT", 0)

input("press enter to exit\n")
