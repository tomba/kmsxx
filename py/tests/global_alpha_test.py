#!/usr/bin/python3

import pykms
import time

card = pykms.Card()
res = pykms.ResourceManager(card)
#conn = res.reserve_connector("HDMI")
conn = card.get_first_connected_connector
crtc = res.reserve_crtc(conn)
mode = conn.get_default_mode()
modeb = mode.to_blob(card)
plane1 = res.reserve_generic_plane(crtc, pykms.PixelFormat.Undefined)
plane2 = res.reserve_generic_plane(crtc, pykms.PixelFormat.Undefined)

print("Got plane1 %d %d plane2 %d %d" %
      (plane1.idx, plane1.id, plane2.idx, plane2.id))

fb1 = pykms.DumbFramebuffer(card, mode.hdisplay, mode.vdisplay, "AR24");
pykms.draw_test_pattern(fb1);

fb2 = pykms.DumbFramebuffer(card, mode.hdisplay >> 1, mode.vdisplay >> 1, "AR24");
pykms.draw_test_pattern(fb2);

req = pykms.AtomicReq(card)
req.add(conn, "CRTC_ID", crtc.id)
req.add(crtc, {"ACTIVE": 1,
	       "MODE_ID": modeb.id})
req.add(plane1, {"FB_ID": fb1.id,
		"CRTC_ID": crtc.id,
		"SRC_X": 0 << 16,
		"SRC_Y": 0 << 16,
		"SRC_W": fb1.width << 16,
		"SRC_H": fb1.height << 16,
		"CRTC_X": 0,
		"CRTC_Y": 0,
		"CRTC_W": fb1.width,
		"CRTC_H": fb1.height,
		"zpos": 0})

r = req.commit_sync(allow_modeset = True)

alpha = 0
while alpha <= 0xFFFF:
	print("alpha %d" % (alpha >>  8))
	req = pykms.AtomicReq(card)
	req.add(plane2, {"FB_ID": fb2.id,
			 "CRTC_ID": crtc.id,
			 "SRC_X": 0 << 16,
			 "SRC_Y": 0 << 16,
			 "SRC_W": fb2.width << 16,
			 "SRC_H": fb2.height << 16,
			 "CRTC_X": 0,
			 "CRTC_Y": 0,
			 "CRTC_W": fb2.width,
			 "CRTC_H": fb2.height,
			 "zpos": 1,
			 "alpha": alpha })
	r = req.commit_sync()
	alpha = alpha + 0xFF
	time.sleep(0.1)
