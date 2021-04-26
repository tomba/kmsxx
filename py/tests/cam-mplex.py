#!/usr/bin/python3

import sys
import selectors
import pykms
import argparse
import time
from collections import deque
import math

#CONFIG = "ov5640"

#CONFIG = "dra76-ov5640-mc"
#CONFIG = "dra76-ub960-2-cam"
#CONFIG = "dra76-ub960-1-cam-meta"
CONFIG = "dra76-ub960-2-cam-meta"

#CONFIG = "am6-ov5640-mc"
#CONFIG = "am6-ub960-2-cam"
#CONFIG = "am6-ub960-1-cam-meta"

print("Configure media entities")

md = pykms.MediaDevice("/dev/media0")

META_LINES = 3

# Disable all possible links
def disable_all_links():
    for ent in md.entities:
        for p in range(ent.num_pads):
            for l in [l for l in ent.get_links(p) if not l.immutable]:
                #print((l.sink, l.sink_pad), (l.source, l.source_pad))
                l.enabled = False
                ent.setup_link(l)

disable_all_links()

# Enable link between (src_ent, src_pad) -> (sink_ent, sink_pad)
def link(source, sink):
    #print("LINK", source, sink)

    src_ent = source[0]
    sink_ent = sink[0]

    links = src_ent.get_links(source[1])

    link = None

    for l in links:
        if l.sink == sink_ent and l.sink_pad == sink[1]:
            link = l
            break

    assert(link != None)

    if link.enabled:
        return

    #print("CONF")

    assert(not link.immutable)

    link.enabled = True
    src_ent.setup_link(link)

stream_configurations = {}

#
# Non-MC OV5640
#
stream_configurations["ov5640"] = [
{
    "fmt": pykms.PixelFormat.YUYV,
    "w": 1280,
    "h": 720,
    "embedded": False,
    "dev": "/dev/video0",
}
]

#
# AM6: MC OV5640
#
stream_configurations["am6-ov5640-mc"] = [
{
    "pipe": [
        {
            "entity": "ov5640 3-003c",
            "source": { "pad": 0, "fmt": (1280, 720, pykms.BusFormat.YUYV8_2X8) },
        },
        {
            "entity": "CAMERARX0",
            "sink": { "pad": 0, "stream": 0, "fmt": (1280, 720, pykms.BusFormat.YUYV8_2X8) },
            "source": { "pad": 1, "stream": 0, "fmt": (1280, 720, pykms.BusFormat.YUYV8_2X8) },
        },
        {
            "entity": "CAL output 0",
            "sink": { "pad": 0, "stream": 0, },
        },
    ],

    "fmt": pykms.PixelFormat.YUYV,
    "w": 1280,
    "h": 720,
    "embedded": False,
    "dev": "/dev/video0",
}
]

#
# DRA76: MC OV5640
#
stream_configurations["dra76-ov5640-mc"] = [
{
    "pipe": [
        {
            "entity": "ov5640 4-003c",
            "source": { "pad": 0, "fmt": (1280, 720, pykms.BusFormat.YUYV8_2X8) },
        },
        {
            "entity": "CAMERARX0",
            "sink": { "pad": 0, "stream": 0, "fmt": (1280, 720, pykms.BusFormat.YUYV8_2X8) },
            "source": { "pad": 1, "stream": 0, "fmt": (1280, 720, pykms.BusFormat.YUYV8_2X8) },
        },
        {
            "entity": "CAL output 0",
            "sink": { "pad": 0, "stream": 0, },
        },
    ],

    "fmt": pykms.PixelFormat.YUYV,
    "w": 1280,
    "h": 720,
    "embedded": False,
    "dev": "/dev/video0",
}
]

