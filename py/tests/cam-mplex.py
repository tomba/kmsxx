#!/usr/bin/python3

import sys
import selectors
import argparse
import time
from collections import deque
import importlib
import pykms
import pyv4l2 as v4l2
import mmap
import pprint

parser = argparse.ArgumentParser()
parser.add_argument("-c", "--config", action="store_true", default=False, help="configure only")
parser.add_argument("-s", "--save", action="store_true", default=False, help="save frames to files")
parser.add_argument("-d", "--display", action="store_true", default=False, help="show frames on screen")
parser.add_argument("-t", "--type", type=str, default="drm", help="buffer type (drm/v4l2)")
parser.add_argument("-p", "--print", action="store_true", default=False, help="print config dict")
args = parser.parse_args()

if not args.type in ["drm", "v4l2"]:
	print("Bad buffer type", args.type)
	exit(-1)

configurations = importlib.import_module("cam-mplex-configs").configurations

def merge_configs(*configs):
	d = { "subdevs": [], "devices": [], "links": [] }

	for config in configs:
		# links can be appended directly
		# xxx there may be (harmless) duplicates
		d["links"] += configurations[config]["links"]

		# devices can be appended directly
		d["devices"] += configurations[config]["devices"]

		# subdevs need to be merged based on entity
		for subdev in configurations[config]["subdevs"]:
			ent = subdev["entity"]

			dst = next((s for s in d["subdevs"] if s["entity"] == ent), None)
			if dst:
				dst["pads"] += subdev["pads"]
				dst["routing"] += subdev["routing"]
			else:
				d["subdevs"].append(subdev)

	return d

config = merge_configs("dra76-ub960-ov10635.0", "dra76-ub960-ov10635.1")

#config = configurations["dra76-ub960-ov10635.0"]

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
	src_ent = source[0]
	sink_ent = sink[0]

	links = src_ent.get_links(source[1])

	link = None

	for l in links:
		if l.sink == sink_ent and l.sink_pad == sink[1]:
			link = l
			break

	if link == None:
		raise Exception("Failed to find link between", source, sink)

	if link.enabled:
		return

	#print("CONF")

	assert(not link.immutable)

	link.enabled = True
	src_ent.setup_link(link)

print("Configure media entities")

md = v4l2.MediaDevice("/dev/media0")

disable_all_links(md)

# Setup links
for l in config.get("links", []):
	source_ent, source_pad = l["src"]
	sink_ent, sink_pad = l["dst"]

	try:
		source_ent = md.find_entity(source_ent)
		sink_ent = md.find_entity(sink_ent)

		link((source_ent, source_pad), (sink_ent, sink_pad))
	except Exception as e:
		print("Failed to link {} -> {}".format((source_ent, source_pad), (sink_ent, sink_pad)))
		raise e

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

			routes.append(v4l2.SubdevRoute(sink_pad, sink_stream, source_pad, source_stream,
			                                is_source_route))

		if len(routes) > 0:
			try:
				subdev.set_routes(routes)
			except Exception as e:
				print("Failed to set routes for {}".format(ent))
				raise e

	# Configure streams
	if "pads" in e:
		for p in e["pads"]:
			if type(p["pad"]) == tuple:
				pad, stream = p["pad"]
			else:
				pad = p["pad"]
				stream = 0
			w, h, fmt = p["fmt"]
			try:
				subdev.set_format(pad, stream, w, h, fmt)
			except Exception as e:
				print("Failed to set format for {}".format(ent))
				raise e

card = None

if args.type == "drm":
	card = pykms.Card()

if args.display:
	if card == None:
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

num_planes = sum(1 for stream in streams if args.display and stream.get("display", True))

display_idx = 0

for i, stream in enumerate(streams):
	stream["display"] = args.display and stream.get("display", True)

	stream["id"] = i

	stream["w"] = stream["fmt"][0]
	stream["h"] = stream["fmt"][1]
	stream["fourcc"] = stream["fmt"][2]

	stream["kms-buf-w"] = stream["w"]
	stream["kms-buf-h"] = stream["h"]

	if stream.get("dra-plane-hack", False):
		# Hack to reserve the unscaleable GFX plane
		res.reserve_generic_plane(crtc, pykms.PixelFormat.RGB565)

	if not "kms-fourcc" in stream:
		if stream["fourcc"] == v4l2.PixelFormat.META_16:
			stream["kms-fourcc"] = pykms.PixelFormat.RGB565
		else:
			stream["kms-fourcc"] = pykms.PixelFormat(stream["fourcc"])

	if args.type == "drm" and "embedded" in stream and stream["embedded"]:
		divs = [16, 8, 4, 2, 1]
		for div in divs:
			w = stream["kms-buf-w"] // div
			if w % 2 == 0:
				break

		h = stream["kms-buf-h"] * div

		stream["kms-buf-w"] = w
		stream["kms-buf-h"] = h

	if stream["display"]:
		max_w = mode.hdisplay // (1 if num_planes == 1 else 2)
		max_h = mode.vdisplay // (1 if num_planes <= 2 else 2)

		stream["kms-src-w"] = min(stream["kms-buf-w"], max_w)
		stream["kms-src-h"] = min(stream["kms-buf-h"], max_h)
		stream["kms-src-x"] = (stream["kms-buf-w"] - stream["kms-src-w"]) // 2
		stream["kms-src-y"] = (stream["kms-buf-h"] - stream["kms-src-h"]) // 2

		stream["kms-dst-w"]  =stream["kms-src-w"]
		stream["kms-dst-h"] = stream["kms-src-h"]

		if display_idx % 2 == 0:
			stream["kms-dst-x"] = 0
		else:
			stream["kms-dst-x"] = mode.hdisplay - stream["kms-dst-w"]

		if display_idx // 2 == 0:
			stream["kms-dst-y"] = 0
		else:
			stream["kms-dst-y"] = mode.vdisplay - stream["kms-dst-h"]

		display_idx += 1

		plane = res.reserve_generic_plane(crtc, stream["kms-fourcc"])
		assert(plane)
		stream["plane"] = plane

