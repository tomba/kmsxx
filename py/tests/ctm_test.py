#!/usr/bin/python3

import sys
import pykms

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


if len(sys.argv) > 1:
    conn_name = sys.argv[1]
else:
    conn_name = ""

card = pykms.Card()
res = pykms.ResourceManager(card)
conn = res.reserve_connector(conn_name)
crtc = res.reserve_crtc(conn)
mode = conn.get_default_mode()

fb = pykms.DumbFramebuffer(card, mode.hdisplay, mode.vdisplay, "XR24");
pykms.draw_test_pattern(fb);

crtc.set_mode(conn, fb, mode)

input("press enter to set normal ctm\n")

ctm = [ 1.0,	0.0,	0.0,
        0.0,	1.0,	0.0,
        0.0,	0.0,	1.0 ]

ctmb = ctm_to_blob(ctm, card)

crtc.set_prop("CTM", ctmb.id)

input("press enter to set new ctm\n")

ctm = [ 0.0,	1.0,	0.0,
        0.0,	0.0,	1.0,
        1.0,	0.0,	0.0 ]

ctmb = ctm_to_blob(ctm, card)

crtc.set_prop("CTM", ctmb.id)

print("r->b g->r b->g ctm active\n")

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
