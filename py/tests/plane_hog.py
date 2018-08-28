#!/usr/bin/python3

import pykms
import sys

card = pykms.Card()
res = pykms.ResourceManager(card)

conn1 = False
conn2 = False

for conn in card.connectors:
     if not conn1:
          conn1 = conn
     elif not conn2:
          conn2 = conn
     else:
          break

crtc1 = res.reserve_crtc(conn1)
mode1 = conn1.get_default_mode()
modeb1 = mode1.to_blob(card)
print("CRTC idx %d goes to %s connector" % (crtc1.idx, conn1.fullname))

if conn2:
     crtc2 = res.reserve_crtc(conn2)
     mode2 = conn2.get_default_mode()
     modeb2 = mode2.to_blob(card)
     print("CRTC idx %d goes to %s connector" % (crtc2.idx, conn2.fullname))

fbwidth = 480
fbheight = 270

fb = pykms.DumbFramebuffer(card, fbwidth, fbheight, "AR24");
pykms.draw_test_pattern(fb);

# Disable request
card.disable_planes()

plane_list = []

while True:
     plane = res.reserve_generic_plane(crtc1)
     if plane:
         print("Got plane idx %d" % plane.idx)
         plane_list.append(plane)
     else:
         break

print("Got %d planes" % len(plane_list))

req = pykms.AtomicReq(card)
req.add(conn1, "CRTC_ID", crtc1.id)
req.add(crtc1, {"ACTIVE": 1,
               "MODE_ID": modeb1.id})

input("Press enter to enable crtc idx %d at %s" % (crtc1.idx, conn1.fullname))
r = req.commit_sync(allow_modeset = True)

print("Crtc enable request returned %d\n" % r)

x = 0
y = 0
z = 0

for plane in plane_list:
    input("Press enter to enable plane idx %d on crtc idx %d" %
          (plane.idx, crtc1.idx))
    req = pykms.AtomicReq(card)
    req.add(plane, {"FB_ID": fb.id,
                     "CRTC_ID": crtc1.id,
                     "SRC_X": 0 << 16,
                     "SRC_Y": 0 << 16,
                     "SRC_W": fb.width << 16,
                     "SRC_H": fb.height << 16,
                     "CRTC_X": x,
                     "CRTC_Y": y,
                     "CRTC_W": fb.width,
                     "CRTC_H": fb.height,
                     "zpos": z})
    r = req.commit_sync()
    print("Plane enable request returned %d\n" % r)

    x = x + 50
    y = y + 50
    z = z + 1

if not conn2:
     sys.exit()

req = pykms.AtomicReq(card)
req.add(conn2, "CRTC_ID", crtc2.id)
req.add(crtc2, {"ACTIVE": 1,
                "MODE_ID": modeb2.id})

input("Press enter to enable crtc idx %d at %s" % (crtc2.idx, conn2.fullname))
r = req.commit_sync(allow_modeset = True)
print("Crtc enable request returned %d\n" % r)

x = 0
y = 0
z = 0

# Code assumes that planes for crtc1 also work for crtc2
for plane in reversed(plane_list):

    input("Press enter to disable plane idx %d on crtc idx %d" %
          (plane.idx, crtc1.idx))
    req = pykms.AtomicReq(card)
    req.add(plane, {"FB_ID": 0,
                    "CRTC_ID": 0})
    r = req.commit_sync(allow_modeset = True)
    print("Plane disable request returned %d\n" % r)

    input("Press enter to enable plane idx %d on crtc idx %d" %
          (plane.idx, crtc2.idx))
    req = pykms.AtomicReq(card)
    req.add(plane, {"FB_ID": fb.id,
                    "CRTC_ID": crtc2.id,
                    "SRC_X": 0 << 16,
                    "SRC_Y": 0 << 16,
                    "SRC_W": fb.width << 16,
                    "SRC_H": fb.height << 16,
                    "CRTC_X": x,
                    "CRTC_Y": y,
                    "CRTC_W": fb.width,
                    "CRTC_H": fb.height,
                    "zpos": z})
    r = req.commit_sync(allow_modeset = True)
    print("Plane enable request returned %d\n" % r)

    x = x + 50
    y = y + 50
    z = z + 1

input("press enter to exit\n")
