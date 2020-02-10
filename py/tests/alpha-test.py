#!/usr/bin/python3

import pykms
import time
import argparse

parser = argparse.ArgumentParser(description='Simple alpha blending test.')
parser.add_argument('--resetcrtc', action="store_true",
		    help='Reset legacy CRTC color properties')
parser.add_argument('--connector', '-c', dest='connector', default="",
		    required=False, help='connector to output')
parser.add_argument('--mode', '-m', dest='modename',
		    required=False, help='Video mode name to use')
args = parser.parse_args()

max_planes = 4

card = pykms.Card()
res = pykms.ResourceManager(card)
conn = res.reserve_connector(args.connector)
crtc = res.reserve_crtc(conn)
if args.modename == None:
    mode = conn.get_default_mode()
else:
    mode = conn.get_mode(args.modename)

planes = []

for i in range(max_planes):
    p = res.reserve_generic_plane(crtc)
    if p == None:
        break
    planes.append(p)

print("Got {} planes. Test supports up to 4 planes.".format(len(planes)))

card.disable_planes()

w = mode.hdisplay
h = mode.vdisplay

fbs=[]

for i in range(max_planes):
    fbs.append(pykms.DumbFramebuffer(card, w, h, "AR24"))

pykms.draw_rect(fbs[0], 50, 50, 200, 200, pykms.RGB(128, 255, 0, 0))
pykms.draw_rect(fbs[1], 150, 50, 200, 200, pykms.RGB(128, 0, 255, 0))
pykms.draw_rect(fbs[2], 50, 150, 200, 200, pykms.RGB(128, 0, 0, 255))
pykms.draw_rect(fbs[3], 150, 150, 200, 200, pykms.RGB(128, 128, 128, 128))

card.disable_planes()

if args.resetcrtc:
    crtc.set_props({
        "trans-key-mode": 0,
        "trans-key": 0,
        "background": 0,
        "alpha_blender": 1,
    })

for i in range(len(planes)):
    plane = planes[i]
    fb = fbs[i]

    print("set crtc {}, plane {}, z {}, fb {}".format(crtc.id, plane.id, i, fb.id))

    plane.set_props({
        "FB_ID": fb.id,
        "CRTC_ID": crtc.id,
        "SRC_W": fb.width << 16,
        "SRC_H": fb.height << 16,
        "CRTC_W": fb.width,
        "CRTC_H": fb.height,
        "zpos": i,
    })

    time.sleep(1)

input("press enter to exit\n")
