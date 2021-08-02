#!/usr/bin/python3

import sys
import selectors
import pykms
import argparse
import time
from collections import deque
import math

#CONFIG = "legacy-ov5640"
#CONFIG = "dra7-ov5640"
#CONFIG = "am6-ov5640"
#CONFIG = "j7-ov5640"

#CONFIG = "dra76-ub960-1-cam-meta"
#CONFIG = "dra76-ub960-2-cam"
CONFIG = "dra76-ub960-2-cam-meta"

#CONFIG = "am6-ub960-2-cam"

META_LINES = 1

sensor_1_w = 1280
sensor_1_h = 720

sensor_2_w = 752
sensor_2_h = 480

PIX_BUS_FMT = pykms.BusFormat.UYVY8_2X8
PIX_FMT = pykms.PixelFormat.UYVY

mbus_fmt_pix_1 = (sensor_1_w, sensor_1_h, PIX_BUS_FMT)
mbus_fmt_meta_1 = (sensor_1_w, META_LINES, pykms.BusFormat.METADATA_16)
fmt_pix_1 = (sensor_1_w, sensor_1_h, PIX_FMT)
fmt_meta_1 = (sensor_1_w, META_LINES, pykms.PixelFormat.META_16)

mbus_fmt_pix_2 = (sensor_2_w, sensor_2_h, PIX_BUS_FMT)
mbus_fmt_meta_2 = (sensor_2_w, META_LINES, pykms.BusFormat.METADATA_16)
fmt_pix_2 = (sensor_2_w, sensor_2_h, PIX_FMT)
fmt_meta_2 = (sensor_2_w, META_LINES, pykms.PixelFormat.META_16)

configurations = {}

#
# Non-MC OV5640
#
configurations["legacy-ov5640"] = {
	"devices": [
		{
			"fmt": fmt_pix_1,
			"dev": "/dev/video0",
		},
	],
}

#
# AM6 EVM: OV5640
#
configurations["am6-ov5640"] = {
	"subdevs": [
		{
			"entity": "ov5640 3-003c",
			"pads": [
				{ "pad": (0, 0), "fmt": mbus_fmt_pix_2 },
			],
		},
		{
			"entity": "CAMERARX0",
			"pads": [
				{ "pad": (0, 0), "fmt": mbus_fmt_pix_2 },
				{ "pad": (1, 0), "fmt": mbus_fmt_pix_2 },
			],
		},
	],

	"devices": [
		{
			"entity": "CAL output 0",
			"fmt": fmt_pix_2,
			"dev": "/dev/video0",
		},
	],

	"links": [
		{ "src": ("ov5640 3-003c", 0), "dst": ("CAMERARX0", 0) },
		{ "src": ("CAMERARX0", 1), "dst": ("CAL output 0", 0) },
	],
}

#
# J7 EVM: OV5640
#
configurations["j7-ov5640"] = {
	"subdevs": [
		{
			"entity": "ov5640 9-003c",
			"pads": [
				{ "pad": (0, 0), "fmt": mbus_fmt_pix_1 },
			],
		},
		{
			"entity": "cdns_csi2rx.4504000.csi-bridge",
		},
	],

	"devices": [
		{
			"entity": "j721e-csi2rx",
			"fmt": fmt_pix_1,
			"dev": "/dev/video0",
		},
	],

	"links": [
		{ "src": ("ov5640 9-003c", 0), "dst": ("cdns_csi2rx.4504000.csi-bridge", 0) },
		{ "src": ("cdns_csi2rx.4504000.csi-bridge", 1), "dst": ("j721e-csi2rx", 0) },
	],
}

#
# DRA76 EVM: OV5640
#
configurations["dra7-ov5640"] = {
	"subdevs": [
		{
			"entity": "ov5640 4-003c",
			"pads": [
				{ "pad": (0, 0), "fmt": mbus_fmt_pix_1 },
			],
		},
		{
			"entity": "CAMERARX0",
			"pads": [
				{ "pad": (0, 0), "fmt": mbus_fmt_pix_1 },
				{ "pad": (1, 0), "fmt": mbus_fmt_pix_1 },
			],
		},
	],

	"devices": [
		{
			"entity": "CAL output 0",
			"fmt": fmt_pix_1,
			"dev": "/dev/video0",
		},
	],

	"links": [
		{ "src": ("ov5640 4-003c", 0), "dst": ("CAMERARX0", 0) },
		{ "src": ("CAMERARX0", 1), "dst": ("CAL output 0", 0) },
	],
}

