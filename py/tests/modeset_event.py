#!/usr/bin/python3

import pykms
import selectors
import sys

def readdrm(fileobj, mask):
    for ev in card.read_events():
        eventhandler(ev)

def waitevent(sel):
    events = sel.select(1)
    if not events:
        print("Error: timeout receiving event")
    else:
        for key, mask in events:
            key.data(key.fileobj, mask)

def eventhandler(event):
    print("Received %s event successfully (seq %d time %f)" %
          (event.type, event.seq, event.time))

card = pykms.Card()
sel = selectors.DefaultSelector()
sel.register(card.fd, selectors.EVENT_READ, readdrm)

res = pykms.ResourceManager(card)
conn = res.reserve_connector()
crtc = res.reserve_crtc(conn)
pplane = res.reserve_primary_plane(crtc)

mode = conn.get_default_mode()
modeb = mode.to_blob(card)

for format in pplane.formats:
    if format == pykms.PixelFormat.XRGB8888:
        break
    if format == pykms.PixelFormat.RGB565:
        break

fb = pykms.DumbFramebuffer(card, mode.hdisplay, mode.vdisplay, format);
pykms.draw_test_pattern(fb);

# Disable request
card.disable_planes()

print("Setting %s to %s using %s" % (conn.fullname, mode.name, format))

req = pykms.AtomicReq(card)

req.add(conn, "CRTC_ID", crtc.id)
req.add(crtc, {"ACTIVE": 1,
               "MODE_ID": modeb.id})
req.add(pplane, {"FB_ID": fb.id,
                 "CRTC_ID": crtc.id,
                 "SRC_X": 0 << 16,
                 "SRC_Y": 0 << 16,
                 "SRC_W": mode.hdisplay << 16,
                 "SRC_H": mode.vdisplay << 16,
                 "CRTC_X": 0,
                 "CRTC_Y": 0,
                 "CRTC_W": mode.hdisplay,
                 "CRTC_H": mode.vdisplay})

ret = req.test(True)
if ret != 0:
    print("Atomic test failed: %d" % ret)
    sys.exit()

req.commit(0, allow_modeset = True)
waitevent(sel)

input("press enter to exit\n")
