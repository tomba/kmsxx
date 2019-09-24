#!/usr/bin/python3

import sys
import pykms
import argparse

parser = argparse.ArgumentParser()
parser.add_argument("-c", "--connector", default="")
args = parser.parse_args()

card = pykms.Card()

res = pykms.ResourceManager(card)
conn = res.reserve_connector(args.connector)
crtc = res.reserve_crtc(conn)
plane = res.reserve_generic_plane(crtc)

card.disable_planes()

modes = conn.get_modes()

def even(i):
	return i & ~1

for mode in modes:
	long_str = mode.to_string_long()
	short_str = mode.to_string_short()

	print(long_str)

	modeb = mode.to_blob(card)

	fb = pykms.DumbFramebuffer(card, mode.hdisplay, mode.vdisplay, "XR24");
	pykms.draw_test_pattern(fb);
	pykms.draw_text(fb, even((fb.width // 2) - (8 * len(short_str)) // 2), 4, short_str, pykms.white)

	req = pykms.AtomicReq(card)

	req.add_connector(conn, crtc)
	req.add_crtc(crtc, modeb)
	req.add_plane(plane, fb, crtc, dst=(0, 0, mode.hdisplay, mode.vdisplay))

	req.commit_sync(allow_modeset = True)

	input("press enter to show next videomode\n")

print("done")