#
# DRA76: UB9060 1 camera, pixel and metadata streams
#
configurations["dra76-ub960-1-cam-meta"] = {
	"subdevs": [
		# cam 1
		{
			"entity": "ov10635 5-0030",
			"pads": [
				{ "pad": 0, "fmt": mbus_fmt_pix_1 },
			],
			"routing": [
				{ "src": (0, 0), "dst": (0, 0), "flags": [ "source" ] },
				{ "src": (0, 1), "dst": (0, 1), "flags": [ "source" ] },
			],
		},
		# Serializer
		{
			"entity": "ds90ub913a 4-0044",
			"routing": [
				{ "src": (0, 0), "dst": (1, 0) },
				{ "src": (0, 1), "dst": (1, 1) },
			],
			"pads": [
				{ "pad": (0, 0), "fmt": mbus_fmt_pix_1 },
				{ "pad": (1, 0), "fmt": mbus_fmt_pix_1 },
				{ "pad": (0, 1), "fmt": mbus_fmt_meta_1 },
				{ "pad": (1, 1), "fmt": mbus_fmt_meta_1 },
			],
		},
		# Deserializer
		{
			"entity": "ds90ub960 4-003d",
			"routing": [
				# cam 1
				{ "src": (0, 0), "dst": (4, 0) },
				{ "src": (0, 1), "dst": (4, 1) },
			],
			"pads": [
				# cam 1
				{ "pad": (0, 0), "fmt": mbus_fmt_pix_1 },
				{ "pad": (0, 1), "fmt": mbus_fmt_meta_1 },
				{ "pad": (4, 0), "fmt": mbus_fmt_pix_1 },
				{ "pad": (4, 1), "fmt": mbus_fmt_meta_1 },
			],
		},
		{
			"entity": "CAMERARX0",
			"routing": [
				# cam 1
				{ "src": (0, 0), "dst": (1, 0) },
				{ "src": (0, 1), "dst": (2, 0) },
			],
			"pads": [
				# cam 1
				{ "pad": (0, 0), "fmt": mbus_fmt_pix_1 },
				{ "pad": (0, 1), "fmt": mbus_fmt_meta_1 },
				{ "pad": (1, 0), "fmt": mbus_fmt_pix_1 },
				{ "pad": (2, 0), "fmt": mbus_fmt_meta_1 },
			],
		},
	],

	"devices": [
		# cam 1
		{
			"entity": "CAL output 0",
			"fmt": fmt_pix_1,
			"embedded": False,
			"dev": "/dev/video0",
		},
		{
			"entity": "CAL output 1",
			"fmt": fmt_meta_1,
			"embedded": True,
			"dev": "/dev/video1",
		},
	],

	"links": [
		{ "src": ("ov10635 5-0030", 0), "dst": ("ds90ub913a 4-0044", 0) },
		{ "src": ("ds90ub913a 4-0044", 1), "dst": ("ds90ub960 4-003d", 0) },
		{ "src": ("ds90ub960 4-003d", 4), "dst": ("CAMERARX0", 0) },
		{ "src": ("CAMERARX0", 1), "dst": ("CAL output 0", 0) },
		{ "src": ("CAMERARX0", 2), "dst": ("CAL output 1", 0) },
	],
}