if args.print:
	for stream in streams:
		pprint.pprint(stream)

for stream in streams:
	vd = v4l2.VideoDevice(stream["dev"])

	cap = vd.capture_streamer
	cap.set_port(0)
	cap.set_format(stream["fourcc"], stream["w"], stream["h"])

	stream["vd"] = vd
	stream["cap"] = cap

if args.config:
	exit(0)

for stream in streams:
	cap = stream["cap"]

	mem_type = v4l2.VideoMemoryType.DMABUF if args.type == "drm" else v4l2.VideoMemoryType.MMAP
	cap.set_queue_size(NUM_BUFS, mem_type)

	if args.type == "drm":
		# Allocate FBs
		fbs = []
		for i in range(NUM_BUFS):
			fb = pykms.DumbFramebuffer(card, stream["kms-buf-w"], stream["kms-buf-h"], stream["kms-fourcc"])
			fbs.append(fb)
		stream["fbs"] = fbs

	if stream["display"]:
		assert(args.type == "drm")

		# Set fb0 to screen
		fb = stream["fbs"][0]
		plane = stream["plane"]

		plane.set_props({
			"FB_ID": fb.id,
			"CRTC_ID": crtc.id,
			"SRC_X": stream["kms-src-x"] << 16,
			"SRC_Y": stream["kms-src-y"] << 16,
			"SRC_W": stream["kms-src-w"] << 16,
			"SRC_H": stream["kms-src-h"] << 16,
			"CRTC_X": stream["kms-dst-x"],
			"CRTC_Y": stream["kms-dst-y"],
			"CRTC_W": stream["kms-dst-w"],
			"CRTC_H": stream["kms-dst-h"],
		})

		stream["kms_old_fb"] = None
		stream["kms_fb"] = fb
		stream["kms_fb_queue"] = deque()

	first_buf = 1 if stream["display"] else 0

	# Queue the rest to the camera
	for i in range(first_buf, NUM_BUFS):
		if args.type == "drm":
			vbuf = v4l2.create_dmabuffer(fbs[i].fd(0))
		else:
			vbuf = v4l2.create_mmapbuffer()
		cap.queue(vbuf)

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
	vbuf = cap.dequeue()

	if args.type == "drm":
		fb = next((fb for fb in stream["fbs"] if fb.fd(0) == vbuf.fd), None)
		assert(fb != None)

	if args.save:
		filename = "frame-{}-{}.data".format(stream["id"], stream["num_frames"])
		print("save to " + filename)

		if args.type == "drm":
			with mmap.mmap(fb.fd(0), fb.size(0), mmap.MAP_SHARED, mmap.PROT_READ) as b:
				with open(filename, "wb") as f:
					f.write(b)
		else:
			with mmap.mmap(cap.fd, vbuf.length, mmap.MAP_SHARED, mmap.PROT_READ,
			               offset=vbuf.offset) as b:
				with open(filename, "wb") as f:
					f.write(b)

	if stream["display"]:
		stream["kms_fb_queue"].append(fb)

		if len(stream["kms_fb_queue"]) >= NUM_BUFS - 1:
			print("WARNING fb_queue {}".format(len(stream["kms_fb_queue"])))

		#print(f'Buf from {stream["dev"]}: kms_fb_queue {len(stream["kms_fb_queue"])}, commit ongoing {kms_committed}')

		# XXX with a small delay we might get more planes to the commit
		if kms_committed == False:
			handle_pageflip()
	else:
		cap.queue(vbuf)


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
		if not stream["display"]:
			continue

		#print(f'Page flip {stream["dev"]}: kms_fb_queue {len(stream["kms_fb_queue"])}, new_fb {stream["kms_fb"]}, old_fb {stream["kms_old_fb"]}')

		cap = stream["cap"]

		if stream["kms_old_fb"]:
			assert(args.type == "drm")

			fb = stream["kms_old_fb"]
			vbuf = v4l2.create_dmabuffer(fb.fd(0))
			cap.queue(vbuf)
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
if args.display:
	sel.register(card.fd, selectors.EVENT_READ, readdrm)
for stream in streams:
	sel.register(stream["cap"].fd, selectors.EVENT_READ, lambda c,m,d=stream: readvid(d))

while True:
	events = sel.select()
	for key, mask in events:
		callback = key.data
		callback(key.fileobj, mask)
