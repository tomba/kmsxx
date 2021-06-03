#!/usr/bin/python3

import pykms

md = pykms.MediaDevice("/dev/media0")

entity = md.find_entity("CAMERARX0")
sd = entity.subdev

def print_sd(sd, type):
	routes = [r for r in sd.get_routes(type) if r.active]
	streams = [r.source_stream for r in routes] + [r.sink_stream for r in routes]

	for r in routes:
		fmt = sd.get_format(r.sink_pad, r.sink_stream, type)
		sink_fmt = "{} x {} {}".format(fmt[0], fmt[1], fmt[2].name)

		fmt = sd.get_format(r.source_pad, r.source_stream, type)
		source_fmt = "{} x {} {}".format(fmt[0], fmt[1], fmt[2].name)

		print("{}/{} [{}] -> {}/{} [{}]".format(r.sink_pad, r.sink_stream, sink_fmt,
		                                        r.source_pad, r.source_stream, source_fmt))

	print()

def print_configs(sd):
	print("ACTIVE")
	print_sd(sd, pykms.ConfigurationType.Active)

	print("TRY")
	print_sd(sd, pykms.ConfigurationType.Try)

print_configs(sd)

routes = [
	pykms.SubdevRoute(0, 2, 1, 3, True),
	pykms.SubdevRoute(0, 5, 2, 4, True),
]

sd.set_routes(routes, pykms.ConfigurationType.Try)

sd.set_format(0, 5, 1280, 480, pykms.BusFormat.YUYV8_2X8, pykms.ConfigurationType.Try)

print_configs(sd)