#
# DRA76: UB9060 2 cameras, only pixel streams
#
configurations["dra76-ub960-2-cam"] = {
	"subdevs": [
		# Camera 1
		{
			"entity": "ov10635 5-0030",
			"pads": [
				{ "pad": 0, "fmt": mbus_fmt_pix_1 },
			],
			"routing": [
				{ "src": (0, 0), "dst": (0, 0), "flags": [ "source" ] },
			],
		},
		# Serializer 1
		{
			"entity": "ds90ub913a 4-0044",
			"routing": [
				{ "src": (0, 0), "dst": (1, 0) },
			],
			"pads": [
				{ "pad": (0, 0), "fmt": mbus_fmt_pix_1 },
				{ "pad": (1, 0), "fmt": mbus_fmt_pix_1 },
			],
		},
		# Camera 2
		{
			"entity": "ov10635 6-0030",
			"pads": [
				{ "pad": 0, "fmt": mbus_fmt_pix_2 },
			],
			"routing": [
				{ "src": (0, 0), "dst": (0, 0), "flags": [ "source" ] },
			],
		},
		# Serializer 2
		{
			"entity": "ds90ub913a 4-0045",
			"routing": [
				{ "src": (0, 0), "dst": (1, 0) },
			],
			"pads": [
				{ "pad": (0, 0), "fmt": mbus_fmt_pix_2 },
				{ "pad": (1, 0), "fmt": mbus_fmt_pix_2 },
			],
		},
		# Deserializer
		{
			"entity": "ds90ub960 4-003d",
			"routing": [
				# Camera 1
				{ "src": (0, 0), "dst": (4, 0) },
				# Camera 2
				{ "src": (1, 0), "dst": (4, 1) },
			],
			"pads": [
				# Camera 1
				{ "pad": (0, 0), "fmt": mbus_fmt_pix_1 },
				{ "pad": (4, 0), "fmt": mbus_fmt_pix_1 },
				# Camera 2
				{ "pad": (1, 0), "fmt": mbus_fmt_pix_2 },
				{ "pad": (4, 1), "fmt": mbus_fmt_pix_2 },
			],
		},
		# CSI-2 RX
		{
			"entity": "CAMERARX0",
			"routing": [
				# cam 1
				{ "src": (0, 0), "dst": (1, 0) },
				# cam 2
				{ "src": (0, 1), "dst": (2, 0) },
			],
			"pads": [
				# cam 1
				{ "pad": (0, 0), "fmt": mbus_fmt_pix_1 },
				{ "pad": (1, 0), "fmt": mbus_fmt_pix_1 },
				# cam 2
				{ "pad": (0, 1), "fmt": mbus_fmt_pix_2 },
				{ "pad": (2, 0), "fmt": mbus_fmt_pix_2 },
			],
		},
	],

	"devices": [
		# cam 1
		{
			"entity": "CAL output 0",
			"fmt": fmt_pix_1,
			"embedded": False,
			"dev": "/dev/video0",
		},
		# cam 2
		{
			"entity": "CAL output 1",
			"fmt": fmt_pix_2,
			"embedded": False,
			"dev": "/dev/video1",
		},
	],

	"links": [
		{ "src": ("ov10635 5-0030", 0), "dst": ("ds90ub913a 4-0044", 0) },
		{ "src": ("ds90ub913a 4-0044", 1), "dst": ("ds90ub960 4-003d", 0) },
		{ "src": ("ov10635 6-0030", 0), "dst": ("ds90ub913a 4-0045", 0) },
		{ "src": ("ds90ub913a 4-0045", 1), "dst": ("ds90ub960 4-003d", 1) },
		{ "src": ("ds90ub960 4-003d", 4), "dst": ("CAMERARX0", 0) },
		{ "src": ("CAMERARX0", 1), "dst": ("CAL output 0", 0) },
		{ "src": ("CAMERARX0", 2), "dst": ("CAL output 1", 0) },
	],
}

