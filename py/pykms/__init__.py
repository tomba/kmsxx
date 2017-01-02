from .pykms import *

#
# Common RGB colours
#

red = RGB(255, 0, 0)
green = RGB(0, 255, 0)
blue = RGB(0, 0, 255)
yellow = RGB(255, 255, 0)
purple = RGB(255, 0, 255)
white = RGB(255, 255, 255)
cyan = RGB(0, 255, 255)

#
# DrmObject API extensions
#

def __obj_set_prop(self, prop, value):
    if self.card.has_atomic:
        areq = AtomicReq(self.card)
        areq.add(self, prop, value)
        if areq.commit_sync() != 0:
            print("commit failed")
    else:
        if self.set_prop_value(prop, value) != 0:
            print("setting property failed")

def __obj_set_props(self, map):
    if self.card.has_atomic:
        areq = AtomicReq(self.card)

        for key, value in map.items():
            areq.add(self, key, value)

        if areq.commit_sync() != 0:
            print("commit failed")
    else:
        for propid,propval in map.items():
            if self.set_prop_value(propid, propval) != 0:
                print("setting property failed")

DrmObject.set_prop = __obj_set_prop
DrmObject.set_props = __obj_set_props

#
# Card API extensions
#

def __card_disable_planes(self):
    areq = AtomicReq(self)

    for p in self.planes:
        areq.add(p, "FB_ID", 0)
        areq.add(p, "CRTC_ID", 0)

    if areq.commit_sync() != 0:
        print("disabling planes failed")

Card.disable_planes = __card_disable_planes
