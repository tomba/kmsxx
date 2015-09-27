#!/usr/bin/lua

require("libluakms")
require("bit32")

card = libluakms.Card()
card:print_short()

conn = card:get_first_connected_connector()

mode = conn:get_default_mode()

fb = libluakms.Framebuffer(card, mode.hdisplay, mode.vdisplay, "XR24");
libluakms.draw_test_pattern(fb);

crtc = conn:get_current_crtc()

crtc:set_mode(conn, fb, mode)

print("Press enter to exit")
io.read()
