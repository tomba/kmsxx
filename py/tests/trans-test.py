#!/usr/bin/python3

import pykms
import time
import sys

if len(sys.argv) != 2:
    print("Usage: {} <test-number>".format(sys.argv[1]))
    print("  1 - test_am5_trans_dest()")
    print("  2 - test_am5_trans_src()")
    print("  3 - test_am4_normal_trans_dst()")
    print("  4 - test_am4_normal_trans_src()")
    print("  5 - test_am4_alpha_trans_src()")
    exit(0)

TEST = int(sys.argv[1])

# This hack makes drm initialize the fbcon, setting up the default connector
card = pykms.Card()
card = 0

card = pykms.Card()
res = pykms.ResourceManager(card)
conn = res.reserve_connector()
crtc = res.reserve_crtc(conn)
mode = conn.get_default_mode()

planes = []
for p in card.planes:
    if p.supports_crtc(crtc) == False:
        continue
    planes.append(p)

card.disable_planes()

w = mode.hdisplay
h = mode.vdisplay

fbs=[]

# See Figure 11-78. DISPC Destination Transparency Color Key Example
def test_am5_trans_dest():
    fbs.append(pykms.DumbFramebuffer(card, w, h, "XR24"))
    fbs.append(pykms.DumbFramebuffer(card, w, h, "XR24"))

    fb = fbs[0]
    pykms.draw_rect(fb, 0, 0, fb.width, fb.height, pykms.purple)
    pykms.draw_rect(fb, 100, 100, 100, 200, pykms.green)
    pykms.draw_rect(fb, 300, 100, 100, 200, pykms.red)
    pykms.draw_rect(fb, 500, 100, 100, 200, pykms.white)

    fb = fbs[1]
    pykms.draw_rect(fb, 0, 0, fb.width, fb.height, pykms.cyan)
    pykms.draw_circle(fb, 350, 200, 100, pykms.yellow)

    crtc.set_props({
        "trans-key-mode": 1,
        "trans-key": pykms.purple.rgb888,
        "background": 0,
        "alpha_blender": 0,
    })

    print("Purple bg. Green, red, white boxes.")

    plane = planes[0]
    fb = fbs[0]
    z = 0

    plane.set_props({
        "FB_ID": fb.id,
        "CRTC_ID": crtc.id,
        "SRC_W": fb.width << 16,
        "SRC_H": fb.height << 16,
        "CRTC_W": fb.width,
        "CRTC_H": fb.height,
        "zorder": z,
    })

    input("press enter\n")

    print("Cyan bg. Green, red, white boxes. Yellow circle behind the red box.")

    plane = planes[1]
    fb = fbs[1]
    z = 1

    plane.set_props({
        "FB_ID": fb.id,
        "CRTC_ID": crtc.id,
        "SRC_W": fb.width << 16,
        "SRC_H": fb.height << 16,
        "CRTC_W": fb.width,
        "CRTC_H": fb.height,
        "zorder": z,
    })

    input("press enter\n")

# See Figure 11-77. DISPC Source Transparency Color Key Example
def test_am5_trans_src():
    fbs.append(pykms.DumbFramebuffer(card, w, h, "XR24"))
    fbs.append(pykms.DumbFramebuffer(card, w, h, "XR24"))

    fb = fbs[0]
    pykms.draw_rect(fb, 0, 0, fb.width, fb.height, pykms.white)
    pykms.draw_rect(fb, 200, 200, 100, 100, pykms.red)
    pykms.draw_rect(fb, fb.width - 300, 200, 100, 100, pykms.green)

    fb = fbs[1]
    pykms.draw_rect(fb, 0, 0, fb.width, fb.height, pykms.cyan)
    pykms.draw_rect(fb, 100, 100, fb.width - 200, fb.height - 200, pykms.purple)

    crtc.set_props({
        "trans-key-mode": 2,
        "trans-key": pykms.purple.rgb888,
        "background": 0,
        "alpha_blender": 0,
    })

    print("White bg. Red and green boxes.")

    plane = planes[0]
    fb = fbs[0]
    z = 0

    plane.set_props({
        "FB_ID": fb.id,
        "CRTC_ID": crtc.id,
        "SRC_W": fb.width << 16,
        "SRC_H": fb.height << 16,
        "CRTC_W": fb.width,
        "CRTC_H": fb.height,
        "zorder": z,
    })

    input("press enter\n")

    print("Cyan bg. Big white box, containing red and green boxes.")

    plane = planes[1]
    fb = fbs[1]
    z = 3

    plane.set_props({
        "FB_ID": fb.id,
        "CRTC_ID": crtc.id,
        "SRC_W": fb.width << 16,
        "SRC_H": fb.height << 16,
        "CRTC_W": fb.width,
        "CRTC_H": fb.height,
        "zorder": z,
    })

    input("press enter\n")

