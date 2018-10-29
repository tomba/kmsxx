#!/usr/bin/python3

import sys
import selectors
import pykms
import argparse
import time

iw = 640
ih = 480
ifmt = pykms.PixelFormat.XRGB8888

ow = 640
oh = 480
ofmt = pykms.PixelFormat.XRGB8888

card = pykms.Card()
res = pykms.ResourceManager(card)
conn = res.reserve_connector()
crtc = res.reserve_crtc(conn)
plane1 = res.reserve_overlay_plane(crtc, ifmt)
plane2 = res.reserve_overlay_plane(crtc, ofmt)

print("{}, {}".format(plane1.id, plane2.id))

mode = conn.get_default_mode()
modeb = mode.to_blob(card)

card.disable_planes()

req = pykms.AtomicReq(card)
req.add(conn, "CRTC_ID", crtc.id)
req.add(crtc, {"ACTIVE": 1,
		"MODE_ID": modeb.id})
req.commit_sync(allow_modeset = True)

NUM_BUFS = 4

src_fbs = []
dst_fbs = []

for i in range(NUM_BUFS):
	fb = pykms.DumbFramebuffer(card, iw, ih, ifmt)
	pykms.draw_test_pattern(fb);
	pykms.draw_text(fb, iw // 2, 2, str(i), pykms.white);
	src_fbs.append(fb)

	fb = pykms.DumbFramebuffer(card, ow, oh, ofmt)
	dst_fbs.append(fb)

# put the planes on the screen, so that WB doesn't take them
req = pykms.AtomicReq(card)
req.add_plane(plane1, src_fbs[0], crtc, dst=(0, 0, 400, 480))
req.add_plane(plane2, dst_fbs[1], crtc, dst=(400, 0, 400, 480))
r = req.commit_sync(allow_modeset = True)
assert r == 0

vid = pykms.VideoDevice("/dev/video10")

src_streamer = vid.output_streamer
dst_streamer = vid.capture_streamer

src_streamer.set_format(ifmt, iw, ih)
(left, top, width, height) = src_streamer.get_selection()
print("get: crop -> {},{}-{}x{}".format(left, top, width, height))
(left, top, width, height) = src_streamer.set_selection(160, 0, 320, 240)
print("set: crop -> {},{}-{}x{}".format(left, top, width, height))

dst_streamer.set_format(ofmt, ow, oh)

src_streamer.set_queue_size(NUM_BUFS)
dst_streamer.set_queue_size(NUM_BUFS)

for fb in src_fbs:
	src_streamer.queue(fb)

for fb in dst_fbs:
	dst_streamer.queue(fb)

input("press enter\n")

src_streamer.stream_on()
dst_streamer.stream_on()

loop_count = 0

def readvid(conn, mask):
	global loop_count
	print("VID EVENT");

	ifb = src_streamer.dequeue()
	ofb = dst_streamer.dequeue()

	req = pykms.AtomicReq(card)
	req.add_plane(plane1, ifb, crtc, dst=(0, 0, 400, 480))
	req.add_plane(plane2, ofb, crtc, dst=(400, 0, 400, 480))
	req.commit_sync(allow_modeset = True)
	time.sleep(1)
	loop_count += 1
	if loop_count >= 10:
		exit(0)
	print("loop #", loop_count) 
	src_streamer.queue(ifb)
	dst_streamer.queue(ofb)
	

def readkey(conn, mask):
	print("KEY EVENT");
	sys.stdin.readline()
	exit(0)

sel = selectors.PollSelector()
sel.register(vid.fd, selectors.EVENT_READ, readvid)
sel.register(sys.stdin, selectors.EVENT_READ, readkey)

while True:
	events = sel.select()
	for key, mask in events:
		callback = key.data
		callback(key.fileobj, mask)

print("done");
exit(0)
