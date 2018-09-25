#!/usr/bin/python3

import pykms
import time
import sys
import argparse

tests = {
    1: "test_am5_trans_dest",
    2: "test_am5_trans_src",
    3: "test_am4_normal_trans_dst",
    4: "test_am4_normal_trans_src",
    5: "test_am4_alpha_trans_src",
}

parser = argparse.ArgumentParser()
parser.add_argument("-c", "--connector", default="")
parser.add_argument("test", type=int, help="test number 1-" + str(len(tests)))
args = parser.parse_args()

#if len(sys.argv) != 2:
#    print("Usage: {} <test-number>".format(sys.argv[0]))
#    print("  1 - test_am5_trans_dest()")
#    print("  2 - test_am5_trans_src()")
#    print("  3 - test_am4_normal_trans_dst()")
#    print("  4 - test_am4_normal_trans_src()")
#    print("  5 - test_am4_alpha_trans_src()")
#    exit(0)

TEST = args.test

card = pykms.Card()
res = pykms.ResourceManager(card)
conn = res.reserve_connector(args.connector)
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
    stepX = fb.width // 7
    stepY = fb.height // 5;

    pykms.draw_rect(fb, 0, 0, fb.width, fb.height, pykms.purple)
    pykms.draw_rect(fb, stepX, stepY,
                    stepX, fb.height - (stepY * 2),
                    pykms.green)
    pykms.draw_rect(fb, stepX * 3, stepY,
                    stepX, fb.height - (stepY * 2),
                    pykms.red)
    pykms.draw_rect(fb, stepX * 5, stepY,
                    stepX, fb.height - (stepY * 2),
                    pykms.white)

    fb = fbs[1]
    pykms.draw_rect(fb, 0, 0,
                    fb.width, fb.height,
                    pykms.cyan)
    pykms.draw_circle(fb, (stepX * 3) + (stepX // 2), fb.height // 2,
                      (fb.height // 2) - stepY,
                      pykms.yellow)

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
        "zpos": z,
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
        "zpos": z,
    })

    input("press enter\n")

# See Figure 11-77. DISPC Source Transparency Color Key Example
def test_am5_trans_src():
    fbs.append(pykms.DumbFramebuffer(card, w, h, "XR24"))
    fbs.append(pykms.DumbFramebuffer(card, w, h, "XR24"))

    fb = fbs[0]
    halfX = fb.width // 2
    stepX = (fb.width // 2) // 5;
    stepY = fb.height // 5;

    pykms.draw_rect(fb, 0, 0, fb.width, fb.height, pykms.white)
    pykms.draw_rect(fb, stepX * 2, stepY * 2,
                    halfX - (stepX * 4), fb.height - (stepY * 4),
                    pykms.red)
    pykms.draw_rect(fb, halfX + stepX * 2, stepY * 2,
                    halfX - (stepX * 4), fb.height - (stepY * 4),
                    pykms.green)

    fb = fbs[1]
    pykms.draw_rect(fb, 0, 0, fb.width, fb.height, pykms.cyan)
    pykms.draw_rect(fb, stepX, stepY,
                    fb.width - (stepX * 2), fb.height - (stepY * 2),
                    pykms.purple)

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
        "zpos": z,
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
        "zpos": z,
    })

    input("press enter\n")

def test_am4_normal_trans_dst():
    fbs.append(pykms.DumbFramebuffer(card, w, h, "XR24"))
    fbs.append(pykms.DumbFramebuffer(card, w, h, "XR24"))

    fb = fbs[0]
    stepX = fb.width // 7
    stepY = fb.height // 5;

    pykms.draw_rect(fb, 0, 0, fb.width, fb.height, pykms.purple)
    pykms.draw_rect(fb, stepX, stepY,
                    stepX, fb.height - (stepY * 2),
                    pykms.green)
    pykms.draw_rect(fb, stepX * 3, stepY,
                    stepX, fb.height - (stepY * 2),
                    pykms.red)
    pykms.draw_rect(fb, stepX * 5, stepY,
                    stepX, fb.height - (stepY * 2),
                    pykms.white)

    fb = fbs[1]
    pykms.draw_rect(fb, 0, 0,
                    fb.width, fb.height,
                    pykms.cyan)
    pykms.draw_circle(fb, (stepX * 3) + (stepX // 2), fb.height // 2,
                      (fb.height // 2) - stepY,
                      pykms.yellow)

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
        "zpos": z,
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
        "zpos": z,
    })

    input("press enter\n")

