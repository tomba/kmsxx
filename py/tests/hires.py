#!/usr/bin/python3

import pykms

card = pykms.Card()

res = pykms.ResourceManager(card)
conn = res.reserve_connector("hdmi")
crtc = res.reserve_crtc(conn)

mode = conn.get_default_mode()

# NOTE: clock in kHz
#mode = pykms.videomode_from_timings(148000, 1920, 88, 44, 148, 1080, 4, 5, 36)
#mode = pykms.videomode_from_timings(197000, 2880, 8, 32, 40, 1080, 17, 8, 6)

mode.hsync = pykms.SyncPolarity.Positive
mode.vsync = pykms.SyncPolarity.Negative

modeb = mode.to_blob(card)
plane1 = res.reserve_generic_plane(crtc)
plane2 = res.reserve_generic_plane(crtc)

fb = pykms.DumbFramebuffer(card, mode.hdisplay, mode.vdisplay, "XR24");

pykms.draw_test_pattern(fb);

card.disable_planes()

req = pykms.AtomicReq(card)

req.add(conn, "CRTC_ID", crtc.id)

req.add(crtc, {"ACTIVE": 1,
               "MODE_ID": modeb.id})

req.add(plane1, {"FB_ID": fb.id,
                 "CRTC_ID": crtc.id,
                 "SRC_X": 0 << 16,
                 "SRC_Y": 0 << 16,
                 "SRC_W": (mode.hdisplay // 2) << 16,
                 "SRC_H": mode.vdisplay << 16,
                 "CRTC_X": 0,
                 "CRTC_Y": 0,
                 "CRTC_W": mode.hdisplay // 2,
                 "CRTC_H": mode.vdisplay,
                 "zorder": 0})


req.add(plane2, {"FB_ID": fb.id,
                 "CRTC_ID": crtc.id,
                 "SRC_X": (mode.hdisplay // 2) << 16,
                 "SRC_Y": 0 << 16,
                 "SRC_W": (mode.hdisplay // 2) << 16,
                 "SRC_H": mode.vdisplay << 16,
                 "CRTC_X": mode.hdisplay // 2,
                 "CRTC_Y": 0,
                 "CRTC_W": mode.hdisplay // 2,
                 "CRTC_H": mode.vdisplay,
                 "zorder": 1})


req.commit_sync(allow_modeset = True)

input("press enter to exit\n")