def test_am4_normal_trans_dst():
    fbs.append(pykms.DumbFramebuffer(card, w, h, "XR24"))
    fbs.append(pykms.DumbFramebuffer(card, w * 2 // 3, h, "XR24"))
    fbs.append(pykms.DumbFramebuffer(card, w * 2 // 3, h, "XR24"))

    fb = fbs[0]
    pykms.draw_rect(fb, 0, 0, w, h, pykms.purple)
    pykms.draw_rect(fb, 100, 50, 50, 200, pykms.green)
    pykms.draw_rect(fb, 200, 50, 50, 200, pykms.red)
    pykms.draw_rect(fb, 300, 50, 50, 200, pykms.white)

    fb = fbs[1]
    pykms.draw_rect(fb, 0, 0, fb.width, fb.height, pykms.blue)

    fb = fbs[2]
    pykms.draw_rect(fb, 0, 0, fb.width, fb.height, pykms.cyan)

    crtc.set_props({
        "trans-key-mode": 1,
        "trans-key": pykms.purple.rgb888,
        "background": 0,
        "alpha_blender": 0,
    })

    print("Purple bg. Green, red, white boxes.")

    plane = planes[0]
    fb = fbs[0]
    plane.set_props({
        "FB_ID": fb.id,
        "CRTC_ID": crtc.id,
        "SRC_W": fb.width << 16,
        "SRC_H": fb.height << 16,
        "CRTC_W": w,
        "CRTC_H": h,
    })

    input("press enter\n")

    print("Blue bg on left, covering green, red, white boxes. Purple bg on right.")

    plane = planes[1]
    fb = fbs[1]
    plane.set_props({
        "FB_ID": fb.id,
        "CRTC_ID": crtc.id,
        "SRC_X": 0 << 16,
        "SRC_Y": 0 << 16,
        "SRC_W": fb.width << 16,
        "SRC_H": fb.height << 16,
        "CRTC_X": 0,
        "CRTC_Y": 0,
        "CRTC_W": fb.width,
        "CRTC_H": fb.height,
    })

    input("press enter\n")

    print("Blue bg on left, covering green and red boxes. Cyan bg on right, covering white box.")

    plane = planes[2]
    fb = fbs[2]
    plane.set_props({
        "FB_ID": fb.id,
        "CRTC_ID": crtc.id,
        "SRC_X": 0 << 16,
        "SRC_Y": 0 << 16,
        "SRC_W": fb.width << 16,
        "SRC_H": fb.height << 16,
        "CRTC_X": w // 3,
        "CRTC_Y": 0,
        "CRTC_W": fb.width,
        "CRTC_H": fb.height,
    })

    input("press enter\n")

def test_am4_normal_trans_src():
    fbs.append(pykms.DumbFramebuffer(card, w, h, "XR24"))
    fbs.append(pykms.DumbFramebuffer(card, w // 2, h, "XR24"))
    fbs.append(pykms.DumbFramebuffer(card, w // 2, h, "XR24"))

    fb = fbs[0]
    pykms.draw_rect(fb, 0, 0, w, h, pykms.RGB(128, 255, 255))
    pykms.draw_rect(fb, 200, 100, 50, 200, pykms.red)
    pykms.draw_rect(fb, w - 200 - 50, 100, 50, 200, pykms.green)

    fb = fbs[1]
    pykms.draw_rect(fb, 0, 0, fb.width, fb.height, pykms.blue)
    pykms.draw_rect(fb, 100, 100, fb.width - 200, fb.height - 200, pykms.purple)

    fb = fbs[2]
    pykms.draw_rect(fb, 0, 0, fb.width, fb.height, pykms.cyan)
    pykms.draw_rect(fb, 100, 100, fb.width - 200, fb.height - 200, pykms.purple)

    crtc.set_props({
        "trans-key-mode": 2,
        "trans-key": pykms.purple.rgb888,
        "background": 0,
        "alpha_blender": 0,
    })

    time.sleep(1)

    plane = planes[0]
    fb = fbs[0]
    plane.set_props({
        "FB_ID": fb.id,
        "CRTC_ID": crtc.id,
        "SRC_W": fb.width << 16,
        "SRC_H": fb.height << 16,
        "CRTC_W": w,
        "CRTC_H": h,
    })

    time.sleep(1)

    plane = planes[1]
    fb = fbs[1]
    plane.set_props({
        "FB_ID": fb.id,
        "CRTC_ID": crtc.id,
        "SRC_X": 0 << 16,
        "SRC_Y": 0 << 16,
        "SRC_W": fb.width << 16,
        "SRC_H": fb.height << 16,
        "CRTC_X": 0,
        "CRTC_Y": 0,
        "CRTC_W": fb.width,
        "CRTC_H": fb.height,
    })

    time.sleep(1)

    plane = planes[2]
    fb = fbs[2]
    plane.set_props({
        "FB_ID": fb.id,
        "CRTC_ID": crtc.id,
        "SRC_X": 0 << 16,
        "SRC_Y": 0 << 16,
        "SRC_W": fb.width << 16,
        "SRC_H": fb.height << 16,
        "CRTC_X": w - fb.width,
        "CRTC_Y": 0,
        "CRTC_W": fb.width,
        "CRTC_H": fb.height,
    })

def test_am4_alpha_trans_src():
    fbs.append(pykms.DumbFramebuffer(card, w, h, "XR24"))
    fbs.append(pykms.DumbFramebuffer(card, w // 2, h, "XR24"))
    fbs.append(pykms.DumbFramebuffer(card, w // 2, h, "XR24"))

    fb = fbs[0]
    pykms.draw_rect(fb, 0, 0, w, h, pykms.purple)
    pykms.draw_rect(fb, 200, 100, 50, 200, pykms.red)
    pykms.draw_rect(fb, w - 200 - 50, 100, 50, 200, pykms.green)

    fb = fbs[1]
    pykms.draw_rect(fb, 0, 0, fb.width, fb.height, pykms.blue)
    pykms.draw_rect(fb, 100, 100, fb.width - 200, fb.height - 200, pykms.purple)

    fb = fbs[2]
    pykms.draw_rect(fb, 0, 0, fb.width, fb.height, pykms.cyan)
    pykms.draw_rect(fb, 100, 100, fb.width - 200, fb.height - 200, pykms.purple)

    crtc.set_props({
        "trans-key-mode": 1,
        "trans-key": pykms.purple.rgb888,
        "background": 0,
        "alpha_blender": 1,
    })

    time.sleep(1)

    plane = planes[0]
    fb = fbs[0]
    plane.set_props({
        "FB_ID": fb.id,
        "CRTC_ID": crtc.id,
        "SRC_W": fb.width << 16,
        "SRC_H": fb.height << 16,
        "CRTC_W": w,
        "CRTC_H": h,
    })

    time.sleep(1)

    plane = planes[1]
    fb = fbs[1]
    plane.set_props({
        "FB_ID": fb.id,
        "CRTC_ID": crtc.id,
        "SRC_X": 0 << 16,
        "SRC_Y": 0 << 16,
        "SRC_W": fb.width << 16,
        "SRC_H": fb.height << 16,
        "CRTC_X": 0,
        "CRTC_Y": 0,
        "CRTC_W": fb.width,
        "CRTC_H": fb.height,
    })

    time.sleep(1)

    plane = planes[2]
    fb = fbs[2]
    plane.set_props({
        "FB_ID": fb.id,
        "CRTC_ID": crtc.id,
        "SRC_X": 0 << 16,
        "SRC_Y": 0 << 16,
        "SRC_W": fb.width << 16,
        "SRC_H": fb.height << 16,
        "CRTC_X": w - fb.width,
        "CRTC_Y": 0,
        "CRTC_W": fb.width,
        "CRTC_H": fb.height,
    })


if TEST == 1:
    test_am5_trans_dest()
elif TEST == 2:
    test_am5_trans_src()
elif TEST == 3:
    test_am4_normal_trans_dst()
elif TEST == 4:
    test_am4_normal_trans_src()
elif TEST == 5:
    test_am4_alpha_trans_src()
else:
    print("Bad test number")
    exit(-1)
