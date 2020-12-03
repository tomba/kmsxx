#!/usr/bin/python3

import pykms
import argparse

parser = argparse.ArgumentParser()
parser.add_argument("-c", "--connector", default="")
parser.add_argument("-l", "--legacy", action="store_true", default=False)
args = parser.parse_args()

card = pykms.Card()
res = pykms.ResourceManager(card)
conn = res.reserve_connector(args.connector)
crtc = res.reserve_crtc(conn)
mode = conn.get_default_mode()

fb = pykms.DumbFramebuffer(card, mode.hdisplay, mode.vdisplay, "XR24");
pykms.draw_test_pattern(fb);

crtc.set_mode(conn, fb, mode)

use_legacy = args.legacy

if not use_legacy:
	prop = crtc.get_prop("GAMMA_LUT")

	if not prop:
		prop = crtc.get_prop("DEGAMMA_LUT")

		if not prop:
			print("No gamma property found")
			exit(-1)
		else:
			print("Using DEGAMMA_LUT for gamma")


def legacy_gamma_set():
	len = crtc.legacy_gamma_size()

	table = []

	for i in range(len):
	    g = round(65535 * pow(i / float(len), 1 / 2.2))
	    table.append((g, g, g))

	crtc.legacy_gamma_set(table)

def legacy_gamma_clear():
	len = crtc.legacy_gamma_size()

	table = []

	for i in range(len):
	    g = round(65535 * (i / float(len)))
	    table.append((g, g, g))

	crtc.legacy_gamma_set(table)

def gamma_set():
	len=256
	arr = bytearray(len*2*4)
	view = memoryview(arr).cast("H")

	for i in range(len):
	    g = round(65535 * pow(i / float(len), 1 / 2.2))

	    view[i * 4 + 0] = g
	    view[i * 4 + 1] = g
	    view[i * 4 + 2] = g
	    view[i * 4 + 3] = 0

	gamma = pykms.Blob(card, arr);

	crtc.set_prop(prop, gamma.id)


def gamma_clear():
	crtc.set_prop(prop, 0)

input("press enter to apply gamma\n")

if use_legacy:
	legacy_gamma_set()
else:
	gamma_set()

input("press enter to remove gamma\n")

if use_legacy:
	legacy_gamma_clear()
else:
	gamma_clear()

input("press enter to exit\n")
