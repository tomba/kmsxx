#!/usr/bin/python3

import sys
import pykms

# draw test pattern via dmabuf?
dmabuf = False

# Use omap?
omap = False

if omap:
	card = pykms.OmapCard()
else:
	card = pykms.Card()

if len(sys.argv) > 1:
    conn_name = sys.argv[1]
else:
    conn_name = ""

res = pykms.ResourceManager(card)
conn = res.reserve_connector(conn_name)
crtc = res.reserve_crtc(conn)
plane = res.reserve_generic_plane(crtc)
mode = conn.get_default_mode()
modeb = mode.to_blob(card)

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

card.disable_planes()

req = pykms.AtomicReq(card)

req.add_connector(conn, crtc)
req.add_crtc(crtc, modeb)
req.add_plane(plane, fb, crtc, dst=(0, 0, mode.hdisplay, mode.vdisplay))

req.commit_sync(allow_modeset = True)

input("press enter to exit\n")
