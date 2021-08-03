#!/usr/bin/python3

import pyv4l2 as v4l2
import argparse

md = v4l2.MediaDevice("/dev/media0")

entities = list(md.entities)

# start with source-only entities (sensors)
print_queue = sorted([ent for ent in entities if all(p.is_source for p in ent.pads)], key=lambda e: e.name)

flatten = lambda t: [item for sublist in t for item in sublist]

printed = set()

while len(print_queue) > 0:
	ent = print_queue.pop(0)

	if ent in printed:
		continue

	printed.add(ent)

	links = flatten([ p.links for p in ent.pads if p.is_source ])
	print_queue += [ l.sink for l in links ]

	pads_with_links = any([p for p in ent.pads if any([l for l in p.links if l.enabled])])
	if not pads_with_links:
		continue

	if ent.subdev:
		routes = [r for r in ent.subdev.get_routes() if r.active]
	else:
		routes = []

	print(ent.name)

	for pad in ent.pads:
		links = list([l for l in pad.links if l.enabled])
		if len(links) == 0:
			continue

		print("  Pad{}: ".format(pad.index),  end='')

		assert(len(links) <= 1)

		if len(links) == 1:
			link = links[0]

			remote = link.source if link.sink == ent else link.sink
			remote_pad = link.source_pad if link.sink == ent else link.sink_pad

			print(" {} {}:{}".format("->" if pad.is_source else "<-", remote.name, remote_pad))
		else:
			print()

		streams = set([r.source_stream for r in routes if r.source_pad == pad.index] + [r.sink_stream for r in routes if r.sink_pad == pad.index])

		if len(streams) == 0:
			streams = [ 0 ]

		streams = sorted(streams)

		for s in streams:
			if ent.subdev:
				try:
					fmt = ent.subdev.get_format(pad.index, s)
				except:
					fmt = None
			else:
				try:
					fmt = ent.dev.capture_streamer.get_format()
				except:
					fmt = None

			if fmt:
				fmt = "    Stream{} {} x {} {}".format(s, fmt[0], fmt[1], fmt[2].name)
			else:
				fmt = "    Stream{} <unknown>".format(s)

			print(fmt)

	if ent.subdev:
		routes = [r for r in ent.subdev.get_routes() if r.active]
		if len(routes) > 0:
			print("  Routing:")
			for r in routes:
				print("    {}/{} -> {}/{}".format(r.sink_pad, r.sink_stream, r.source_pad, r.source_stream))

	print()

