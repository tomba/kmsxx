#!/usr/bin/python3

import pykms

def ctm_to_blob(ctm, card):
    len=9
    arr = bytearray(len*8)
    view = memoryview(arr).cast("I")

    for x in range(len):
        i, d = divmod(ctm[x], 1)
        if i < 0:
            i = -i
            sing = 1 << 31
        else:
            sing = 0
        view[x * 2 + 0] = int(d * ((2 ** 31) - 1))
        view[x * 2 + 1] = int(i) | sing
        #print("%f = %08x.%08x" % (ctm[x], view[x * 2 + 1], view[x * 2 + 0]))

    return pykms.Blob(card, arr);


card = pykms.Card()
res = pykms.ResourceManager(card)
conn = res.reserve_connector("unknown")
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

input("press enter to set r->g g->b b->r ctm\n")

ctm = [ 0.0,	1.0,	0.0,
        0.0,	0.0,	1.0,
        1.0,	0.0,	0.0 ]

ctmb = ctm_to_blob(ctm, card)

crtc.set_prop("CTM", ctmb.id)

input("press enter to turn off the crtc\n")

crtc.disable_mode()

input("press enter to enable crtc again\n")

crtc.set_mode(conn, fb, mode)

input("press enter to remove ctm\n")

crtc.set_prop("CTM", 0)

input("press enter to exit\n")
