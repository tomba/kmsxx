#!/usr/bin/python3

import pykms
import random
import time
import sys
import select
import argparse
import selectors

black = pykms.RGB(0, 0, 0)

parser = argparse.ArgumentParser(formatter_class=argparse.RawTextHelpFormatter)
parser.add_argument('--flipmode', choices=['single', 'separate'], default='single', required=False,
    help="""Page flip method to use:
    single: Page flip on all displays with one request (default)
    separate: Separate page flip on the displays""")

args = parser.parse_args()

card = pykms.Card()

if not card.has_atomic:
    print('Atomic mode settings is not supported :(')
    sys.exit()

if args.flipmode == 'single':
    print('Page flip on all displays with one request')
elif args.flipmode == 'separate':
    print('Page flip on all displays with separate requests')

res = pykms.ResourceManager(card)

conn_list = []
crtc_list = []
mode_list = []
plane_list = []
big_fb_list = []

for conn in card.connectors:
    if conn.connected() == 1:
        conn_list.append(conn)

print('Have {} connected connectors:'.format(len(conn_list)))
for conn in conn_list:
    crtc = res.reserve_crtc(conn)
    crtc_list.append(crtc)

    mode = conn.get_default_mode()
    mode_list.append(mode)

    print(' {}: {} ({}x{})'.format(conn.idx, conn.fullname,
        mode.hdisplay, mode.vdisplay))

fbX = sum(mode.hdisplay for mode in mode_list)
fbY = max(mode.vdisplay for mode in mode_list)

print('FB Resolution: {}x{}\n'.format(fbX, fbY))

# Create the (big)framebuffer(s)
for x in range(2):
    fb_tmp = pykms.DumbFramebuffer(card, fbX, fbY, 'XR24');
    big_fb_list.append(fb_tmp)

fb = big_fb_list[0]
screen_offset = 0

card.disable_planes()
for i in range(0, len(conn_list)):
    conn = conn_list[i]
    crtc = crtc_list[i]
    mode = mode_list[i]

    plane = res.reserve_generic_plane(crtc)
    plane_list.append(plane)

    modeb = mode.to_blob(card)
    req = pykms.AtomicReq(card)
    req.add(conn, 'CRTC_ID', crtc.id)
    req.add(crtc, {'ACTIVE': 1,
                    'MODE_ID': modeb.id})
    req.add(plane, {'FB_ID': fb.id,
                    'CRTC_ID': crtc.id,
                    'SRC_X': screen_offset << 16,
                    'SRC_Y': 0 << 16,
                    'SRC_W': mode.hdisplay << 16,
                    'SRC_H': mode.vdisplay << 16,
                    'CRTC_X': 0,
                    'CRTC_Y': 0,
                    'CRTC_W': mode.hdisplay,
                    'CRTC_H': mode.vdisplay,
                    'zpos': 0})

    req.commit_sync(allow_modeset = True)

    screen_offset += mode.hdisplay

