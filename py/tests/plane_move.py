#!/usr/bin/python3

import pykms
import random
import time
import sys
import select
import selectors

if len(sys.argv) != 3:
    print('Usage: plane_move.py <connector0> <connector1>')
    sys.exit()

card = pykms.Card()

if not card.has_atomic:
    print('Atomic modesetting is not supported')
    sys.exit(-1)

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
    if conn is None:
        print('Invalid connector: {}'.format(sys.argv[i + 1]))
        sys.exit(-1)

    if conn.connected() == True:
        conn_list.append(conn)
    else:
        print('connector: {} is not connected'.format(sys.argv[i + 1]))
        sys.exit(-1)

    crtc = res.reserve_crtc(conn)
    crtc_list.append(crtc)

    mode = conn.get_default_mode()
    mode_list.append(mode)

    fb_tmp = pykms.DumbFramebuffer(card, src_w, src_h, 'XR24');
    fb_list.append(fb_tmp)

    rootplane = res.reserve_primary_plane(crtc, pykms.PixelFormat.XRGB8888)
    rootplane_list.append(rootplane)

card.disable_planes()

print('Using the following connectors:')
for i in range(2):
    print(' {}: {} ({}x{})'.format(conn_list[i].idx, conn_list[i].fullname,
        mode_list[i].hdisplay, mode_list[i].vdisplay))

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
