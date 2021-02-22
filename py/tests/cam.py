#!/usr/bin/python3

import sys
import selectors
import pykms
import argparse
import time
from collections import deque
import math

print("Configure media entities")

md = pykms.MediaDevice("/dev/media0")

SHOW_EMBEDDED = True

cameras = [
    { "sensor": "ov10635 5-0030", "w": 1280, "h": 720, "busfmt": pykms.BusFormat.YUYV8_2X8 },
    { "sensor": "ov10635 6-0030", "w": 752, "h": 480, "busfmt": pykms.BusFormat.YUYV8_2X8 },
]

for camera in cameras:
    ov10635 = md.find_entity(camera["sensor"])
    links = ov10635.get_links(0)

    assert(len(links) == 1)

    link = links[0]

    ub960 = link.sink
    ub_sink_pad = link.sink_pad

    ub_routing = ub960.subdev.get_routing()

    ub_out_streams = []

    streams = []

    for ub_route in ub_routing:
        if ub_route.sink_pad != ub_sink_pad:
            continue

        stream = { "ub_source_pad": ub_route.source_pad, "ub_source_stream": ub_route.source_stream }

        links = ub960.get_links(ub_route.source_pad)
        assert(len(links) == 1)
        link = links[0]

        rx = link.sink
        rx_sink_pad = link.sink_pad

        stream.update({ "rx": rx.name, "rx_sink_pad": rx_sink_pad })

        for rx_route in rx.subdev.get_routing():
            if rx_route.sink_pad != rx_sink_pad:
                continue

            if rx_route.sink_stream != ub_route.source_stream:
                continue

            stream.update({ "rx_source_pad": rx_route.source_pad })

            links = rx.get_links(rx_route.source_pad)
            assert(len(links) == 1)
            link = links[0]

            vid = link.sink

            stream["dev"] = vid.dev_path

        streams.append(stream)


    print("Camera", ov10635.name)
    for stream in streams:
        print("    ", stream)

    camera["streams"] = streams

    # Set format between OV10635 outout pad and UB960 input pad
    ov10635.subdev.set_format(0, camera["w"], camera["h"], camera["busfmt"])
    ub960.subdev.set_format(ub_sink_pad, *ov10635.subdev.get_format(0))

captures = []

for camera in cameras:
    # if a camera has two streams, one of them is embedded data
    has_embedded = len(camera["streams"]) == 2

    for stream in camera["streams"]:
        # odd streams are embedded data
        embedded = has_embedded and stream["ub_source_stream"] % 2 == 1

        if not SHOW_EMBEDDED and embedded:
            continue

        data = { "dev": stream["dev"], "camera": camera, "fmt": pykms.PixelFormat.YUYV,
            "embedded" : embedded }

        captures.append(data)

#print("Capturing in {}x{} {}".format(w, h, fmt))

card = pykms.Card()
res = pykms.ResourceManager(card)
conn = res.reserve_connector()
crtc = res.reserve_crtc(conn)

mode = conn.get_default_mode()
modeb = mode.to_blob(card)

req = pykms.AtomicReq(card)
req.add(conn, "CRTC_ID", crtc.id)
req.add(crtc, {"ACTIVE": 1,
        "MODE_ID": modeb.id})
req.commit_sync(allow_modeset = True)

NUM_BUFS = 5

for i, capture in enumerate(captures):
    capture["planefmt"] = capture["fmt"]

    plane = res.reserve_generic_plane(crtc, capture["fmt"])
    if plane == None:
        plane = res.reserve_generic_plane(crtc, pykms.PixelFormat.RGB565)
        capture["planefmt"] = pykms.PixelFormat.RGB565

    assert(plane)

    capture["plane"] = plane
    capture["w"] = capture["camera"]["w"]
    capture["h"] = capture["camera"]["h"]

    if capture["embedded"]:
        w = int(round(math.sqrt(capture["w"])))
        w //= 8
        w *= 8

        h = int(math.ceil(capture["w"] / w))

        capture["w"] = w
        capture["h"] = h

    if i == 0:
        capture["x"] = 0
        capture["y"] = 0
    elif i == 1:
        capture["x"] = mode.hdisplay - capture["w"]
        capture["y"] = 0
    elif i == 2:
        capture["x"] = 0
        capture["y"] = mode.vdisplay - capture["h"]
    elif i == 3:
        capture["x"] = mode.hdisplay - capture["w"]
        capture["y"] = mode.vdisplay - capture["h"]

for capture in captures:
    vd = pykms.VideoDevice(capture["dev"])
    cap = vd.capture_streamer
    cap.set_port(0)
    cap.set_format(capture["fmt"], capture["w"], capture["h"])
    cap.set_queue_size(NUM_BUFS)

    capture["vd"] = vd
    capture["cap"] = cap

for capture in captures:
    # Allocate FBs
    fbs = []
    for i in range(NUM_BUFS):
        fb = pykms.DumbFramebuffer(card, capture["w"], capture["h"], capture["planefmt"])
        fbs.append(fb)
    capture["fbs"] = fbs

    # Set fb0 to screen
    fb = capture["fbs"][0]
    plane = capture["plane"]

    plane.set_props({
        "FB_ID": fb.id,
        "CRTC_ID": crtc.id,
        "SRC_W": fb.width << 16,
        "SRC_H": fb.height << 16,
        "CRTC_X": capture["x"],
        "CRTC_Y": capture["y"],
        "CRTC_W": fb.width,
        "CRTC_H": fb.height,
    })

    capture["kms_old_fb"] = None
    capture["kms_fb"] = fb
    capture["kms_fb_queue"] = deque()

    # Queue the rest to the camera
    cap = capture["cap"]
    for i in range(1, NUM_BUFS):
        cap.queue(fbs[i])

for data in captures:
    data["num_frames"] = 0
    data["time"] = time.perf_counter()

    #print("ENABLE STREAM")
    print(f'{data["dev"]}: stream on')
    data["cap"].stream_on()
    #time.sleep(0.5)
    #print("ENABLED STREAM")
    #time.sleep(1)

kms_committed = False

def readvid(data):
    data["num_frames"] += 1

    if data["num_frames"] == 100:
        diff = time.perf_counter() - data["time"]

        print("{}: 100 frames in {:.2f} s, {:.2f} fps".format(data["dev"], diff, 100 / diff))

        data["num_frames"] = 0
        data["time"] = time.perf_counter()

    cap = data["cap"]
    fb = cap.dequeue()

    data["kms_fb_queue"].append(fb)

    if len(data["kms_fb_queue"]) >= NUM_BUFS - 1:
        print("WARNING fb_queue {}".format(len(data["kms_fb_queue"])))

    #print(f'Buf from {data["dev"]}: kms_fb_queue {len(data["kms_fb_queue"])}, commit ongoing {kms_committed}')

    # XXX with a small delay we might get more planes to the commit
    if kms_committed == False:
        handle_pageflip()

def readkey(conn, mask):
    for data in reversed(captures):
        print(f'{data["dev"]}: stream off')
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

    for data in captures:
        #print(f'Page flip {data["dev"]}: kms_fb_queue {len(data["kms_fb_queue"])}, new_fb {data["kms_fb"]}, old_fb {data["kms_old_fb"]}')

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
for data in captures:
    sel.register(data["cap"].fd, selectors.EVENT_READ, lambda c,m,d=data: readvid(d))

while True:
    events = sel.select()
    for key, mask in events:
        callback = key.data
        callback(key.fileobj, mask)
