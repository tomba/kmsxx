#!/usr/bin/python3

import pykms

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

