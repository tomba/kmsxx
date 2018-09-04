#!/usr/bin/python3

import pykms
import time

card = pykms.Card()
res = pykms.ResourceManager(card)
conn = res.reserve_connector("")
crtc = res.reserve_crtc(conn)
mode = conn.get_default_mode()
modeb = mode.to_blob(card)
format = pykms.PixelFormat.ARGB8888
plane1 = res.reserve_generic_plane(crtc, format)
plane2 = res.reserve_generic_plane(crtc, format)

print("Got plane1 %d %d plane2 %d %d" %
      (plane1.idx, plane1.id, plane2.idx, plane2.id))

fb1 = pykms.DumbFramebuffer(card, mode.hdisplay, mode.vdisplay, format);
pykms.draw_test_pattern(fb1);

fb2 = pykms.DumbFramebuffer(card, mode.hdisplay >> 1, mode.vdisplay >> 1, format);
pykms.draw_test_pattern(fb2);

alpha = 0

req = pykms.AtomicReq(card)
req.add(conn, "CRTC_ID", crtc.id)
req.add(crtc, {"ACTIVE": 1,
	       "MODE_ID": modeb.id})
req.add_plane(plane1, fb1, crtc)
req.add_plane(plane2, fb2, crtc)

r = req.commit_sync(allow_modeset = True)
assert r == 0, "Initial commit failed: %d" % r

while alpha <= 0xFFFF:
	print("alpha %d" % (alpha >>  8))
	req = pykms.AtomicReq(card)
	req.add(plane2, {"alpha": alpha })
	r = req.commit_sync()
	assert r == 0, "alpha change commit failed: %d" % r
	alpha = alpha + 0xFF
	time.sleep(0.1)

input("press enter exit\n")
