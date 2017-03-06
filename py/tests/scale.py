#!/usr/bin/python3

import pykms
import time
import random

card = pykms.Card()
res = pykms.ResourceManager(card)
conn = res.reserve_connector("hdmi")
crtc = res.reserve_crtc(conn)
plane = res.reserve_overlay_plane(crtc)

mode = conn.get_default_mode()
#mode = conn.get_mode(1920, 1080, 60, False)

# Blank framefuffer for primary plane
fb0 = pykms.DumbFramebuffer(card, mode.hdisplay, mode.vdisplay, "AR24");

crtc.set_mode(conn, fb0, mode)

# Initialize framebuffer for the scaled plane
fbX = 1920
fbY = 1080
fb = pykms.DumbFramebuffer(card, fbX, fbY, "RG16");
pykms.draw_test_pattern(fb);

# Plane's scaled size and size increments
W = 72
H = 54
Winc = 1
Hinc = 1

# Plane's position and position increments
X = 0
Y = 0
Xinc = 1
Yinc = 1
while True:
	print("+%d+%d %dx%d" % (X, Y, W, H))
	crtc.set_plane(plane, fb, X, Y, W, H, 0, 0, fbX, fbY)
	W = W + Winc
	H = H + Hinc
	if (Winc == 1 and W >= mode.hdisplay - X):
		Winc = -1
	if (Winc == -1 and W <= fbX/32):
		Winc = 1
	if (Hinc == 1 and H >= mode.vdisplay - Y):
		Hinc = -1
	if (Hinc == -1 and H <= fbY/32):
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