#
# DRA76: UB9060 2 cameras, pixel and metadata streams
#
configurations["dra76-ub960-2-cam-meta"] = {
	"subdevs": [
		# Camera 1
		{
			"entity": "ov10635 5-0030",
			"routing": [
				{ "src": (0, 0), "dst": (0, 0), "flags": [ "source" ] },
				{ "src": (0, 1), "dst": (0, 1), "flags": [ "source" ] },
			],
			"pads": [
				{ "pad": (0, 0), "fmt": mbus_fmt_pix_1 },
			],
		},
		# Serializer 1
		{
			"entity": "ds90ub913a 4-0044",
			"routing": [
				{ "src": (0, 0), "dst": (1, 0) },
				{ "src": (0, 1), "dst": (1, 1) },
			],
			"pads": [
				{ "pad": (0, 0), "fmt": mbus_fmt_pix_1 },
				{ "pad": (1, 0), "fmt": mbus_fmt_pix_1 },
				{ "pad": (0, 1), "fmt": mbus_fmt_meta_1 },
				{ "pad": (1, 1), "fmt": mbus_fmt_meta_1 },
			],
		},
		# Camera 2
		{
			"entity": "ov10635 6-0030",
			"routing": [
				{ "src": (0, 0), "dst": (0, 0), "flags": [ "source" ] },
				{ "src": (0, 1), "dst": (0, 1), "flags": [ "source" ] },
			],
			"pads": [
				{ "pad": 0, "fmt": mbus_fmt_pix_2 },
			],
		},
		# Serializer 2
		{
			"entity": "ds90ub913a 4-0045",
			"routing": [
				{ "src": (0, 0), "dst": (1, 0) },
				{ "src": (0, 1), "dst": (1, 1) },
			],
			"pads": [
				{ "pad": (0, 0), "fmt": mbus_fmt_pix_2 },
				{ "pad": (1, 0), "fmt": mbus_fmt_pix_2 },
				{ "pad": (0, 1), "fmt": mbus_fmt_meta_2 },
				{ "pad": (1, 1), "fmt": mbus_fmt_meta_2 },
			],
		},
		# Deserializer
		{
			"entity": "ds90ub960 4-003d",
			"routing": [
				# Camera 1
				{ "src": (0, 0), "dst": (4, 0) },
				{ "src": (0, 1), "dst": (4, 1) },
				# Camera 2
				{ "src": (1, 0), "dst": (4, 2) },
				{ "src": (1, 1), "dst": (4, 3) },
			],
			"pads": [
				# Camera 1
				{ "pad": (0, 0), "fmt": mbus_fmt_pix_1 },
				{ "pad": (0, 1), "fmt": mbus_fmt_meta_1 },
				{ "pad": (4, 0), "fmt": mbus_fmt_pix_1 },
				{ "pad": (4, 1), "fmt": mbus_fmt_meta_1 },
				# Camera 2
				{ "pad": (1, 0), "fmt": mbus_fmt_pix_2 },
				{ "pad": (1, 1), "fmt": mbus_fmt_meta_2 },
				{ "pad": (4, 2), "fmt": mbus_fmt_pix_2 },
				{ "pad": (4, 3), "fmt": mbus_fmt_meta_2 },
			],
		},
		# CSI-2 RX
		{
			"entity": "CAMERARX0",
			"routing": [
				# cam 1
				{ "src": (0, 0), "dst": (1, 0) },
				{ "src": (0, 1), "dst": (2, 0) },
				# cam 2
				{ "src": (0, 2), "dst": (3, 0) },
				{ "src": (0, 3), "dst": (4, 0) },
			],
			"pads": [
				# cam 1
				{ "pad": (0, 0), "fmt": mbus_fmt_pix_1 },
				{ "pad": (0, 1), "fmt": mbus_fmt_meta_1 },
				{ "pad": (1, 0), "fmt": mbus_fmt_pix_1 },
				{ "pad": (2, 0), "fmt": mbus_fmt_meta_1 },
				# cam 2
				{ "pad": (0, 2), "fmt": mbus_fmt_pix_2 },
				{ "pad": (0, 3), "fmt": mbus_fmt_meta_2 },
				{ "pad": (3, 0), "fmt": mbus_fmt_pix_2 },
				{ "pad": (4, 0), "fmt": mbus_fmt_meta_2 },
			],
		},
	],

	"devices": [
		# cam 1
		{
			"entity": "CAL output 0",
			"fmt": fmt_pix_1,
			"embedded": False,
			"dev": "/dev/video0",
		},
		{
			"entity": "CAL output 1",
			"fmt": fmt_meta_1,
			"embedded": True,
			"dev": "/dev/video1",
		},
		# cam 2
		{
			"entity": "CAL output 2",
			"fmt": fmt_pix_2,
			"embedded": False,
			"dev": "/dev/video2",
		},
		{
			"entity": "CAL output 3",
			"fmt": fmt_meta_2,
			"embedded": True,
			"dev": "/dev/video3",
		},
	],

	"links": [
		{ "src": ("ov10635 5-0030", 0), "dst": ("ds90ub913a 4-0044", 0) },
		{ "src": ("ds90ub913a 4-0044", 1), "dst": ("ds90ub960 4-003d", 0) },
		{ "src": ("ov10635 6-0030", 0), "dst": ("ds90ub913a 4-0045", 0) },
		{ "src": ("ds90ub913a 4-0045", 1), "dst": ("ds90ub960 4-003d", 1) },
		{ "src": ("ds90ub960 4-003d", 4), "dst": ("CAMERARX0", 0) },
		{ "src": ("CAMERARX0", 1), "dst": ("CAL output 0", 0) },
		{ "src": ("CAMERARX0", 2), "dst": ("CAL output 1", 0) },
		{ "src": ("CAMERARX0", 3), "dst": ("CAL output 2", 0) },
		{ "src": ("CAMERARX0", 4), "dst": ("CAL output 3", 0) },
	],
}

