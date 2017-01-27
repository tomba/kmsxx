#!/usr/bin/python3

import pyudev
import pykms

card = pykms.Card()
res = pykms.ResourceManager(card)
conn = res.reserve_connector("hdmi")

context = pyudev.Context()

dev = pyudev.Devices.from_name(context, 'drm', 'card0')

monitor = pyudev.Monitor.from_netlink(context)
monitor.filter_by('drm')

for device in iter(monitor.poll, None):
	if 'HOTPLUG' in device:
		conn.refresh()
		mode = conn.get_modes()
		print("HPD")
		print(mode)
