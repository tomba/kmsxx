#!/usr/bin/python3

import urwid
import pykms

def exit_on_q(key):
	if key in ('q', 'Q'):
		raise urwid.ExitMainLoop()
	elif key == 'a':
		apply_mode()

alarm_handle = None

def recalc_info(l, d):
	global alarm_handle

	alarm_handle = None

	for w in recalc_list:
		w.recalc()

def div_or_zero(n, d):
	if d == 0:
		return 0
	else:
		return n / d

class MyIntEdit(urwid.IntEdit):
	_metaclass_ = urwid.signals.MetaSignals
	signals = ['value_change']

	def __init__(self, caption, calc=None):
		self._myval = 0
		self._disable_change = False
		self._calc = calc
		self._updlist = None

		super().__init__(caption, 0)

	def set_edit_text(self, text):
		global alarm_handle

		super().set_edit_text(text)
		newtext = super().get_edit_text()
		new_val = int(newtext) if newtext != "" else 0
		if new_val != self._myval:
			self._myval = new_val
			if not self._disable_change:
				urwid.emit_signal(self, 'value_change', self, self._myval)

				if alarm_handle == None:
					alarm_handle = loop.set_alarm_in(0, recalc_info)

				if self._updlist != None:
					for w in self._updlist:
						w.recalc()

	def recalc(self):
		self._disable_change = True
		self.set_val(self._calc())
		self._disable_change = False

	def set_val(self, val):
		self.set_edit_text(str(int(val)))

	def get_val(self):
		return self._myval

	def set_updlist(self, list):
		self._updlist = list

	def keypress(self, size, key):
		if key == '+':
			self.set_edit_text(str(self.value() + 1))
		elif key == '-':
			self.set_edit_text(str(self.value() - 1))
		else:
			return super().keypress(size, key)

class MyIntText(urwid.Text):
	def __init__(self, fmt, calc=None):
		super().__init__("")
		self._fmt = fmt
		self._calc = calc

	def recalc(self):
		val = self._calc()
		super().set_text(self._fmt.format(val))

def khz_to_ps(khz):
	if khz == 0:
		return 0
	else:
		return 1.0 / khz * 1000 * 1000 * 1000

def khz_to_us(khz):
	if khz == 0:
		return 0
	else:
		return 1.0 / khz * 1000

pclk_khz_widget = MyIntEdit(u"pclk (kHz) ")
pclk_ps_widget = MyIntText(fmt="pclk {:.2f} ps", calc = lambda: khz_to_ps(pclk_khz_widget.get_val()))

pclk_widgets = [pclk_khz_widget, pclk_ps_widget]

pclk_columns = urwid.LineBox(urwid.Columns(pclk_widgets), title = "Pixel clock")

# Horizontal widgets

hdisp_widget = MyIntEdit(u"hdisp ", calc = lambda: hdisp2_widget.get_val())
hfp_widget = MyIntEdit(u"hfp   ", calc = lambda: hss_widget.get_val() - hdisp_widget.get_val())
hsw_widget = MyIntEdit(u"hsw   ", calc = lambda: hse_widget.get_val() - hss_widget.get_val())
hbp_widget = MyIntEdit(u"hbp   ", calc = lambda: htot_widget.get_val() - hse_widget.get_val())

hdisp2_widget = MyIntEdit(u"hdisp ", calc = lambda: hdisp_widget.get_val())
hss_widget = MyIntEdit(u"hss   ",
	calc = lambda: hdisp_widget.get_val() + hfp_widget.get_val())
hse_widget = MyIntEdit(u"hse   ",
	calc = lambda: hdisp_widget.get_val() + hfp_widget.get_val() + hsw_widget.get_val())
htot_widget = MyIntEdit(u"htot  ",
	calc = lambda: hdisp_widget.get_val() + hfp_widget.get_val() + hsw_widget.get_val() + hbp_widget.get_val())

hwidgets1 = [hdisp_widget, hfp_widget, hsw_widget, hbp_widget]
hwidgets2 = [hdisp2_widget, hss_widget, hse_widget, htot_widget]

horiz_pile1 = urwid.Pile(hwidgets1)
horiz_pile2 = urwid.Pile(hwidgets2)

h_columns = urwid.LineBox(urwid.Columns([(15, horiz_pile1), (15, horiz_pile2)]), title = "Horizontal")

# Vertical columns

