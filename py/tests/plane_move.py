#!/usr/bin/python3

import pykms
import random
import time
import sys
import select
import selectors

if len(sys.argv) not in (1, 3):
    print('Without parameters: list of available connectors')
    print('{} [name of connector0] [name of connector1]\n'.format(sys.argv[0]))
    sys.exit(-1)

card = pykms.Card()

if not card.has_atomic:
    print('Atomic mode settings is not supported :(')
    sys.exit(-1)

if len(sys.argv) == 1:
    print('List of available connectors:')
    for conn in card.connectors:
        if conn.connected() == 1:
            mode = conn.get_default_mode()
            print(' {}: {} ({}x{})'.format(conn.idx, conn.fullname,
                                           mode.hdisplay, mode.vdisplay))
    print('\nNow you can try:')
    print('{} [name of connector0] [name of connector1]\n'.format(sys.argv[0]))
    sys.exit()

res = pykms.ResourceManager(card)

conn_list = []
crtc_list = []
mode_list = []
rootplane_list = []
fb_list = []
colors = []

src_w = 300
src_h = 300

for i in range(2):
    conn = res.reserve_connector(sys.argv[i + 1])
    if conn.connected() == 1:
        conn_list.append(conn)

if len(conn_list) != 2:
    print('We need two connectors')
    sys.exit(-1)

card.disable_planes()

print('Using the following connectors:')
for conn in conn_list:
    crtc = res.reserve_crtc(conn)
    crtc_list.append(crtc)

    mode = conn.get_default_mode()
    mode_list.append(mode)

    fb_tmp = pykms.DumbFramebuffer(card, src_w, src_h, 'XR24');
    fb_list.append(fb_tmp)

    rootplane = res.reserve_primary_plane(crtc, pykms.PixelFormat.XRGB8888)
    rootplane_list.append(rootplane)

    print(' {}: {} ({}x{})'.format(conn.idx, conn.fullname,
        mode.hdisplay, mode.vdisplay))

colors.append(pykms.red)
colors.append(pykms.green)

for i in range(2):
    pykms.draw_rect(fb_list[i], 0, 0, src_w, src_h, colors[i])

for i in range(2):
    req = pykms.AtomicReq(card)
    modeb = mode_list[i].to_blob(card)
    req.add(conn_list[i], 'CRTC_ID', crtc_list[i].id)
    req.add(crtc_list[i], {'ACTIVE': 1,
                    'MODE_ID': modeb.id})
    req.add(rootplane_list[i], {'FB_ID': fb_list[i].id,
                'CRTC_ID': crtc_list[i].id,
                'SRC_W': src_w << 16,
                'SRC_H': src_h << 16,
                'CRTC_W': src_w,
                'CRTC_H': src_h})

    req.commit_sync(allow_modeset = True)

print('\nRed box on {}, Green box on {}.'.format(conn_list[0].fullname,
                                                 conn_list[1].fullname))
input('ENTER to continue\n')

# FIXME: it should be possible to move plane without disabling it, but the
# omapdrm driver does not supports it at the moment.
req = pykms.AtomicReq(card)
req.add(rootplane_list[0], {"FB_ID": 0,
                "CRTC_ID": 0})
req.commit_sync(allow_modeset = True)

req = pykms.AtomicReq(card)
req.add(rootplane_list[0], {'FB_ID': fb_list[0].id,
            'CRTC_ID': crtc_list[1].id,
            'SRC_W': src_w << 16,
            'SRC_H': src_h << 16,
            'CRTC_X': 150,
            'CRTC_Y': 150,
            'CRTC_W': src_w,
            'CRTC_H': src_h})
req.commit_sync(allow_modeset = True)

print('The red box from {} is moved underneath the green box on {}.'.format(
                                conn_list[0].fullname, conn_list[1].fullname))
input('ENTER to continue\n')

# FIXME: it should be possible to move plane without disabling it, but the
# omapdrm driver does not supports it at the moment.
req = pykms.AtomicReq(card)
req.add(rootplane_list[1], {"FB_ID": 0,
                "CRTC_ID": 0})
req.commit_sync(allow_modeset = True)

req = pykms.AtomicReq(card)
req.add(rootplane_list[1], {'FB_ID': fb_list[1].id,
            'CRTC_ID': crtc_list[0].id,
            'SRC_W': src_w << 16,
            'SRC_H': src_h << 16,
            'CRTC_W': src_w,
            'CRTC_H': src_h})
req.commit_sync(allow_modeset = True)

print('Green box on {}, Red box on {}.'.format(conn_list[0].fullname,
                                               conn_list[1].fullname))
input('ENTER to exit\n')
