#!/usr/bin/python3

import pykms
import argparse

md = pykms.MediaDevice("/dev/media0")

skip = ["CAMERARX1", "ds90ub960 4-0036"]

for ent in md.entities:
	if ent.name in skip:
		continue

	pads_with_links = any([p for p in ent.pads if any([l for l in p.links if l.enabled])])
	if not pads_with_links:
		continue

	print(ent.name)

	for pad in ent.pads:
		links = list([l for l in pad.links if l.enabled])
		if len(links) == 0:
			continue

		try:
			fmt = ent.subdev.get_format(pad.index)
			fmt = "[ {} x {} {} ]".format(fmt[0], fmt[1], fmt[2].name)
		except:
			fmt = ""

		print("  Pad{}: {}".format(pad.index, fmt),  end='')

		if len(links) == 1:
			link = links[0]

			remote = link.source if link.sink == ent else link.sink
			remote_pad = link.source_pad if link.sink == ent else link.sink_pad

			print(" {} {}:{}".format("->" if pad.is_source else "<-", remote.name, remote_pad))
		else:
			print()

		if len(links) > 1:
			for link in links:
				remote = link.source if link.sink == ent else link.sink
				remote_pad = link.source_pad if link.sink == ent else link.sink_pad

				print("    {} {}:{}".format("->" if pad.is_source else "<-", remote.name, remote_pad))

	if ent.subdev:
		routes = [r for r in ent.subdev.get_routes() if r.active]
		if len(routes) > 0:
			print("  Routing:")
			for r in routes:
				print("    {}/{} -> {}/{}".format(r.sink_pad, r.sink_stream, r.source_pad, r.source_stream))

	print()