#
# DRA76: MC UB9060 2 cameras, only pixel streams
#
stream_configurations["dra76-ub960-2-cam"] = [
# Camera 0 pixel stream
{
    "pipe": [
        {
            "entity": "ov10635 5-0030 SENSOR",
            "source": { "pad": 0, "fmt": (1280, 720 - META_LINES, pykms.BusFormat.YUYV8_2X8) },
        },
        {
            "entity": "ov10635 5-0030",
            "sink": { "pad": 0, "stream": 0, },
            "source": { "pad": 2, "stream": 0 },
        },
        {
            "entity": "ds90ub960 4-003d",
            "sink": { "pad": 0, "stream": 0, },
            "source": { "pad": 4, "stream": 0 },
        },
        {
            "entity": "CAMERARX0",
            "sink": { "pad": 0, "stream": 0, },
            "source": { "pad": 1, "stream": 0, "fmt": (1280, 720 - META_LINES, pykms.BusFormat.YUYV8_2X8) },
        },
        {
            "entity": "CAL output 0",
            "sink": { "pad": 0, "stream": 0, },
        },
    ],

    "fmt": pykms.PixelFormat.YUYV,
    "w": 1280,
    "h": 720 - META_LINES,
    "embedded": False,
    "dev": "/dev/video0",
},
# Camera 1 pixel stream
{
    "pipe": [
        {
            "entity": "ov10635 6-0030 SENSOR",
            "source": { "pad": 0, "fmt": (752, 480 - META_LINES, pykms.BusFormat.YUYV8_2X8) },
        },
        {
            "entity": "ov10635 6-0030",
            "sink": { "pad": 0, "stream": 0, },
            "source": { "pad": 2, "stream": 0 },
        },
        {
            "entity": "ds90ub960 4-003d",
            "sink": { "pad": 1, "stream": 0, },
            "source": { "pad": 4, "stream": 2 },
        },
        {
            "entity": "CAMERARX0",
            "sink": { "pad": 0, "stream": 2, },
            "source": { "pad": 3, "stream": 0, "fmt": (752, 480 - META_LINES, pykms.BusFormat.YUYV8_2X8) },
        },
        {
            "entity": "CAL output 2",
            "sink": { "pad": 0, "stream": 0, },
        },
    ],

    "fmt": pykms.PixelFormat.YUYV,
    "w": 752,
    "h": 480 - META_LINES,
    "embedded": False,
    "dev": "/dev/video2",
},
]

