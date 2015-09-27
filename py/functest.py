#!/usr/bin/python3.4

import pykms

card = pykms.Card()

conn = card.get_first_connected_connector()

mode = conn.get_default_mode()

fb = pykms.Framebuffer(card, mode.hdisplay, mode.vdisplay, "XR24");
fb.draw_test_pattern(0);

crtc = conn.get_current_crtc()

crtc.set_mode(conn, fb, mode)

print("OK")