def test_am4_normal_trans_src():
    fbs.append(pykms.DumbFramebuffer(card, w, h, "XR24"))
    fbs.append(pykms.DumbFramebuffer(card, w, h, "XR24"))

    fb = fbs[0]
    halfX = fb.width // 2
    stepX = (fb.width // 2) // 5;
    stepY = fb.height // 5;

    pykms.draw_rect(fb, 0, 0, fb.width, fb.height, pykms.white)
    pykms.draw_rect(fb, stepX * 2, stepY * 2,
                    halfX - (stepX * 4), fb.height - (stepY * 4),
                    pykms.red)
    pykms.draw_rect(fb, halfX + stepX * 2, stepY * 2,
                    halfX - (stepX * 4), fb.height - (stepY * 4),
                    pykms.green)

    fb = fbs[1]
    pykms.draw_rect(fb, 0, 0, fb.width, fb.height, pykms.cyan)
    pykms.draw_rect(fb, stepX, stepY,
                    fb.width - (stepX * 2), fb.height - (stepY * 2),
                    pykms.purple)

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
        "zpos": z,
    })

    input("press enter\n")

    print("Cyan bg. Big white box, containing red and green boxes.")

    plane = planes[1]
    fb = fbs[1]
    z = 2

    plane.set_props({
        "FB_ID": fb.id,
        "CRTC_ID": crtc.id,
        "SRC_W": fb.width << 16,
        "SRC_H": fb.height << 16,
        "CRTC_W": fb.width,
        "CRTC_H": fb.height,
        "zpos": z,
    })

    input("press enter\n")

def test_am4_alpha_trans_src():
    fbs.append(pykms.DumbFramebuffer(card, w, h, "XR24"))
    fbs.append(pykms.DumbFramebuffer(card, w // 2, h, "XR24"))
    fbs.append(pykms.DumbFramebuffer(card, w // 2, h, "XR24"))

    fb = fbs[0]
    halfX = fb.width // 2
    stepX = (fb.width // 2) // 5;
    stepY = fb.height // 5;

    pykms.draw_rect(fb, 0, 0, w, h, pykms.purple)
    pykms.draw_rect(fb, stepX * 2, stepY * 2,
                    halfX - (stepX * 4), fb.height - (stepY * 4),
                    pykms.red)
    pykms.draw_rect(fb, halfX + stepX * 2, stepY * 2,
                    halfX - (stepX * 4), fb.height - (stepY * 4),
                    pykms.green)

    fb = fbs[1]
    pykms.draw_rect(fb, 0, 0, fb.width, fb.height, pykms.blue)
    pykms.draw_rect(fb, stepX, stepY,
                    fb.width - (stepX * 2), fb.height - (stepY * 2),
                    pykms.purple)

    fb = fbs[2]
    pykms.draw_rect(fb, 0, 0, fb.width, fb.height, pykms.cyan)
    pykms.draw_rect(fb, stepX, stepY,
                    fb.width - (stepX * 2), fb.height - (stepY * 2),
                    pykms.purple)

    crtc.set_props({
        "trans-key-mode": 1,
        "trans-key": pykms.purple.rgb888,
        "background": 0x666666,
        "alpha_blender": 1,
    })

    print("grey background")
    input("press enter\n")

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

    print("grey background, red and green boxes")
    input("press enter\n")

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

    print("left side: blue bg, purple box, red box inside purple. right side: unchanged")
    input("press enter\n")

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

    print("left side: unchanged. right side: cyan bg, purple box, green box inside purple.")
    input("press enter\n")

print(tests[args.test])
locals()[tests[args.test]]()