#
# DRA76: MC UB9060 2 cameras, pixel and metadata streams
#
stream_configurations["dra76-ub960-2-cam-meta"] = [
# Camera 0 pixel stream
{
    "pipe": [
        {
            "entity": "ov10635 5-0030 SENSOR",
            "source": { "pad": 0, "fmt": (1280, 720 - META_LINES, pykms.BusFormat.YUYV8_2X8) },
        },
        {
            "entity": "ov10635 5-0030",
            "sink": { "pad": 0, "stream": 0, },
            "source": { "pad": 2, "stream": 0 },
        },
        {
            "entity": "ds90ub960 4-003d",
            "sink": { "pad": 0, "stream": 0, },
            "source": { "pad": 4, "stream": 0 },
        },
        {
            "entity": "CAMERARX0",
            "sink": { "pad": 0, "stream": 0, },
            "source": { "pad": 1, "stream": 0, "fmt": (1280, 720 - META_LINES, pykms.BusFormat.YUYV8_2X8) },
        },
        {
            "entity": "CAL output 0",
            "sink": { "pad": 0, "stream": 0, },
        },
    ],

    "fmt": pykms.PixelFormat.YUYV,
    "w": 1280,
    "h": 720 - META_LINES,
    "embedded": False,
    "dev": "/dev/video0",
},
# Camera 0 metadata stream
{
    "pipe": [
        {
            "entity": "ov10635 5-0030 SENSOR",
            "source": { "pad": 1, "fmt": (1280, META_LINES, pykms.BusFormat.METADATA_16) },
        },
        {
            "entity": "ov10635 5-0030",
            "sink": { "pad": 1, "stream": 0, },
            "source": { "pad": 2, "stream": 1 },
        },
        {
            "entity": "ds90ub960 4-003d",
            "sink": { "pad": 0, "stream": 1, },
            "source": { "pad": 4, "stream": 1 },
        },
        {
            "entity": "CAMERARX0",
            "sink": { "pad": 0, "stream": 1, },
            "source": { "pad": 2, "stream": 0, "fmt": (1280, META_LINES, pykms.BusFormat.METADATA_16) },
        },
        {
            "entity": "CAL output 1",
            "sink": { "pad": 0, "stream": 0, },
        },
    ],

    "fmt": pykms.PixelFormat.META_16,
    "w": 1280,
    "h": META_LINES,
    "embedded": True,
    "dev": "/dev/video1",
},
# Camera 1 pixel stream
{
    "pipe": [
        {
            "entity": "ov10635 6-0030 SENSOR",
            "source": { "pad": 0, "fmt": (752, 480 - META_LINES, pykms.BusFormat.YUYV8_2X8) },
        },
        {
            "entity": "ov10635 6-0030",
            "sink": { "pad": 0, "stream": 0, },
            "source": { "pad": 2, "stream": 0 },
        },
        {
            "entity": "ds90ub960 4-003d",
            "sink": { "pad": 1, "stream": 0, },
            "source": { "pad": 4, "stream": 2 },
        },
        {
            "entity": "CAMERARX0",
            "sink": { "pad": 0, "stream": 2, },
            "source": { "pad": 3, "stream": 0, "fmt": (752, 480 - META_LINES, pykms.BusFormat.YUYV8_2X8) },
        },
        {
            "entity": "CAL output 2",
            "sink": { "pad": 0, "stream": 0, },
        },
    ],

    "fmt": pykms.PixelFormat.YUYV,
    "w": 752,
    "h": 480 - META_LINES,
    "embedded": False,
    "dev": "/dev/video2",
},
# Camera 1 metadata stream
{
    "pipe": [
        {
            "entity": "ov10635 6-0030 SENSOR",
            "source": { "pad": 1, "fmt": (752, META_LINES, pykms.BusFormat.METADATA_16) },
        },
        {
            "entity": "ov10635 6-0030",
            "sink": { "pad": 1, "stream": 0, },
            "source": { "pad": 2, "stream": 1 },
        },
        {
            "entity": "ds90ub960 4-003d",
            "sink": { "pad": 1, "stream": 1, },
            "source": { "pad": 4, "stream": 3 },
        },
        {
            "entity": "CAMERARX0",
            "sink": { "pad": 0, "stream": 3, },
            "source": { "pad": 4, "stream": 0, "fmt": (752, META_LINES, pykms.BusFormat.METADATA_16) },
        },
        {
            "entity": "CAL output 3",
            "sink": { "pad": 0, "stream": 0, },
        },
    ],

    "fmt": pykms.PixelFormat.META_16,
    "w": 752,
    "h": META_LINES,
    "embedded": True,
    "dev": "/dev/video3",
}
]

#
# DRA76: MC UB9060 1 camera, pixel and metadata streams
#
stream_configurations["dra76-ub960-1-cam-meta"] = [
# Camera 0 pixel stream
{
    "pipe": [
        {
            "entity": "ov10635 5-0030 SENSOR",
            "source": { "pad": 0, "fmt": (1280, 720 - META_LINES, pykms.BusFormat.YUYV8_2X8) },
        },
        {
            "entity": "ov10635 5-0030",
            "sink": { "pad": 0, "stream": 0, },
            "source": { "pad": 2, "stream": 0 },
        },
        {
            "entity": "ds90ub960 4-003d",
            "sink": { "pad": 0, "stream": 0, },
            "source": { "pad": 4, "stream": 0 },
        },
        {
            "entity": "CAMERARX0",
            "sink": { "pad": 0, "stream": 0, },
            "source": { "pad": 1, "stream": 0, "fmt": (1280, 720 - META_LINES, pykms.BusFormat.YUYV8_2X8) },
        },
        {
            "entity": "CAL output 0",
            "sink": { "pad": 0, "stream": 0, },
        },
    ],

    "fmt": pykms.PixelFormat.YUYV,
    "w": 1280,
    "h": 720 - META_LINES,
    "embedded": False,
    "dev": "/dev/video0",
},
# Camera 0 metadata stream
{
    "pipe": [
        {
            "entity": "ov10635 5-0030 SENSOR",
            "source": { "pad": 1, "fmt": (1280, META_LINES, pykms.BusFormat.METADATA_16) },
        },
        {
            "entity": "ov10635 5-0030",
            "sink": { "pad": 1, "stream": 0, },
            "source": { "pad": 2, "stream": 1 },
        },
        {
            "entity": "ds90ub960 4-003d",
            "sink": { "pad": 0, "stream": 1, },
            "source": { "pad": 4, "stream": 1 },
        },
        {
            "entity": "CAMERARX0",
            "sink": { "pad": 0, "stream": 1, },
            "source": { "pad": 2, "stream": 0, "fmt": (1280, META_LINES, pykms.BusFormat.METADATA_16) },
        },
        {
            "entity": "CAL output 1",
            "sink": { "pad": 0, "stream": 0, },
        },
    ],

    "fmt": pykms.PixelFormat.META_16,
    "w": 1280,
    "h": META_LINES,
    "embedded": True,
    "dev": "/dev/video1",
},
]

