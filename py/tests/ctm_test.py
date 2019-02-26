#!/usr/bin/python3

import sys
import pykms
import argparse

def ctm_to_blob(ctm, card):
    len=9
    arr = bytearray(len*8)
    view = memoryview(arr).cast("I")

    for x in range(len):
        i, d = divmod(ctm[x], 1)
        if i < 0:
            i = -i
            sign = 1 << 31
        else:
            sign = 0
        view[x * 2 + 0] = int(d * ((2 ** 32) - 1))
        view[x * 2 + 1] = int(i) | sign
        #print("%f = %08x.%08x" % (ctm[x], view[x * 2 + 1], view[x * 2 + 0]))

    return pykms.Blob(card, arr);


parser = argparse.ArgumentParser(description='Simple CRTC CTM-property test.')
parser.add_argument('--connector', '-c', dest='connector',
		    required=False, help='connector to output')
parser.add_argument('--mode', '-m', dest='modename',
		    required=False, help='Video mode name to use')
parser.add_argument('--plane', '-p', dest='plane', type=int,
		    required=False, help='plane number to use')
args = parser.parse_args()

card = pykms.Card()
res = pykms.ResourceManager(card)
conn = res.reserve_connector(args.connector)
crtc = res.reserve_crtc(conn)
format = pykms.PixelFormat.ARGB8888
if args.modename == None:
    mode = conn.get_default_mode()
else:
    mode = conn.get_mode(args.modename)
modeb = mode.to_blob(card)

fb = pykms.DumbFramebuffer(card, mode.hdisplay, mode.vdisplay, "XR24");
pykms.draw_test_pattern(fb);

if args.plane == None:
	plane = res.reserve_generic_plane(crtc, fb.format)
else:
	plane = card.planes[args.plane]

card.disable_planes()
crtc.disable_mode()

input("press enter to set ctm at the same time with crtc mode\n")

ctm = [ 0.0,	1.0,	0.0,
        0.0,	0.0,	1.0,
        1.0,	0.0,	0.0 ]

ctmb = ctm_to_blob(ctm, card)

req = pykms.AtomicReq(card)
req.add(conn, "CRTC_ID", crtc.id)
req.add(crtc, {"ACTIVE": 1,
               "MODE_ID": modeb.id,
               "CTM": ctmb.id})
req.add_plane(plane, fb, crtc)
r = req.commit_sync(allow_modeset = True)
assert r == 0, "Initial commit failed: %d" % r

print("r->b g->r b->g ctm active\n")

input("press enter to set normal ctm\n")

ctm = [ 1.0,	0.0,	0.0,
        0.0,	1.0,	0.0,
        0.0,	0.0,	1.0 ]

ctmb = ctm_to_blob(ctm, card)

crtc.set_prop("CTM", ctmb.id)

input("press enter to set new ctm\n")

ctm = [ 0.0,	0.0,	1.0,
        1.0,	0.0,	0.0,
        0.0,	1.0,	0.0 ]

ctmb = ctm_to_blob(ctm, card)

crtc.set_prop("CTM", ctmb.id)
input("r->g g->b b->r ctm active\n")

input("press enter to turn off the crtc\n")

crtc.disable_mode()

input("press enter to enable crtc again\n")

crtc.set_mode(conn, fb, mode)

input("press enter to remove ctm\n")

crtc.set_prop("CTM", 0)

input("press enter to exit\n")
