#!/usr/bin/python3

import ctypes
import fcntl
import os
import pykms
import selectors
import sys
import time

bar_width = 20
bar_speed = 8

class Timer(object):
    timers = []

    def __init__(self, timeout, callback, data):
        self.timeout = time.clock_gettime(time.CLOCK_MONOTONIC) + timeout
        self.callback = callback
        self.data = data

        print("adding timer %f" % self.timeout)
        self.timers.append(self)
        self.timers.sort(key=lambda timer: timer.timeout)

    @classmethod
    def fire(_class):
        clk = time.clock_gettime(time.CLOCK_MONOTONIC)
        while len(_class.timers) > 0:
            timer = _class.timers[0]
            if timer.timeout > clk:
                break

            del _class.timers[0]
            print("fireing timer %f" % timer.timeout)
            timer.callback(timer.data)

    @classmethod
    def next_timeout(_class):
        clk = time.clock_gettime(time.CLOCK_MONOTONIC)
        if len(_class.timers) == 0 or _class.timers[0].timeout < clk:
            return None

        return _class.timers[0].timeout - clk


class Timeline(object):

    class sw_sync_create_fence_data(ctypes.Structure):
        _fields_ = [
            ('value', ctypes.c_uint32),
            ('name', ctypes.c_char * 32),
            ('fence', ctypes.c_int32),
        ]

    SW_SYNC_IOC_CREATE_FENCE = (3 << 30) | (ctypes.sizeof(sw_sync_create_fence_data) << 16) | (ord('W') << 8) | (0 << 0)
    SW_SYNC_IOC_INC = (1 << 30) | (ctypes.sizeof(ctypes.c_uint32) << 16) | (ord('W') << 8) | (1 << 0)

    class SWSync(object):
        def __init__(self, fd):
            self.fd = fd
        def __del__(self):
            os.close(self.fd)

    def __init__(self):
        self.value = 0
        try:
            self.fd = os.open('/sys/kernel/debug/sync/sw_sync', 0);
        except:
            raise RuntimeError('Failed to open sw_sync file')

    def close(self):
        os.close(self.fd)

    def create_fence(self, value):
        data = self.sw_sync_create_fence_data(value = value);
        print("ioctl number %u" % self.SW_SYNC_IOC_CREATE_FENCE)
        ret = fcntl.ioctl(self.fd, self.SW_SYNC_IOC_CREATE_FENCE, data);
        if ret < 0:
            raise RuntimeError('Failed to create fence')

        return self.SWSync(data.fence)

    def signal(self, value):
        fcntl.ioctl(self.fd, self.SW_SYNC_IOC_INC, ctypes.c_uint32(value))
        self.value += value


class FlipHandler():
    def __init__(self, crtc, width, height):
        super().__init__()
        self.crtc = crtc
        self.timeline = Timeline()
        self.bar_xpos = 0
        self.front_buf = 0
        self.fb1 = pykms.DumbFramebuffer(crtc.card, width, height, "XR24");
        self.fb2 = pykms.DumbFramebuffer(crtc.card, width, height, "XR24");
        self.flips = 0
        self.flips_last = 0
        self.frame_last = 0
        self.time_last = 0

    def handle_page_flip(self, frame, time):
        if self.time_last == 0:
            self.frame_last = frame
            self.time_last = time

        # Verify that the page flip hasn't completed before the timeline got
        # signaled.
        if self.timeline.value < 2 * self.flips - 1:
            raise RuntimeError('Page flip %u for fence %u complete before timeline (%u)!' %
                               (self.flips, 2 * self.flips - 1, self.timeline.value))

        self.flips += 1

        # Print statistics every 5 seconds.
        time_delta = time - self.time_last
        if time_delta >= 5:
            frame_delta = frame - self.frame_last
            flips_delta = self.flips - self.flips_last
            print("Frame rate: %f (%u/%u frames in %f s)" %
                  (frame_delta / time_delta, flips_delta, frame_delta, time_delta))

            self.frame_last = frame
            self.flips_last = self.flips
            self.time_last = time

        # Draw the color bar on the back buffer.
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

        # Flip the buffers with an in fence located in the future. The atomic
        # commit is asynchronous and returns immediately, but the flip should
        # not complete before the fence gets signaled.
        print("flipping with fence @%u, timeline is @%u" % (2 * self.flips - 1, self.timeline.value))
        fence = self.timeline.create_fence(2 * self.flips - 1)
        req = pykms.AtomicReq(self.crtc.card)
        req.add(self.crtc.primary_plane, { 'FB_ID': fb.id, 'IN_FENCE_FD': fence.fd })
        req.commit()
        del fence

        # Arm a timer to signal the fence in 0.5s.
        def timeline_signal(timeline):
            print("signaling timeline @%u" % timeline.value)
            timeline.signal(2)

        Timer(0.5, timeline_signal, self.timeline)


def main(argv):
    if len(argv) > 1:
        conn_name = argv[1]
    else:
        conn_name = ''

    card = pykms.Card()
    if not card.has_atomic:
        raise RuntimeError('This test requires atomic update support')

    res = pykms.ResourceManager(card)
    conn = res.reserve_connector(conn_name)
    crtc = res.reserve_crtc(conn)
    mode = conn.get_default_mode()

    flip_handler = FlipHandler(crtc, mode.hdisplay, mode.vdisplay)

    fb = flip_handler.fb1
    pykms.draw_color_bar(fb, fb.width - bar_width - bar_speed, bar_speed, bar_width)
    mode_blob = mode.to_blob(card)

    req = pykms.AtomicReq(card)
    req.add(conn, 'CRTC_ID', crtc.id)
    req.add(crtc, { 'ACTIVE': 1, 'MODE_ID': mode_blob.id })
    req.add(crtc.primary_plane, {
                'FB_ID': fb.id,
                'CRTC_ID': crtc.id,
                'SRC_X': 0 << 16,
                'SRC_Y': 0 << 16,
                'SRC_W': fb.width << 16,
                'SRC_H': fb.height << 16,
                'CRTC_X': 0,
                'CRTC_Y': 0,
                'CRTC_W': fb.width,
                'CRTC_H': fb.height,
    })
    ret = req.commit(allow_modeset = True)
    if ret < 0:
        raise RuntimeError('Atomic mode set failed with %d' % ret)

    def bye():
        # Signal the timeline to complete all pending page flips
        flip_handler.timeline.signal(100)
        exit(0)

    def readdrm(fileobj, mask):
        for ev in card.read_events():
            if ev.type == pykms.DrmEventType.FLIP_COMPLETE:
                flip_handler.handle_page_flip(ev.seq, ev.time)

    def readkey(fileobj, mask):
        sys.stdin.readline()
        bye()

    sel = selectors.DefaultSelector()
    sel.register(card.fd, selectors.EVENT_READ, readdrm)
    sel.register(sys.stdin, selectors.EVENT_READ, readkey)

    while True:
        timeout = Timer.next_timeout()
        print("--> timeout %s" % repr(timeout))
        try:
            events = sel.select(timeout)
        except KeyboardInterrupt:
            bye()
        for key, mask in events:
            callback = key.data
            callback(key.fileobj, mask)

        Timer.fire()

if __name__ == '__main__':
    main(sys.argv)