#
# AM6: MC UB9060 2 cameras, only pixel streams
#
stream_configurations["am6-ub960-2-cam"] = [
# Camera 0 pixel stream
{
    "pipe": [
        {
            "entity": "ov10635 6-0030 SENSOR",
            "source": { "pad": 0, "fmt": (1280, 720 - META_LINES, pykms.BusFormat.YUYV8_2X8) },
        },
        {
            "entity": "ov10635 6-0030",
            "sink": { "pad": 0, "stream": 0, },
            "source": { "pad": 2, "stream": 0 },
        },
        {
            "entity": "ds90ub960 3-003d",
            "sink": { "pad": 0, "stream": 0, },
            "source": { "pad": 4, "stream": 0 },
        },
        {
            "entity": "CAMERARX0",
            "sink": { "pad": 0, "stream": 0, },
            "source": { "pad": 1, "stream": 0, "fmt": (1280, 720 - META_LINES, pykms.BusFormat.YUYV8_2X8) },
        },
        {
            "entity": "CAL output 0",
            "sink": { "pad": 0, "stream": 0, },
        },
    ],

    "fmt": pykms.PixelFormat.YUYV,
    "w": 1280,
    "h": 720 - META_LINES,
    "embedded": False,
    "dev": "/dev/video0",
},
# Camera 1 pixel stream
{
    "pipe": [
        {
            "entity": "ov10635 7-0030 SENSOR",
            "source": { "pad": 0, "fmt": (752, 480 - META_LINES, pykms.BusFormat.YUYV8_2X8) },
        },
        {
            "entity": "ov10635 7-0030",
            "sink": { "pad": 0, "stream": 0, },
            "source": { "pad": 2, "stream": 0 },
        },
        {
            "entity": "ds90ub960 3-003d",
            "sink": { "pad": 1, "stream": 0, },
            "source": { "pad": 4, "stream": 2 },
        },
        {
            "entity": "CAMERARX0",
            "sink": { "pad": 0, "stream": 2, },
            "source": { "pad": 3, "stream": 0, "fmt": (752, 480 - META_LINES, pykms.BusFormat.YUYV8_2X8) },
        },
        {
            "entity": "CAL output 2",
            "sink": { "pad": 0, "stream": 0, },
        },
    ],

    "fmt": pykms.PixelFormat.YUYV,
    "w": 752,
    "h": 480 - META_LINES,
    "embedded": False,
    "dev": "/dev/video2",
},
]

