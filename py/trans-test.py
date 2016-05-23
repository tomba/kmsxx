#!/usr/bin/python3

import pykms
from helpers import *
import time

# This hack makes drm initialize the fbcon, setting up the default connector
card = pykms.Card()
card = 0

card = pykms.Card()

conn = card.get_first_connected_connector()
mode = conn.get_default_mode()
crtc = conn.get_current_crtc()

planes = []
for p in card.planes:
    if p.supports_crtc(crtc) == False:
        continue
    planes.append(p)

disable_planes(card)

w = mode.hdisplay
h = mode.vdisplay

fbs=[]

def test_am5_trans_dest():
    fbs.append(pykms.DumbFramebuffer(card, w, h, "XR24"))
    fbs.append(pykms.DumbFramebuffer(card, w, h, "XR24"))

    fb = fbs[0]
    pykms.draw_rect(fb, 0, 0, fb.width, fb.height, purple)
    pykms.draw_rect(fb, 100, 100, 100, 200, green)
    pykms.draw_rect(fb, 300, 100, 100, 200, red)
    pykms.draw_rect(fb, 500, 100, 100, 200, white)

    fb = fbs[1]
    pykms.draw_rect(fb, 0, 0, fb.width, fb.height, cyan)
    pykms.draw_rect(fb, 250, 100, 200, 200, yellow)

    set_props(crtc, {
        "trans-key-mode": 1,
        "trans-key": purple.rgb888,
        "background": 0,
        "alpha_blender": 0,
    })

    plane = 0

    for i in range(0,2):
        print("set crtc {}, plane {}, fb {}".format(crtc.id, planes[i].id, fbs[i].id))

        plane = planes[i]
        fb = fbs[i]
        set_props(plane, {
            "FB_ID": fb.id,
            "CRTC_ID": crtc.id,
            "SRC_W": fb.width << 16,
            "SRC_H": fb.height << 16,
            "CRTC_W": fb.width,
            "CRTC_H": fb.height,
            "zorder": i,
        })

        time.sleep(1)

def test_am5_trans_src():
    fbs.append(pykms.DumbFramebuffer(card, w, h, "XR24"))
    fbs.append(pykms.DumbFramebuffer(card, w, h, "XR24"))

    fb = fbs[0]
    pykms.draw_rect(fb, 0, 0, fb.width, fb.height, white)
    pykms.draw_rect(fb, 200, 200, 100, 100, red)
    pykms.draw_rect(fb, fb.width - 300, 200, 100, 100, green)

    fb = fbs[1]
    pykms.draw_rect(fb, 0, 0, fb.width, fb.height, cyan)
    pykms.draw_rect(fb, 100, 100, 500, 500, purple)

    set_props(crtc, {
        "trans-key-mode": 2,
        "trans-key": purple.rgb888,
        "background": 0,
        "alpha_blender": 0,
    })

    plane = 0

    for i in range(0,2):
        print("set crtc {}, plane {}, fb {}".format(crtc.id, planes[i].id, fbs[i].id))

        plane = planes[i]
        fb = fbs[i]
        set_props(plane, {
            "FB_ID": fb.id,
            "CRTC_ID": crtc.id,
            "SRC_W": fb.width << 16,
            "SRC_H": fb.height << 16,
            "CRTC_W": fb.width,
            "CRTC_H": fb.height,
            "zorder": 3 if i == 1 else 0,
        })

        time.sleep(1)

def test_am4_normal_trans_dst():
    fbs.append(pykms.DumbFramebuffer(card, w, h, "XR24"))
    fbs.append(pykms.DumbFramebuffer(card, w * 2 // 3, h, "XR24"))
    fbs.append(pykms.DumbFramebuffer(card, w * 2 // 3, h, "XR24"))

    fb = fbs[0]
    pykms.draw_rect(fb, 0, 0, w, h, purple)
    pykms.draw_rect(fb, 100, 50, 50, 200, green)
    pykms.draw_rect(fb, 200, 50, 50, 200, red)
    pykms.draw_rect(fb, 300, 50, 50, 200, white)

    fb = fbs[1]
    pykms.draw_rect(fb, 0, 0, fb.width, fb.height, blue)

    fb = fbs[2]
    pykms.draw_rect(fb, 0, 0, fb.width, fb.height, cyan)

    set_props(crtc, {
        "trans-key-mode": 1,
        "trans-key": purple.rgb888,
        "background": 0,
        "alpha_blender": 0,
    })

    time.sleep(1)

    plane = planes[0]
    fb = fbs[0]
    set_props(plane, {
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
    set_props(plane, {
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
    set_props(plane, {
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

def test_am4_normal_trans_src():
    fbs.append(pykms.DumbFramebuffer(card, w, h, "XR24"))
    fbs.append(pykms.DumbFramebuffer(card, w // 2, h, "XR24"))
    fbs.append(pykms.DumbFramebuffer(card, w // 2, h, "XR24"))

    fb = fbs[0]
    pykms.draw_rect(fb, 0, 0, w, h, pykms.RGB(128, 255, 255))
    pykms.draw_rect(fb, 200, 100, 50, 200, red)
    pykms.draw_rect(fb, w - 200 - 50, 100, 50, 200, green)

    fb = fbs[1]
    pykms.draw_rect(fb, 0, 0, fb.width, fb.height, blue)
    pykms.draw_rect(fb, 100, 100, fb.width - 200, fb.height - 200, purple)

    fb = fbs[2]
    pykms.draw_rect(fb, 0, 0, fb.width, fb.height, cyan)
    pykms.draw_rect(fb, 100, 100, fb.width - 200, fb.height - 200, purple)

    set_props(crtc, {
        "trans-key-mode": 2,
        "trans-key": purple.rgb888,
        "background": 0,
        "alpha_blender": 0,
    })

    time.sleep(1)

    plane = planes[0]
    fb = fbs[0]
    set_props(plane, {
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
    set_props(plane, {
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
    set_props(plane, {
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
    pykms.draw_rect(fb, 0, 0, w, h, purple)
    pykms.draw_rect(fb, 200, 100, 50, 200, red)
    pykms.draw_rect(fb, w - 200 - 50, 100, 50, 200, green)

    fb = fbs[1]
    pykms.draw_rect(fb, 0, 0, fb.width, fb.height, blue)
    pykms.draw_rect(fb, 100, 100, fb.width - 200, fb.height - 200, purple)

    fb = fbs[2]
    pykms.draw_rect(fb, 0, 0, fb.width, fb.height, cyan)
    pykms.draw_rect(fb, 100, 100, fb.width - 200, fb.height - 200, purple)

    set_props(crtc, {
        "trans-key-mode": 1,
        "trans-key": purple.rgb888,
        "background": 0,
        "alpha_blender": 1,
    })

    time.sleep(1)

    plane = planes[0]
    fb = fbs[0]
    set_props(plane, {
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
    set_props(plane, {
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
    set_props(plane, {
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



#test_am5_trans_dest()
test_am5_trans_src()
#test_am4_normal_trans_dst()
#test_am4_normal_trans_src()
#test_am4_alpha_trans_src()

input("press enter to exit\n")
