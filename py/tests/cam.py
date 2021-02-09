#!/usr/bin/python3

import sys
import selectors
import pykms
import argparse
import time
from collections import deque

#parser = argparse.ArgumentParser()
#parser.add_argument("-d", "--device", type=str, default="/dev/video0")
#parser.add_argument("-m", "--media", type=str, default="/dev/media0")
#parser.add_argument("width", type=int)
#parser.add_argument("height", type=int)
#parser.add_argument("fourcc", type=str, nargs="?", default="YUVY")
#args = parser.parse_args()

#w = args.width
#h = args.height
#fmt = pykms.fourcc_to_pixelformat(args.fourcc)

w = 752
h = 480
fmt = pykms.PixelFormat.YUYV
busfmt = pykms.BusFormat.YUYV8_2X8

print("Configure media entities")

md = pykms.MediaDevice("/dev/media0")

ov10635_0 = md.find_entity("ov10635 5-0030")
ov10635_1 = md.find_entity("ov10635 6-0030")
ub960 = md.find_entity("ds90ub960")
rx0 = md.find_entity("CAMERARX0")
#  cal = md.find_entity("CAL output 0")

ov10635_0.subdev.set_format(0, w, h, busfmt)
ov10635_1.subdev.set_format(0, w, h, busfmt)

ub960.subdev.set_format(0, *ov10635_0.subdev.get_format(0))    # input 0
ub960.subdev.set_format(1, *ov10635_1.subdev.get_format(0))    # input 1
ub960.subdev.set_format(4, w, h, busfmt)    # output 0

#routes = ub960.subdev.get_routing()
#print(routes)
#exit(0)

rx0.subdev.set_format(0, *ub960.subdev.get_format(4))
rx0.subdev.set_format(1, w, h, busfmt)


print("Capturing in {}x{} {}".format(w, h, fmt))

card = pykms.Card()
res = pykms.ResourceManager(card)
conn = res.reserve_connector()
crtc = res.reserve_crtc(conn)
plane1 = res.reserve_generic_plane(crtc, fmt)
plane2 = res.reserve_generic_plane(crtc, pykms.PixelFormat.YUYV)
plane3 = res.reserve_generic_plane(crtc, fmt)
plane4 = res.reserve_generic_plane(crtc, pykms.PixelFormat.RGB565)

assert(plane1 and plane2 and plane3 and plane4)

mode = conn.get_default_mode()
modeb = mode.to_blob(card)

req = pykms.AtomicReq(card)
req.add(conn, "CRTC_ID", crtc.id)
req.add(crtc, {"ACTIVE": 1,
        "MODE_ID": modeb.id})
req.commit_sync(allow_modeset = True)

NUM_BUFS = 5

sensors = [
    { "idx": 0, "path": "/dev/video0", "plane": plane1, "x": 0, "y": 0, "hack": False },
    { "idx": 1, "path": "/dev/video1", "plane": plane2, "x": mode.hdisplay - w, "y": 0, "hack": False },
    { "idx": 2, "path": "/dev/video2", "plane": plane3, "x": 0, "y": mode.vdisplay - h, "hack": False },
    { "idx": 3, "path": "/dev/video3", "plane": plane4, "x": mode.hdisplay - w, "y": mode.vdisplay - h, "hack": True },
]

for data in sensors:
    vd = pykms.VideoDevice(data["path"])
    cap = vd.capture_streamer
    cap.set_port(0)
    cap.set_format(fmt, w, h)
    cap.set_queue_size(NUM_BUFS)

    data["vd"] = vd
    data["cap"] = cap

for data in sensors:
    # Allocate FBs
    fbs = []
    for i in range(NUM_BUFS):
        fb = pykms.DumbFramebuffer(card, w, h, pykms.PixelFormat.RGB565 if data["hack"] else fmt)
        fbs.append(fb)
    data["fbs"] = fbs

    # Set fb0 to screen
    fb = data["fbs"][0]
    plane = data["plane"]

    plane.set_props({
        "FB_ID": fb.id,
        "CRTC_ID": crtc.id,
        "SRC_W": fb.width << 16,
        "SRC_H": fb.height << 16,
        "CRTC_X": data["x"],
        "CRTC_Y": data["y"],
        "CRTC_W": fb.width,
        "CRTC_H": fb.height,
    })

    data["kms_old_fb"] = None
    data["kms_fb"] = fb
    data["kms_fb_queue"] = deque()

    # Queue the rest to the camera
    cap = data["cap"]
    for i in range(1, NUM_BUFS):
        cap.queue(fbs[i])

for data in sensors:
    data["num_frames"] = 0
    data["time"] = time.perf_counter()

    #print("ENABLE STREAM")
    print(f'{data["path"]}: stream on')
    data["cap"].stream_on()
    #time.sleep(0.5)
    #print("ENABLED STREAM")
    #time.sleep(1)

kms_committed = False

def readvid(data):
    data["num_frames"] += 1

    if data["num_frames"] == 100:
        diff = time.perf_counter() - data["time"]

        print("{}: 100 frames in {:.2f} s, {:.2f} fps".format(data["path"], diff, 100 / diff))

        data["num_frames"] = 0
        data["time"] = time.perf_counter()

    cap = data["cap"]
    fb = cap.dequeue()

    data["kms_fb_queue"].append(fb)

    if len(data["kms_fb_queue"]) >= NUM_BUFS - 1:
        print("WARNING fb_queue {}".format(len(data["kms_fb_queue"])))

    #print(f'Buf from {data["path"]}: kms_fb_queue {len(data["kms_fb_queue"])}, commit ongoing {kms_committed}')

    # XXX with a small delay we might get more planes to the commit
    if kms_committed == False:
        handle_pageflip()

def readkey(conn, mask):
    for data in reversed(sensors):
        print(f'{data["path"]}: stream off')
        data["cap"].stream_off()
        #time.sleep(0.5)
        #print("DISABLED CAP")
        #time.sleep(1)

    print("Done")
    sys.stdin.readline()
    exit(0)

def handle_pageflip():
    global kms_committed

    kms_committed = False

    req = pykms.AtomicReq(card)

    do_commit = False

    for data in sensors:
        #print(f'Page flip {data["path"]}: kms_fb_queue {len(data["kms_fb_queue"])}, new_fb {data["kms_fb"]}, old_fb {data["kms_old_fb"]}')

        cap = data["cap"]

        if data["kms_old_fb"]:
            cap.queue(data["kms_old_fb"])
            data["kms_old_fb"] = None

        if len(data["kms_fb_queue"]) == 0:
            continue

        data["kms_old_fb"] = data["kms_fb"]

        fb = data["kms_fb_queue"].popleft()
        data["kms_fb"] = fb

        plane = data["plane"]

        req.add(plane, "FB_ID", fb.id)

        do_commit = True

    if do_commit:
        req.commit(allow_modeset = False)
        kms_committed = True


def readdrm(fileobj, mask):
    #print("EVENT");
    for ev in card.read_events():
        if ev.type == pykms.DrmEventType.FLIP_COMPLETE:
            handle_pageflip()

sel = selectors.DefaultSelector()
sel.register(sys.stdin, selectors.EVENT_READ, readkey)
sel.register(card.fd, selectors.EVENT_READ, readdrm)
for data in sensors:
    sel.register(data["cap"].fd, selectors.EVENT_READ, lambda c,m,d=data: readvid(d))

while True:
    events = sel.select()
    for key, mask in events:
        callback = key.data
        callback(key.fileobj, mask)
