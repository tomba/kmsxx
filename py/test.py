#!/usr/bin/python3

import pykms
from helpers import *

card = pykms.Card()

conn = card.get_first_connected_connector()

mode = conn.get_default_mode()

fb = pykms.DumbFramebuffer(card, mode.hdisplay, mode.vdisplay, "XR24");
pykms.draw_test_pattern(fb);

crtc = get_crtc_for_connector(conn)

crtc.set_mode(conn, fb, mode)

input("press enter to exit\n")