vdisp_widget = MyIntEdit(u"vdisp ", calc = lambda: vdisp2_widget.get_val())
vfp_widget = MyIntEdit(u"vfp   ", calc = lambda: vss_widget.get_val() - vdisp_widget.get_val())
vsw_widget = MyIntEdit(u"vsw   ", calc = lambda: vse_widget.get_val() - vss_widget.get_val())
vbp_widget = MyIntEdit(u"vbp   ", calc = lambda: vtot_widget.get_val() - vse_widget.get_val())

vdisp2_widget = MyIntEdit(u"vdisp ", calc = lambda: vdisp_widget.get_val())
vss_widget = MyIntEdit(u"vss   ",
	calc = lambda: vdisp_widget.get_val() + vfp_widget.get_val())
vse_widget = MyIntEdit(u"vse   ",
	calc = lambda: vdisp_widget.get_val() + vfp_widget.get_val() + vsw_widget.get_val())
vtot_widget = MyIntEdit(u"vtot  ",
	calc = lambda: vdisp_widget.get_val() + vfp_widget.get_val() + vsw_widget.get_val() + vbp_widget.get_val())

vwidgets1 = [vdisp_widget, vfp_widget, vsw_widget, vbp_widget]
vwidgets2 = [vdisp2_widget, vss_widget, vse_widget, vtot_widget]

vert_pile1 = urwid.Pile(vwidgets1)
vert_pile2 = urwid.Pile(vwidgets2)

v_columns = urwid.LineBox(urwid.Columns([(15, vert_pile1), (15, vert_pile2)]), title = "Vertical")

# Info widgets

line_us_widget = MyIntText(fmt="line {:.2f} us",
	calc = lambda: khz_to_us(pclk_khz_widget.get_val()) * htot_widget.get_val())
line_khz_widget = MyIntText(fmt="line {:.2f} kHz",
	calc = lambda: div_or_zero(pclk_khz_widget.get_val(), htot_widget.get_val()))

frame_tot_widget = MyIntText(fmt="tot {} pix",
	calc = lambda: htot_widget.get_val() * vtot_widget.get_val())
frame_us_widget = MyIntText(fmt="frame {:.2f} ms",
	calc = lambda: khz_to_us(pclk_khz_widget.get_val()) * htot_widget.get_val() * vtot_widget.get_val() / 1000)
frame_khz_widget = MyIntText(fmt="frame {:.2f} Hz",
	calc = lambda: div_or_zero(pclk_khz_widget.get_val() * 1000,  htot_widget.get_val() * vtot_widget.get_val()))

info_box = urwid.LineBox(urwid.Pile([line_us_widget, line_khz_widget, urwid.Divider(), frame_tot_widget, frame_us_widget, frame_khz_widget]), title = "Info")

# Set update lists

recalc_list = [ pclk_ps_widget, line_us_widget, line_khz_widget, frame_tot_widget, frame_us_widget, frame_khz_widget ]

hdisp_widget.set_updlist([hdisp2_widget, hss_widget, hse_widget, htot_widget])
hfp_widget.set_updlist([hss_widget, hse_widget, htot_widget])
hsw_widget.set_updlist([hse_widget, htot_widget])
hbp_widget.set_updlist([htot_widget])
hdisp2_widget.set_updlist([hdisp_widget, hfp_widget])
hss_widget.set_updlist([hfp_widget, hsw_widget])
hse_widget.set_updlist([hsw_widget, hbp_widget])
htot_widget.set_updlist([hbp_widget])

vdisp_widget.set_updlist([vdisp2_widget, vss_widget, vse_widget, vtot_widget])
vfp_widget.set_updlist([vss_widget, vse_widget, vtot_widget])
vsw_widget.set_updlist([vse_widget, vtot_widget])
vbp_widget.set_updlist([vtot_widget])
vdisp2_widget.set_updlist([vdisp_widget, vfp_widget])
vss_widget.set_updlist([vfp_widget, vsw_widget])
vse_widget.set_updlist([vsw_widget, vbp_widget])
vtot_widget.set_updlist([vbp_widget])

# Flags

fb = None

DRM_MODE_FLAG_PHSYNC = (1<<0)
DRM_MODE_FLAG_NHSYNC = (1<<1)
DRM_MODE_FLAG_PVSYNC = (1<<2)
DRM_MODE_FLAG_NVSYNC = (1<<3)
DRM_MODE_FLAG_INTERLACE = (1<<4)
DRM_MODE_FLAG_DBLCLK = (1<<12)