#
# AM6: MC UB9060 1 camera, pixel and metadata streams
#
stream_configurations["am6-ub960-1-cam-meta"] = [
# Camera 0 pixel stream
{
    "pipe": [
        {
            "entity": "ov10635 6-0030 SENSOR",
            "source": { "pad": 0, "fmt": (1280, 720 - META_LINES, pykms.BusFormat.YUYV8_2X8) },
        },
        {
            "entity": "ov10635 6-0030",
            "sink": { "pad": 0, "stream": 0, },
            "source": { "pad": 2, "stream": 0 },
        },
        {
            "entity": "ds90ub960 3-003d",
            "sink": { "pad": 0, "stream": 0, },
            "source": { "pad": 4, "stream": 0 },
        },
        {
            "entity": "CAMERARX0",
            "sink": { "pad": 0, "stream": 0, },
            "source": { "pad": 1, "stream": 0, "fmt": (1280, 720 - META_LINES, pykms.BusFormat.YUYV8_2X8) },
        },
        {
            "entity": "CAL output 0",
            "sink": { "pad": 0, "stream": 0, },
        },
    ],

    "fmt": pykms.PixelFormat.YUYV,
    "w": 1280,
    "h": 720 - META_LINES,
    "embedded": False,
    "dev": "/dev/video0",
},
# Camera 0 metadata stream
{
    "pipe": [
        {
            "entity": "ov10635 6-0030 SENSOR",
            "source": { "pad": 1, "fmt": (1280, META_LINES, pykms.BusFormat.METADATA_16) },
        },
        {
            "entity": "ov10635 6-0030",
            "sink": { "pad": 1, "stream": 0, },
            "source": { "pad": 2, "stream": 1 },
        },
        {
            "entity": "ds90ub960 3-003d",
            "sink": { "pad": 0, "stream": 1, },
            "source": { "pad": 4, "stream": 1 },
        },
        {
            "entity": "CAMERARX0",
            "sink": { "pad": 0, "stream": 1, },
            "source": { "pad": 2, "stream": 0, "fmt": (1280, META_LINES, pykms.BusFormat.METADATA_16) },
        },
        {
            "entity": "CAL output 1",
            "sink": { "pad": 0, "stream": 0, },
        },
    ],

    "fmt": pykms.PixelFormat.META_16,
    "w": 1280,
    "h": META_LINES,
    "embedded": True,
    "dev": "/dev/video1",
},
]



streams = stream_configurations[CONFIG]

# Resolve entities
for stream in streams:
    if not "pipe" in stream:
        continue

    pipe = stream["pipe"]
    for p in pipe:
        p["entity"] = md.find_entity(p["entity"])
        assert(p["entity"] != None)

# enable links
for stream in streams:
    if not "pipe" in stream:
        continue

    pipe = stream["pipe"]
    for i in range(len(pipe) - 1):
        source_ent = pipe[i]["entity"]
        source_pad = pipe[i]["source"]["pad"]

        sink_ent = pipe[i+1]["entity"]
        sink_pad = pipe[i+1]["sink"]["pad"]

        link((source_ent, source_pad), (sink_ent, sink_pad))

# setup routes

all_entities = set([p["entity"] for s in streams if "pipe" in s for p in s["pipe"]])

for e in all_entities:
    if e.subdev == None:
        continue

    routes = []
    for stream in streams:
        p = next((p for p in stream["pipe"] if p["entity"] == e), None)
        if p == None:
            continue

        if (not "source" in p) or (not "sink" in p):
            continue

        if (not "stream" in p["source"]) or (not "stream" in p["sink"]):
            continue

        sink_pad = p["sink"]["pad"]
        sink_stream = p["sink"]["stream"]

        source_pad = p["source"]["pad"]
        source_stream = p["source"]["stream"]

        routes.append(pykms.SubdevRoute(sink_pad, sink_stream, source_pad, source_stream, True))

    if len(routes) == 0:
        continue

    e.subdev.set_routes(routes)

# setup bus formats
for stream in streams:
    if not "pipe" in stream:
        continue

    for p in stream["pipe"]:
        if ("sink" in p) and ("fmt" in p["sink"]):
            w, h, fmt = p["sink"]["fmt"]
            p["entity"].subdev.set_format(p["sink"]["pad"], w, h, fmt)

        if ("source" in p) and ("fmt" in p["source"]):
            w, h, fmt = p["source"]["fmt"]
            p["entity"].subdev.set_format(p["source"]["pad"], w, h, fmt)

for stream in streams:
    print(stream)

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

for i, stream in enumerate(streams):
    if stream["fmt"] == pykms.PixelFormat.META_16:
        stream["plane_fmt"] = pykms.PixelFormat.YUYV
    else:
        stream["plane_fmt"] = stream["fmt"]

    plane = res.reserve_generic_plane(crtc, stream["plane_fmt"])
    if plane == None:
        plane = res.reserve_generic_plane(crtc, pykms.PixelFormat.RGB565)
        stream["plane_fmt"] = pykms.PixelFormat.RGB565

    assert(plane)

    stream["plane"] = plane
    stream["plane_w"] = stream["w"]
    stream["plane_h"] = stream["h"]

    if stream["embedded"]:
        divs = [16, 8, 4, 2, 1]
        for div in divs:
            w = stream["plane_w"] // div
            if w % 2 == 0:
                break

        h = stream["plane_h"] * div

        stream["plane_w"] = w
        stream["plane_h"] = h

    if i == 0:
        stream["x"] = 0
        stream["y"] = 0
    elif i == 1:
        stream["x"] = mode.hdisplay - stream["plane_w"]
        stream["y"] = 0
    elif i == 2:
        stream["x"] = 0
        stream["y"] = mode.vdisplay - stream["plane_h"]
    elif i == 3:
        stream["x"] = mode.hdisplay - stream["plane_w"]
        stream["y"] = mode.vdisplay - stream["plane_h"]

