#!/usr/bin/python3

import pykms

# This hack makes drm initialize the fbcon, setting up the default connector
card = pykms.Card()
card = 0

card = pykms.Card()
res = pykms.ResourceManager(card)
conn = res.reserve_connector()
crtc = res.reserve_crtc(conn)
mode = conn.get_default_mode()

fb = pykms.DumbFramebuffer(card, mode.hdisplay, mode.vdisplay, "XR24");
pykms.draw_test_pattern(fb);

crtc.set_mode(conn, fb, mode)

len=256
arr = bytearray(len*2*4)
view = memoryview(arr).cast("H")

for i in range(len):
    g = round(65535 * pow(i / float(len), 1 / 2.2))

    view[i * 4 + 0] = g
    view[i * 4 + 1] = g
    view[i * 4 + 2] = g
    view[i * 4 + 3] = 0

gamma = pykms.Blob(card, arr);

crtc.set_prop("GAMMA_LUT", gamma.id)

input("press enter to remove gamma\n")

crtc.set_prop("GAMMA_LUT", 0)

input("press enter to exit\n")