def mode_is_ilace(mode):
	return (mode.flags & DRM_MODE_FLAG_INTERLACE) != 0

def apply_mode():
	global fb

	mode = pykms.Videomode()
	mode.clock = pclk_khz_widget.get_val()

	mode.hdisplay = hdisp2_widget.get_val()
	mode.hsync_start = hss_widget.get_val()
	mode.hsync_end = hse_widget.get_val()
	mode.htotal = htot_widget.get_val()

	mode.vdisplay = vdisp2_widget.get_val()
	mode.vsync_start = vss_widget.get_val()
	mode.vsync_end = vse_widget.get_val()
	mode.vtotal = vtot_widget.get_val()

	if ilace_box.state:
		mode.flags |= DRM_MODE_FLAG_INTERLACE

	if dblclk_box.state:
		mode.flags |= DRM_MODE_FLAG_DBLCLK

	if hsync_pol.state == True:
		mode.flags |= DRM_MODE_FLAG_PHSYNC
	elif hsync_pol.state == False:
		mode.flags |= DRM_MODE_FLAG_NHSYNC

	if vsync_pol.state == True:
		mode.flags |= DRM_MODE_FLAG_PVSYNC
	elif vsync_pol.state == False:
		mode.flags |= DRM_MODE_FLAG_NVSYNC

	fb = pykms.DumbFramebuffer(card, mode.hdisplay, mode.vdisplay, "XR24");
	pykms.draw_test_pattern(fb);

	crtc.set_mode(conn, fb, mode)

def read_mode(mode):
	pclk_khz_widget.set_val(mode.clock)
	hdisp2_widget.set_val(mode.hdisplay)
	hss_widget.set_val(mode.hsync_start)
	hse_widget.set_val(mode.hsync_end)
	htot_widget.set_val(mode.htotal)

	vdisp2_widget.set_val(mode.vdisplay)
	vss_widget.set_val(mode.vsync_start)
	vse_widget.set_val(mode.vsync_end)
	vtot_widget.set_val(mode.vtotal)

	ilace_box.set_state(mode_is_ilace(mode))
	dblclk_box.set_state((mode.flags & DRM_MODE_FLAG_DBLCLK) != 0)

	sync = 'mixed'
	if (mode.flags & DRM_MODE_FLAG_PHSYNC) != 0:
		sync = True
	elif (mode.flags & DRM_MODE_FLAG_NHSYNC) != 0:
		sync = False
	hsync_pol.set_state(sync)

	sync = 'mixed'
	if (mode.flags & DRM_MODE_FLAG_PVSYNC) != 0:
		sync = True
	elif (mode.flags & DRM_MODE_FLAG_NVSYNC) != 0:
		sync = False
	vsync_pol.set_state(sync)

def apply_press(w):
	apply_mode()

ilace_box = urwid.CheckBox('interlace')
hsync_pol = urwid.CheckBox('hsync positive', has_mixed=True)
vsync_pol = urwid.CheckBox('vsync positive', has_mixed=True)
dblclk_box = urwid.CheckBox('double clock')

flags_pile = urwid.LineBox(urwid.Pile([ilace_box, hsync_pol, vsync_pol, dblclk_box]), title = "Flags")

apply_button = urwid.LineBox(urwid.Padding(urwid.Button('apply', on_press=apply_press)))

# Main

def mode_press(w, mode):
	read_mode(mode)

def mode_to_str(mode):
	return "{}@{}{}".format(mode.name, mode.vrefresh, "i" if mode_is_ilace(mode) else "")

mode_buttons = []

card = pykms.Card()
conn = card.get_first_connected_connector()
crtc = conn.get_current_crtc()
modes = conn.get_modes()
i = 0
for m in modes:
	mode_buttons.append(urwid.Button(mode_to_str(m), on_press=mode_press, user_data=m))
	i += 1

modes_pile = urwid.LineBox(urwid.Pile(mode_buttons), title = "Video modes")

main_pile = urwid.Pile([modes_pile, pclk_columns, urwid.Columns([ h_columns, v_columns ]), info_box, flags_pile, apply_button])

main_columns = urwid.Filler(main_pile, valign='top')

loop = urwid.MainLoop(main_columns, unhandled_input=exit_on_q, handle_mouse=False)

# select the first mode
mode_press(None, modes[0])

loop.run()

fb = None
