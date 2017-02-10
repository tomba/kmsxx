#!/usr/bin/python3

import pykms

# draw test pattern via dmabuf?
dmabuf = False

# Use omap?
omap = False

if omap:
	card = pykms.OmapCard()
else:
	card = pykms.Card()

res = pykms.ResourceManager(card)
conn = res.reserve_connector()
crtc = res.reserve_crtc(conn)
mode = conn.get_default_mode()

if omap:
	origfb = pykms.OmapFramebuffer(card, mode.hdisplay, mode.vdisplay, "XR24");
else:
	origfb = pykms.DumbFramebuffer(card, mode.hdisplay, mode.vdisplay, "XR24");

if dmabuf:
	fb = pykms.ExtFramebuffer(card, origfb.width, origfb.height, origfb.format,
		[origfb.fd(0)], [origfb.stride(0)], [origfb.offset(0)])
else:
	fb = origfb

pykms.draw_test_pattern(fb);

crtc.set_mode(conn, fb, mode)

input("press enter to exit\n")