#
# AM6: UB9060 2 cameras, only pixel streams
#
configurations["am6-ub960-2-cam"] = {
	"subdevs": [
		# cam 1
		{
			"entity": "ov10635 6-0030",
			"pads": [
				{ "pad": 0, "fmt": mbus_fmt_pix_1 },
			],
		},
		{
			"entity": "ds90ub913a 3-0044",
		},
		# cam 2
		{
			"entity": "ov10635 7-0030",
			"pads": [
				{ "pad": 0, "fmt": mbus_fmt_pix_2 },
			],
		},
		{
			"entity": "ds90ub913a 3-0045",
		},
		# deser
		{
			"entity": "ds90ub960 3-003d",
			"routing": [
				# cam 1
				{ "src": (0, 0), "dst": (4, 0) },
				# cam 2
				{ "src": (1, 0), "dst": (4, 1) },
			],
			"pads": [
				# cam 1
				{ "pad": (0, 0), "fmt": mbus_fmt_pix_1 },
				{ "pad": (4, 0), "fmt": mbus_fmt_pix_1 },
				# cam 2
				{ "pad": (1, 0), "fmt": mbus_fmt_pix_2 },
				{ "pad": (4, 1), "fmt": mbus_fmt_pix_2 },
			],
		},
		{
			"entity": "CAMERARX0",
			"routing": [
				# cam 1
				{ "src": (0, 0), "dst": (1, 0) },
				{ "src": (0, 1), "dst": (2, 0) },
				# cam 2
				{ "src": (0, 2), "dst": (3, 0) },
				{ "src": (0, 3), "dst": (4, 0) },
			],
			"pads": [
				# cam 1
				{ "pad": (0, 0), "fmt": mbus_fmt_pix_1 },
				{ "pad": (0, 1), "fmt": mbus_fmt_meta_1 },
				{ "pad": (1, 0), "fmt": mbus_fmt_pix_1 },
				{ "pad": (2, 0), "fmt": mbus_fmt_meta_1 },
				# cam 2
				{ "pad": (0, 2), "fmt": mbus_fmt_pix_2 },
				{ "pad": (0, 3), "fmt": mbus_fmt_meta_2 },
				{ "pad": (3, 0), "fmt": mbus_fmt_pix_2 },
				{ "pad": (4, 0), "fmt": mbus_fmt_meta_2 },
			],
		},
	],

	"devices": [
		{
			"entity": "CAL output 0",
			"fmt": fmt_pix_1,
			"embedded": False,
			"dev": "/dev/video0",
		},
		{
			"entity": "CAL output 1",
			"fmt": fmt_pix_2,
			"embedded": False,
			"dev": "/dev/video1",
		},
	],

	"links": [
		{ "src": ("ov10635 6-0030", 0), "dst": ("ds90ub913a 3-0044", 0) },
		{ "src": ("ds90ub913a 3-0044", 1), "dst": ("ds90ub960 3-003d", 0) },
		{ "src": ("ov10635 7-0030", 0), "dst": ("ds90ub913a 3-0045", 0) },
		{ "src": ("ds90ub913a 3-0045", 1), "dst": ("ds90ub960 3-003d", 1) },
		{ "src": ("ds90ub960 3-003d", 4), "dst": ("CAMERARX0", 0) },
		{ "src": ("CAMERARX0", 1), "dst": ("CAL output 0", 0) },
		{ "src": ("CAMERARX0", 2), "dst": ("CAL output 1", 0) },
	],
}