# Double buffering, page flipping
class bigFB_db:
    def __init__(self, fb1, fb2):
        self.speed_y = random.randrange(1, 10, 1)
        self.dir_y = random.randrange(-1, 3, 2)
        self.first_run = True
        self.fbs = [fb1,fb2]
        self.draw_buf = 0
        self.fbX = fb1.width
        self.fbY = fb1.height
        self.pos_y = self.fbY // 2
        self.old_pos_y = -1
        # 5 + 10 + 15 + 10 + 5 = 45
        self.bar_size = 45
        self.flips = 0
        self.frames = 0
        self.time = 0
        self.flip_count = 100

    def new_color(self):
        r = random.randrange(255)
        g = random.randrange(255)
        b = random.randrange(255)
        self.color = pykms.RGB(r, g, b)
        self.color2 = pykms.RGB(r // 2, g // 2, b // 2)
        self.color3 = pykms.RGB(r // 3, g // 3, b // 3)
    def move_stripe(self):
        if self.first_run:
            self.new_color()
            self.first_run = False

        fb = self.fbs[self.draw_buf]

        old_box_y = self.old_pos_y
        self.old_pos_y = self.pos_y
        change_speed = 0

        self.pos_y = int(self.pos_y + (self.dir_y * self.speed_y))

        if self.pos_y < 0:
            self.pos_y = 0
            change_speed = 1
            self.dir_y = 1
        elif self.pos_y > (self.fbY - self.bar_size):
            self.pos_y = self.fbY - self.bar_size
            change_speed = 1
            self.dir_y = -1

        if change_speed == 1:
            self.new_color()
            self.speed_y = random.randrange(1, 10, 1)

        # Erease the old box
        if old_box_y >= 0:
            pykms.draw_rect(fb, 0, old_box_y, self.fbX, self.bar_size, black)

        pos_y = self.pos_y
        pykms.draw_rect(fb, 0, pos_y, self.fbX, 5, self.color3)
        pos_y += 5
        pykms.draw_rect(fb, 0, pos_y, self.fbX, 10, self.color2)
        pos_y += 10
        pykms.draw_rect(fb, 0, pos_y, self.fbX, 15, self.color)
        pos_y += 15
        pykms.draw_rect(fb, 0, pos_y, self.fbX, 10, self.color2)
        pos_y += 10
        pykms.draw_rect(fb, 0, pos_y, self.fbX, 5, self.color3)

    def handle_page_flip_single(self):
        self.draw_buf ^= 1
        self.move_stripe()

        # one atomic request to flip on all displays/crtcs
        fb = self.fbs[self.draw_buf]
        screen_offset = 0

        req = pykms.AtomicReq(card)
        for i in range(0, len(conn_list)):
            crtc = crtc_list[i]
            mode = mode_list[i]

            plane = plane_list[i]

            req.add(plane, {'FB_ID': fb.id,
                            'CRTC_ID': crtc.id,
                            'SRC_X': screen_offset << 16,
                            'SRC_Y': 0 << 16,
                            'SRC_W': mode.hdisplay << 16,
                            'SRC_H': mode.vdisplay << 16,
                            'CRTC_X': 0,
                            'CRTC_Y': 0,
                            'CRTC_W': mode.hdisplay,
                            'CRTC_H': mode.vdisplay,
                            'zpos': 0})

            screen_offset += mode.hdisplay

        req.commit(0)

    def handle_page_flip_separate(self):
        self.draw_buf ^= 1
        self.move_stripe()

        # ask to flip the first screen
        fb = self.fbs[self.draw_buf]
        screen_offset = 0

        # add separate atomic request for each display (crtc)
        for i in range(0, len(conn_list)):
            req = pykms.AtomicReq(card)
            crtc = crtc_list[i]
            mode = mode_list[i]

            plane = plane_list[i]

            req.add(plane, {'FB_ID': fb.id,
                            'CRTC_ID': crtc.id,
                            'SRC_X': screen_offset << 16,
                            'SRC_Y': 0 << 16,
                            'SRC_W': mode.hdisplay << 16,
                            'SRC_H': mode.vdisplay << 16,
                            'CRTC_X': 0,
                            'CRTC_Y': 0,
                            'CRTC_W': mode.hdisplay,
                            'CRTC_H': mode.vdisplay,
                            'zpos': 0})

            screen_offset += mode.hdisplay

            req.commit(0)

    def handle_page_flip_main(self, frame, time):
        self.flip_count += 1

        if self.flip_count < len(conn_list):
            return

        self.flip_count = 0

        # statistics
        self.flips += 1
        if self.time == 0:
            self.frames = frame
            self.time = time

        time_delta = time - self.time
        if time_delta >= 5:
            frame_delta = frame - self.frames
            print('Frame rate: %f (%u/%u frames in %f s)' %
                  (frame_delta / time_delta, self.flips, frame_delta, time_delta))

            self.flips = 0
            self.frames = frame
            self.time = time

        if args.flipmode == 'single':
            self.handle_page_flip_single()
        elif args.flipmode == 'separate':
            self.handle_page_flip_separate()

print('Press ENTER to exit\n')

box_db = bigFB_db(big_fb_list[0], big_fb_list[1])
box_db.handle_page_flip_main(0, 0)

def readdrm(fileobj, mask):
    for ev in card.read_events():
        if ev.type == pykms.DrmEventType.FLIP_COMPLETE:
            box_db.handle_page_flip_main(ev.seq, ev.time)

def readkey(fileobj, mask):
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
