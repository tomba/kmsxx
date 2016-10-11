#!/usr/bin/python3

import sys
import re

# Convert DMT pdf to txt:
# pdftotext -layout -f 18 -l 105  DMTr1\ v13.pdf DMT.txt

# Path to the text file
filepath = sys.argv[1]

m = {}
line = ""

def parsei(key, regex, base=10):
	global m
	global line

	match = re.search(regex, line)
	if match != None:
		m[key] = int(match.group(1), base)

def parsef(key, regex, base=10):
	global m
	global line

	match = re.search(regex, line)
	if match != None:
		m[key] = float(match.group(1))

for line in open(filepath, 'r'):
	# each page starts with this
	if "VESA MONITOR TIMING STANDARD" in line:
		m = { }

	# each page ends with this
	if "VESA Display Monitor Timing Standard" in line:
		print("// {:#x} - {}".format(m["dmt_id"], m["name"]))

		flags = []
		if m["ilace"]:
			flags += [ "DRM_MODE_FLAG_INTERLACE" ]

		if m["hsp"]:
			flags += [ "DRM_MODE_FLAG_PHSYNC" ]
		else:
			flags += [ "DRM_MODE_FLAG_NHSYNC" ]

		if m["vsp"]:
			flags += [ "DRM_MODE_FLAG_PVSYNC" ]
		else:
			flags += [ "DRM_MODE_FLAG_NVSYNC" ]

		print("DRM_MODE(\"{}\", {}, {}, {}, {}, {}, {}, {}, {}, {}, {}),".format(
			m["name"],
			int(m["pclk"] * 1000),
			m["hact"], m["hfp"], m["hsw"], m["hbp"],
			m["vact"], m["vfp"], m["vsw"], m["vbp"],
			" | ".join(flags)
			))

	match = re.search("Timing Name\s+=\s+([^;]+)", line)
	if match != None:
		m["name"] = str.strip(match.group(1))

	parsei("dmt_id", "EDID ID:\s+DMT ID: ([0-9A-Fa-f]+)h", 16)
	parsef("pclk", "Pixel Clock\s+=\s+(\d+\.\d+)")

	parsei("hact", "Hor Pixels\s+=\s+(\d+)")
	parsei("hfp", "H Front Porch.*\s(\d+) Pixels")
	parsei("hsw", "Hor Sync Time.*\s(\d+) Pixels")
	parsei("hbp", "H Back Porch.*\s(\d+) Pixels")

	parsei("vact", "Ver Pixels\s+=\s+(\d+)")
	parsei("vfp", "V Front Porch.*\s(\d+)\s+lines")
	parsei("vsw", "Ver Sync Time.*\s(\d+)\s+lines")
	parsei("vbp", "V Back Porch.*\s(\d+)\s+lines")

	match = re.search("Scan Type\s+=\s+(\w+);", line)
	if match != None:
		if match.group(1) == "NONINTERLACED":
			m["ilace"] = False
		elif match.group(1) == "INTERLACED":
			m["ilace"] = True
		else:
			print("Bad scan type")
			exit(-1)

	match = re.search("Hor Sync Polarity\s+=\s+(\w+)", line)
	if match != None:
		if match.group(1) == "POSITIVE":
			m["hsp"] = True
		elif match.group(1) == "NEGATIVE":
			m["hsp"] = False
		else:
			print("Bad hsync polarity")
			exit(-1)

	match = re.search("Ver Sync Polarity\s+=\s+(\w+)", line)
	if match != None:
		if match.group(1) == "POSITIVE":
			m["vsp"] = True
		elif match.group(1) == "NEGATIVE":
			m["vsp"] = False
		else:
			print("Bad vsync polarity")
			exit(-1)
