#!/usr/bin/python3

import pykms
import time
import random
import argparse

card = pykms.Card()
res = pykms.ResourceManager(card)
conn = res.reserve_connector('')
crtc = res.reserve_crtc(conn)

format = pykms.PixelFormat.ARGB8888

card.disable_planes()


mode = conn.get_default_mode()
modeb = mode.to_blob(card)
req = pykms.AtomicReq(card)
req.add(conn, "CRTC_ID", crtc.id)
req.add(crtc, {"ACTIVE": 1,
               "MODE_ID": modeb.id})
r = req.commit_sync(allow_modeset = True)
assert r == 0, "Initial commit failed: %d" % r

fbX = 1920
fbY = 1080

plane1 = res.reserve_generic_plane(crtc, format)
fb1 = pykms.DumbFramebuffer(card, fbX, fbY, format);
pykms.draw_test_pattern(fb1);

plane2 = res.reserve_generic_plane(crtc, format)
# Creates transparent buffer
fb2 = pykms.DumbFramebuffer(card, fbX, fbY, format);

plane3 = res.reserve_generic_plane(crtc, format)
fb3 = pykms.DumbFramebuffer(card, fbX, fbY, format);
pykms.draw_test_pattern(fb3);

max_downscale_x=5
max_downscale_y=8

W = 640
H = 480

X = 0
Y = 0

Winc = 1
Hinc = 1
Xinc = 1
Yinc = 1

sleep = 0.014

primary_h = 1080
# Uncomment this line to reproduce incorrect mixer region issue:
#primary_h = 1080/2

while True:
	print("+%d+%d %dx%d" % (X, Y, W, H))
	time.sleep(sleep)
	req = pykms.AtomicReq(card)

	req.add_plane(plane1, fb1, crtc, None, (X, Y, W, H))
	req.add_plane(plane2, fb2, crtc, None, (0, 0, 1920, primary_h))
	req.add_plane(plane3, fb3, crtc, None, (abs(1920-W-X), abs(1080-H-Y), W, H))

	r = req.commit_sync()
	assert r == 0, "Plane commit failed: %d" % r

	W = W + Winc
	H = H + Hinc
	if (Winc == 1 and W >= mode.hdisplay - X):
		Winc = -1
	if (Winc == -1 and W <= fbX/max_downscale_x):
		Winc = 1
	if (Hinc == 1 and H >= mode.vdisplay - Y):
		Hinc = -1
	if (Hinc == -1 and H <= fbY/max_downscale_y):
		Hinc = 1
	X = X + Xinc
	Y = Y + Yinc
	if (Xinc == 1 and X >= mode.hdisplay - W):
		Xinc = -1
	if (Xinc == -1 and X <= 0):
		Xinc = 1
	if (Yinc == 1 and Y >= mode.vdisplay - H):
		Yinc = -1
	if (Yinc == -1 and Y <= 0):
		Yinc = 1