for stream in streams:
    vd = pykms.VideoDevice(stream["dev"])

    cap = vd.capture_streamer
    cap.set_port(0)
    cap.set_format(stream["fmt"], stream["w"], stream["h"])
    cap.set_queue_size(NUM_BUFS)

    stream["vd"] = vd
    stream["cap"] = cap

for stream in streams:
    # Allocate FBs
    fbs = []
    for i in range(NUM_BUFS):
        fb = pykms.DumbFramebuffer(card, stream["plane_w"], stream["plane_h"], stream["plane_fmt"])
        fbs.append(fb)
    stream["fbs"] = fbs

    # Set fb0 to screen
    fb = stream["fbs"][0]
    plane = stream["plane"]

    plane.set_props({
        "FB_ID": fb.id,
        "CRTC_ID": crtc.id,
        "SRC_W": fb.width << 16,
        "SRC_H": fb.height << 16,
        "CRTC_X": stream["x"],
        "CRTC_Y": stream["y"],
        "CRTC_W": fb.width,
        "CRTC_H": fb.height,
    })

    stream["kms_old_fb"] = None
    stream["kms_fb"] = fb
    stream["kms_fb_queue"] = deque()

    # Queue the rest to the camera
    cap = stream["cap"]
    for i in range(1, NUM_BUFS):
        cap.queue(fbs[i])

for stream in streams:
    stream["num_frames"] = 0
    stream["time"] = time.perf_counter()

    #print("ENABLE STREAM")
    print(f'{stream["dev"]}: stream on')
    stream["cap"].stream_on()
    #time.sleep(0.5)
    #print("ENABLED STREAM")
    #time.sleep(1)

kms_committed = False

def readvid(stream):
    stream["num_frames"] += 1

    if stream["num_frames"] == 100:
        diff = time.perf_counter() - stream["time"]

        print("{}: 100 frames in {:.2f} s, {:.2f} fps".format(stream["dev"], diff, 100 / diff))

        stream["num_frames"] = 0
        stream["time"] = time.perf_counter()

    cap = stream["cap"]
    fb = cap.dequeue()

    stream["kms_fb_queue"].append(fb)

    if len(stream["kms_fb_queue"]) >= NUM_BUFS - 1:
        print("WARNING fb_queue {}".format(len(stream["kms_fb_queue"])))

    #print(f'Buf from {stream["dev"]}: kms_fb_queue {len(stream["kms_fb_queue"])}, commit ongoing {kms_committed}')

    # XXX with a small delay we might get more planes to the commit
    if kms_committed == False:
        handle_pageflip()

def readkey(conn, mask):
    for stream in reversed(streams):
        print(f'{stream["dev"]}: stream off')
        stream["cap"].stream_off()
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

    for stream in streams:
        #print(f'Page flip {stream["dev"]}: kms_fb_queue {len(stream["kms_fb_queue"])}, new_fb {stream["kms_fb"]}, old_fb {stream["kms_old_fb"]}')

        cap = stream["cap"]

        if stream["kms_old_fb"]:
            cap.queue(stream["kms_old_fb"])
            stream["kms_old_fb"] = None

        if len(stream["kms_fb_queue"]) == 0:
            continue

        stream["kms_old_fb"] = stream["kms_fb"]

        fb = stream["kms_fb_queue"].popleft()
        stream["kms_fb"] = fb

        plane = stream["plane"]

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
for stream in streams:
    sel.register(stream["cap"].fd, selectors.EVENT_READ, lambda c,m,d=stream: readvid(d))

while True:
    events = sel.select()
    for key, mask in events:
        callback = key.data
        callback(key.fileobj, mask)
