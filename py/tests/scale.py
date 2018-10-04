#!/usr/bin/python3

import pykms
import time
import random

def plane_commit(card, crtc, plane, fb, x, y, w, h) :
	req = pykms.AtomicReq(card)
	req.add(plane, {"FB_ID": fb.id,
		"CRTC_ID": crtc.id,
		"SRC_X": 0 << 16,
		"SRC_Y": 0 << 16,
		"SRC_W": fb.width << 16,
		"SRC_H": fb.height << 16,
		"CRTC_X": x,
		"CRTC_Y": y,
		"CRTC_W": w,
		"CRTC_H": h,
		"zpos": 0})
	r = req.commit_sync()

card = pykms.Card()
res = pykms.ResourceManager(card)
#conn = res.reserve_connector("hdmi")
conn = card.get_first_connected_connector
crtc = res.reserve_crtc(conn)
#plane = res.reserve_overlay_plane(crtc)
plane = card.planes[0]

mode = conn.get_default_mode()
#mode = conn.get_mode(1920, 1080, 60, False)
modeb = mode.to_blob(card)
req = pykms.AtomicReq(card)
req.add(conn, "CRTC_ID", crtc.id)
req.add(crtc, {"ACTIVE": 1,
               "MODE_ID": modeb.id})
r = req.commit_sync(allow_modeset = True)

# Initialize framebuffer for the scaled plane
fbX = 1920
fbY = 1080
fb = pykms.DumbFramebuffer(card, fbX, fbY, "RG16");
pykms.draw_test_pattern(fb);

# max downscale
max_downscale_x=128
max_downscale_y=128

# Plane's initial scaled size
W = 160
H = 100

# Plane's initial position
X = 0
Y = 0

# initialize increments
Winc = 1
Hinc = 1
Xinc = 1
Yinc = 1

while True:
	print("+%d+%d %dx%d" % (X, Y, W, H))
	plane_commit(card, crtc, plane, fb, X, Y, W, H)
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
