#!/usr/bin/python3

import pyudev
import pykms

card = pykms.Card()
conns = card.connectors

context = pyudev.Context()

dev = pyudev.Devices.from_name(context, 'drm', 'card0')

monitor = pyudev.Monitor.from_netlink(context)
monitor.filter_by('drm')

for device in iter(monitor.poll, None):
	if 'HOTPLUG' in device:
		print("HPD")
		for conn in conns:
			conn.refresh()
			modes = conn.get_modes()
			print("  ", conn.fullname, ["{}x{}".format(m.hdisplay, m.vdisplay) for m in modes])
