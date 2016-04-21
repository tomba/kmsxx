#!/usr/bin/python3

import sys
import pykms
import selectors

bar_width = 20
bar_speed = 8

class FlipHandler(pykms.PageFlipHandlerBase):
    def __init__(self):
        super().__init__()
        self.bar_xpos = 0
        self.front_buf = 0
        self.fb1 = pykms.DumbFramebuffer(card, mode.hdisplay, mode.vdisplay, "XR24");
        self.fb2 = pykms.DumbFramebuffer(card, mode.hdisplay, mode.vdisplay, "XR24");

    def handle_page_flip(self, frame, time):
        if self.front_buf == 0:
            fb = self.fb2
        else:
            fb = self.fb1

        self.front_buf = self.front_buf ^ 1

        current_xpos = self.bar_xpos;
        old_xpos = (current_xpos + (fb.width() - bar_width - bar_speed)) % (fb.width() - bar_width);
        new_xpos = (current_xpos + bar_speed) % (fb.width() - bar_width);

        self.bar_xpos = new_xpos

        pykms.draw_color_bar(fb, old_xpos, new_xpos, bar_width)

        if card.has_atomic():
            ctx = pykms.AtomicReq(card)
            ctx.add(crtc, "FB_ID", fb.id())
            ctx.commit(self)
        else:
            crtc.page_flip(fb, self)



card = pykms.Card()
conn = card.get_first_connected_connector()
mode = conn.get_default_mode()
crtc = conn.get_current_crtc()

fliphandler = FlipHandler()

crtc.set_mode(conn, fliphandler.fb1, mode)

fliphandler.handle_page_flip(0, 0)

def readdrm(conn, mask):
    #print("EVENT");
    card.call_page_flip_handlers()

def readkey(conn, mask):
    #print("KEY EVENT");
    sys.stdin.readline()
    exit(0)

sel = selectors.DefaultSelector()
sel.register(card.fd(), selectors.EVENT_READ, readdrm)
sel.register(sys.stdin, selectors.EVENT_READ, readkey)

while True:
    events = sel.select()
    for key, mask in events:
        callback = key.data
        callback(key.fileobj, mask)
