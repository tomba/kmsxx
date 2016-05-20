import pykms

def add_props(areq, ob, map):
    for key, value in map.items():
        areq.add(ob, key, value)

def props(o):
    o.refresh_props()
    map = o.prop_map
    for propid,propval in map.items():
        prop = o.card.get_prop(propid)
        print("%-15s %d (%#x)" % (prop.name, propval, propval))

def set_props(ob, map):
    areq = pykms.AtomicReq(ob.card())

    for key, value in map.items():
        areq.add(ob, key, value)

    if areq.commit() != 0:
        print("commit failed")

red = pykms.RGB(255, 0, 0)
green = pykms.RGB(0, 255, 0)
blue = pykms.RGB(0, 0, 255)
yellow = pykms.RGB(255, 255, 0)
purple = pykms.RGB(255, 0, 255)
white = pykms.RGB(255, 255, 255)
cyan = pykms.RGB(0, 255, 255)

def disable_planes(card):
    areq = pykms.AtomicReq(card)

    for p in card.planes:
        areq.add(p, "FB_ID", 0)
        areq.add(p, "CRTC_ID", 0)

    if areq.commit() != 0:
        print("disabling planes failed")
