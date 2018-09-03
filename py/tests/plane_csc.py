#!/usr/bin/python3

import pykms

card = pykms.Card()
res = pykms.ResourceManager(card)
conn = res.reserve_connector("")
crtc = res.reserve_crtc(conn)
mode = conn.get_default_mode()
modeb = mode.to_blob(card)
plane = res.reserve_generic_plane(crtc, pykms.PixelFormat.UYVY)

print("Got plane %d %d" % (plane.idx, plane.id))

fb = pykms.DumbFramebuffer(card, mode.hdisplay, mode.vdisplay, "UYVY");
pykms.draw_test_pattern(fb);

req = pykms.AtomicReq(card)
req.add(conn, "CRTC_ID", crtc.id)
req.add(crtc, {"ACTIVE": 1,
               "MODE_ID": modeb.id})

input("Press enter to enable crtc idx %d at %s" % (crtc.idx, conn.fullname))
r = req.commit_sync(allow_modeset = True)

input("Press enter to enable plane idx %d at %s" % (plane.idx, conn.fullname))

req = pykms.AtomicReq(card)
req.add_plane(plane, fb, crtc)
r = req.commit_sync()
print("Plane enable request returned %d\n" % r)

yuv_types = [pykms.YUVType.BT601_Lim,
             pykms.YUVType.BT601_Full,
             pykms.YUVType.BT709_Lim,
             pykms.YUVType.BT709_Full]

encoding_enums = plane.get_prop("COLOR_ENCODING").enums
range_enums = plane.get_prop("COLOR_RANGE").enums

for i in range(0, 2):
    for j in range(0, 2):
        input("press enter to for encoding: \"%s\" range: \"%s\"\n" %
              (encoding_enums[i], range_enums[j]))

        req = pykms.AtomicReq(card)
        req.add(plane, {"COLOR_ENCODING": i,
                 "COLOR_RANGE": j})
        req.commit_sync()

        for t in yuv_types:
            input("press enter to redraw with yuv_type %s\n" % t)
            pykms.draw_test_pattern(fb, t);

input("press enter to exit\n")
