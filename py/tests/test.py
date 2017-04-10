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

req.add(conn, "CRTC_ID", crtc.id)

req.add(crtc, {"ACTIVE": 1,
		"MODE_ID": modeb.id})

req.add(plane, {"FB_ID": fb.id,
		"CRTC_ID": crtc.id,
		"SRC_X": 0 << 16,
		"SRC_Y": 0 << 16,
		"SRC_W": mode.hdisplay << 16,
		"SRC_H": mode.vdisplay << 16,
		"CRTC_X": 0,
		"CRTC_Y": 0,
		"CRTC_W": mode.hdisplay,
		"CRTC_H": mode.vdisplay,
		"zorder": 0})

req.commit_sync(allow_modeset = True)

input("press enter to exit\n")
