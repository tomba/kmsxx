#!/usr/bin/python3

import pykms
import time
import random
import argparse

def plane_commit(card, crtc, plane, fb, x, y, w, h) :
	req = pykms.AtomicReq(card)
	req.add_plane(plane, fb, crtc, None, (x, y, w, h))
	r = req.commit_sync()
	assert r == 0, "Plane commit failed: %d" % r


parser = argparse.ArgumentParser(description='Simple scaling stress test.')
parser.add_argument('--plane', '-p', dest='plane', default="",
		    required=False, help='plane number to use')
parser.add_argument('--connector', '-c', dest='connector', default="",
		    required=False, help='connector to output')

args = parser.parse_args()

card = pykms.Card()
res = pykms.ResourceManager(card)
conn = res.reserve_connector(args.connector)
crtc = res.reserve_crtc(conn)
format = pykms.PixelFormat.NV12

if args.plane == "":
	plane = res.reserve_generic_plane(crtc, format)
else:
	plane = card.planes[int(args.plane)]

mode = conn.get_default_mode()
modeb = mode.to_blob(card)
req = pykms.AtomicReq(card)
req.add(conn, "CRTC_ID", crtc.id)
req.add(crtc, {"ACTIVE": 1,
               "MODE_ID": modeb.id})
r = req.commit_sync(allow_modeset = True)
assert r == 0, "Initial commit failed: %d" % r

# Initialize framebuffer for the scaled plane
fbX = 1920
fbY = 1080
fb = pykms.DumbFramebuffer(card, fbX, fbY, format);
pykms.draw_test_pattern(fb);

# max downscale.
# The values bellow are for DSS5. For DSS7 use 64 for both.
max_downscale_x=5
max_downscale_y=8

# Plane's initial scaled size
W = 640
H = 480

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