# Disable all possible links
def disable_all_links(md):
	for ent in md.entities:
		for p in range(ent.num_pads):
			for l in [l for l in ent.get_links(p) if not l.immutable]:
				#print((l.sink, l.sink_pad), (l.source, l.source_pad))
				l.enabled = False
				ent.setup_link(l)

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

print("Configure media entities")

md = pykms.MediaDevice("/dev/media0")

disable_all_links(md)

config = configurations[CONFIG]

# Setup links
for l in config.get("links", []):
	source_ent, source_pad = l["src"]
	sink_ent, sink_pad = l["dst"]

	source_ent = md.find_entity(source_ent)
	sink_ent = md.find_entity(sink_ent)

	link((source_ent, source_pad), (sink_ent, sink_pad))

# Configure entities
for e in config.get("subdevs", []):
	ent = md.find_entity(e["entity"])
	subdev = ent.subdev

	# Configure routes
	if "routing" in e:
		routes = []
		for r in e["routing"]:
			sink_pad, sink_stream = r["src"]
			source_pad, source_stream = r["dst"]

			is_source_route = "source" in r.get("flags", [])

			routes.append(pykms.SubdevRoute(sink_pad, sink_stream, source_pad, source_stream,
			                                is_source_route))

		if len(routes) > 0:
			subdev.set_routes(routes)

	# Configure streams
	if "pads" in e:
		for p in e["pads"]:
			if type(p["pad"]) == tuple:
				pad, stream = p["pad"]
			else:
				pad = p["pad"]
				stream = 0
			w, h, fmt = p["fmt"]
			subdev.set_format(pad, stream, w, h, fmt)

card = pykms.Card()
res = pykms.ResourceManager(card)
conn = res.reserve_connector()
crtc = res.reserve_crtc(conn)

card.disable_planes()

mode = conn.get_default_mode()
modeb = mode.to_blob(card)

req = pykms.AtomicReq(card)
req.add(conn, "CRTC_ID", crtc.id)
req.add(crtc, {"ACTIVE": 1,
		"MODE_ID": modeb.id})
req.commit_sync(allow_modeset = True)

NUM_BUFS = 5

streams = config["devices"]

for i, stream in enumerate(streams):
	stream["w"] = stream["fmt"][0]
	stream["h"] = stream["fmt"][1]
	stream["fourcc"] = stream["fmt"][2]

	if stream["fourcc"] == pykms.PixelFormat.META_16:
		stream["plane_fourcc"] = pykms.PixelFormat.RGB565
	else:
		stream["plane_fourcc"] = stream["fourcc"]

	plane = res.reserve_generic_plane(crtc, stream["plane_fourcc"])
	assert(plane)

	stream["plane"] = plane
	stream["plane_w"] = stream["w"]
	stream["plane_h"] = stream["h"]

	if "embedded" in stream and stream["embedded"]:
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
	cap.set_format(stream["fourcc"], stream["w"], stream["h"])
	cap.set_queue_size(NUM_BUFS)

	stream["vd"] = vd
	stream["cap"] = cap

for stream in streams:
	# Allocate FBs
	fbs = []
	for i in range(NUM_BUFS):
		fb = pykms.DumbFramebuffer(card, stream["plane_w"], stream["plane_h"], stream["plane_fourcc"])
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
