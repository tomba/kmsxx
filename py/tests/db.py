#!/usr/bin/python3

import sys
import pykms
import selectors

bar_width = 20
bar_speed = 8

class FlipHandler():
    def __init__(self):
        super().__init__()
        self.bar_xpos = 0
        self.front_buf = 0
        self.fb1 = pykms.DumbFramebuffer(card, mode.hdisplay, mode.vdisplay, "XR24");
        self.fb2 = pykms.DumbFramebuffer(card, mode.hdisplay, mode.vdisplay, "XR24");
        self.flips = 0
        self.frames = 0
        self.time = 0

    def handle_page_flip(self, frame, time):
        self.flips += 1
        if self.time == 0:
            self.frames = frame
            self.time = time

        time_delta = time - self.time
        if time_delta >= 5:
            frame_delta = frame - self.frames
            print("Frame rate: %f (%u/%u frames in %f s)" %
                  (frame_delta / time_delta, self.flips, frame_delta, time_delta))

            self.flips = 0
            self.frames = frame
            self.time = time

        if self.front_buf == 0:
            fb = self.fb2
        else:
            fb = self.fb1

        self.front_buf = self.front_buf ^ 1

        current_xpos = self.bar_xpos;
        old_xpos = (current_xpos + (fb.width - bar_width - bar_speed)) % (fb.width - bar_width);
        new_xpos = (current_xpos + bar_speed) % (fb.width - bar_width);

        self.bar_xpos = new_xpos

        pykms.draw_color_bar(fb, old_xpos, new_xpos, bar_width)

        if card.has_atomic:
            ctx = pykms.AtomicReq(card)
            ctx.add(crtc.primary_plane, "FB_ID", fb.id)
            ctx.commit()
        else:
            crtc.page_flip(fb)

if len(sys.argv) > 1:
    conn_name = sys.argv[1]
else:
    conn_name = ''

card = pykms.Card()
res = pykms.ResourceManager(card)
conn = res.reserve_connector(conn_name)
crtc = res.reserve_crtc(conn)
mode = conn.get_default_mode()

fliphandler = FlipHandler()

crtc.set_mode(conn, fliphandler.fb1, mode)

fliphandler.handle_page_flip(0, 0)

def readdrm(fileobj, mask):
    #print("EVENT");
    for ev in card.read_events():
        if ev.type == pykms.DrmEventType.FLIP_COMPLETE:
            fliphandler.handle_page_flip(ev.seq, ev.time)


def readkey(fileobj, mask):
    #print("KEY EVENT");
    sys.stdin.readline()
    exit(0)

sel = selectors.DefaultSelector()
sel.register(card.fd, selectors.EVENT_READ, readdrm)
sel.register(sys.stdin, selectors.EVENT_READ, readkey)

while True:
    events = sel.select()
    for key, mask in events:
        callback = key.data
        callback(key.fileobj, mask)
