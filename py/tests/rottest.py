#!/usr/bin/python3

import pykms
from enum import Enum

import termios, sys, os, tty

card = pykms.OmapCard()

res = pykms.ResourceManager(card)
conn = res.reserve_connector()
crtc = res.reserve_crtc(conn)
mode = conn.get_default_mode()
modeb = mode.to_blob(card)
rootplane = res.reserve_primary_plane(crtc, pykms.PixelFormat.XRGB8888)
plane = res.reserve_overlay_plane(crtc, pykms.PixelFormat.NV12)

card.disable_planes()

req = pykms.AtomicReq(card)

req.add(conn, "CRTC_ID", crtc.id)

req.add(crtc, {"ACTIVE": 1,
		"MODE_ID": modeb.id})

# This enables the root plane

#rootfb = pykms.OmapFramebuffer(card, mode.hdisplay, mode.vdisplay, "XR24");
#pykms.draw_test_pattern(rootfb);
#
#req.add(rootplane, {"FB_ID": rootfb.id,
#		"CRTC_ID": crtc.id,
#		"SRC_X": 0 << 16,
#		"SRC_Y": 0 << 16,
#		"SRC_W": mode.hdisplay << 16,
#		"SRC_H": mode.vdisplay << 16,
#		"CRTC_X": 0,
#		"CRTC_Y": 0,
#		"CRTC_W": mode.hdisplay,
#		"CRTC_H": mode.vdisplay,
#		"zpos": 0})

req.commit_sync(allow_modeset = True)

def show_rot_plane(crtc, plane, fb, rot, x_scale, y_scale):

	crtc_w = int(fb_w * x_scale)
	crtc_h = int(fb_h * y_scale)

	if (rot & pykms.Rotation.ROTATE_90) or (rot & pykms.Rotation.ROTATE_270):
		tmp = crtc_w
		crtc_w = crtc_h
		crtc_h = tmp

	crtc_x = int(mode.hdisplay / 2 - crtc_w / 2)
	crtc_y = int(mode.vdisplay / 2 - crtc_h / 2)

	req = pykms.AtomicReq(card)

	src_x = 0
	src_y = 0
	src_w = fb_w - src_x
	src_h = fb_h - src_y

	print("SRC {},{}-{}x{}  DST {},{}-{}x{}".format(
		src_x, src_y, src_w, src_h,
		crtc_x, crtc_y, crtc_w, crtc_h))

	angle_str = pykms.Rotation(rot & pykms.Rotation.ROTATE_MASK).name
	reflect_x_str = "REFLECT_X" if rot & pykms.Rotation.REFLECT_X else ""
	reflect_y_str = "REFLECT_Y" if rot & pykms.Rotation.REFLECT_Y else ""

	print("{} {} {}".format(angle_str, reflect_x_str, reflect_y_str))

	sys.stdout.flush()

	req.add(plane, {"FB_ID": fb.id,
			"CRTC_ID": crtc.id,
			"SRC_X": src_x << 16,
			"SRC_Y": src_y << 16,
			"SRC_W": src_w << 16,
			"SRC_H": src_h << 16,
			"CRTC_X": crtc_x,
			"CRTC_Y": crtc_y,
			"CRTC_W": crtc_w,
			"CRTC_H": crtc_h,
			"rotation": rot,
			"zpos": 2})

	req.commit_sync(allow_modeset = True)


fb_w = 480
fb_h = 150
x_scale = 1
y_scale = 1

fb = pykms.OmapFramebuffer(card, fb_w, fb_h, "NV12", flags = pykms.OmapFramebuffer.Tiled);
#fb = pykms.DumbFramebuffer(card, fb_w, fb_h, "NV12")
pykms.draw_test_pattern(fb);

def even(i):
	return i & ~1

pykms.draw_text(fb, even((fb_w // 2) - (8 * 3) // 2), 4, "TOP", pykms.white)
pykms.draw_text(fb, even((fb_w // 2) - (8 * 6) // 2), fb_h - 8 - 4, "BOTTOM", pykms.white)
pykms.draw_text(fb, 4, even(((fb_h // 2) - 4)), "L", pykms.white)
pykms.draw_text(fb, fb_w - 8 - 4, even(((fb_h // 2) - 4)), "R", pykms.white)

rots = [ pykms.Rotation.ROTATE_0, pykms.Rotation.ROTATE_90, pykms.Rotation.ROTATE_180, pykms.Rotation.ROTATE_270 ]
cursors = [ "A", "D", "B", "C" ]

print("Use the cursor keys, x and y to change rotation. Press q to quit.")

fd = sys.stdin.fileno()
oldterm = termios.tcgetattr(fd)
tty.setcbreak(fd)

try:
	esc_seq = 0

	current_rot = pykms.Rotation.ROTATE_0

	show_rot_plane(crtc, plane, fb, current_rot, x_scale, y_scale)

	while True:
		c = sys.stdin.read(1)
		#print("Got character {}".format(repr(c)))

		changed = False
		handled = False

		if esc_seq == 0:
			if c == "\x1b":
				esc_seq = 1
				handled = True
		elif esc_seq == 1:
			if c == "[":
				esc_seq = 2
				handled = True
			else:
				esc_seq = 0
		elif esc_seq == 2:
			esc_seq = 0

			if c in cursors:
				handled = True

				rot = rots[cursors.index(c)]

				current_rot &= ~pykms.Rotation.ROTATE_MASK
				current_rot |= rot

				changed = True

		if not handled:
			if c == "q":
				break
			elif c == "x":
				current_rot ^= pykms.Rotation.REFLECT_X
				changed = True
			elif c == "y":
				current_rot ^= pykms.Rotation.REFLECT_Y
				changed = True

		if changed:
			show_rot_plane(crtc, plane, fb, current_rot, x_scale, y_scale)

finally:
	termios.tcsetattr(fd, termios.TCSAFLUSH, oldterm)
