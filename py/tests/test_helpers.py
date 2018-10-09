#!/usr/bin/python3

from pykms import *
from argparse import *

class ArgumentParserHelper(ArgumentParser):
	def __init__(self, description):
		ArgumentParser.__init__(self, description=description)

	def add_plane_args(self):
		self.add_argument('--plane', '-p', dest='plane', default="",
				  required=False, help='plane number to use')

	def add_conn_args(self):
		self.add_argument('--connector', '-c', dest='connector',
				  default="",
				  required=False, help='connector to output')

	def add_default_args(self):
		self.add_plane_args()
		self.add_conn_args()

class ArgsHelper:
	def __init__(self, args, card):
		self.args = args
		self.card = card
		self.res = ResourceManager(card)
		self.conn = self.res.reserve_connector(self.args.connector)
		self.crtc = self.res.reserve_crtc(self.conn)

	def get_res(self):
		return self.res

	def get_conn(self):
		return self.conn

	def get_crtc(self):
		return self.crtc

	def get_plane(self, format=PixelFormat.Undefined):
		if self.args.plane == "":
			return self.res.reserve_generic_plane(self.crtc,
							      format)
		else:
			return self.card.planes[int(self.args.plane)]
