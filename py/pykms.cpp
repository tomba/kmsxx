#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <kms++.h>
#include "kmstest.h"

namespace py = pybind11;

using namespace kms;
using namespace std;

class PyPageFlipHandlerBase : PageFlipHandlerBase
{
public:
	using PageFlipHandlerBase::PageFlipHandlerBase;

	virtual void handle_page_flip(uint32_t frame, double time)
	{
		PYBIND11_OVERLOAD_PURE(
					void,                /* Return type */
					PageFlipHandlerBase, /* Parent class */
					handle_page_flip,    /* Name of function */
					frame, time
					);
	}
};

PYBIND11_PLUGIN(pykms) {
	py::module m("pykms", "kms bindings");

	py::class_<Card>(m, "Card")
			.def(py::init<>())
			.def_property_readonly("fd", &Card::fd)
			.def("get_first_connected_connector", &Card::get_first_connected_connector)
			.def_property_readonly("connectors", &Card::get_connectors)
			.def_property_readonly("crtcs", &Card::get_crtcs)
			.def_property_readonly("encoders", &Card::get_encoders)
			.def_property_readonly("planes", &Card::get_planes)
			.def_property_readonly("has_atomic", &Card::has_atomic)
			.def("call_page_flip_handlers", &Card::call_page_flip_handlers)
			.def("get_prop", (Property* (Card::*)(uint32_t) const)&Card::get_prop)
			.def("get_prop", (Property* (Card::*)(const string&) const)&Card::get_prop)
			;

	py::class_<DrmObject, DrmObject*>(m, "DrmObject")
			.def_property_readonly("id", &DrmObject::id)
			.def("refresh_props", &DrmObject::refresh_props)
			.def_property_readonly("prop_map", &DrmObject::get_prop_map)
			.def_property_readonly("card", &DrmObject::card)
			;

	py::class_<Connector, Connector*>(m, "Connector",  py::base<DrmObject>())
			.def_property_readonly("fullname", &Connector::fullname)
			.def("get_default_mode", &Connector::get_default_mode)
			.def("get_current_crtc", &Connector::get_current_crtc)
			.def("get_modes", &Connector::get_modes)
			.def("__repr__", [](const Connector& o) { return "<pykms.Connector " + to_string(o.id()) + ">"; })
			;

	py::class_<Crtc, Crtc*>(m, "Crtc",  py::base<DrmObject>())
			.def("set_mode", &Crtc::set_mode)
			.def("page_flip", &Crtc::page_flip)
			.def("set_plane", &Crtc::set_plane)
			.def_property_readonly("possible_planes", &Crtc::get_possible_planes)
			.def("__repr__", [](const Crtc& o) { return "<pykms.Crtc " + to_string(o.id()) + ">"; })
			;

	py::class_<Encoder, Encoder*>(m, "Encoder",  py::base<DrmObject>())
			;

	py::class_<Plane, Plane*>(m, "Plane",  py::base<DrmObject>())
			.def("supports_crtc", &Plane::supports_crtc)
			.def_property_readonly("plane_type", &Plane::plane_type)
			.def("__repr__", [](const Plane& o) { return "<pykms.Plane " + to_string(o.id()) + ">"; })
			;

	py::enum_<PlaneType>(m, "PlaneType")
			.value("Overlay", PlaneType::Overlay)
			.value("Primary", PlaneType::Primary)
			.value("Cursor", PlaneType::Cursor)
			;

	py::class_<Property, Property*>(m, "Property",  py::base<DrmObject>())
			.def_property_readonly("name", &Property::name)
			;

	py::class_<Framebuffer>(m, "Framebuffer",  py::base<DrmObject>())
			;

	py::class_<DumbFramebuffer>(m, "DumbFramebuffer",  py::base<Framebuffer>())
			.def(py::init<Card&, uint32_t, uint32_t, const string&>(),
			     py::keep_alive<1, 2>())	// Keep Card alive until this is destructed
			.def_property_readonly("width", &DumbFramebuffer::width)
			.def_property_readonly("height", &DumbFramebuffer::height)
			;

	py::class_<Videomode>(m, "Videomode")
			.def(py::init<>())

			.def_readwrite("name", &Videomode::name)

			.def_readwrite("clock", &Videomode::clock)

			.def_readwrite("hdisplay", &Videomode::hdisplay)
			.def_readwrite("hsync_start", &Videomode::hsync_start)
			.def_readwrite("hsync_end", &Videomode::hsync_end)
			.def_readwrite("htotal", &Videomode::htotal)

			.def_readwrite("vdisplay", &Videomode::vdisplay)
			.def_readwrite("vsync_start", &Videomode::vsync_start)
			.def_readwrite("vsync_end", &Videomode::vsync_end)
			.def_readwrite("vtotal", &Videomode::vtotal)

			.def_readwrite("vrefresh", &Videomode::vrefresh)

			.def_readwrite("flags", &Videomode::flags)
			.def_readwrite("type", &Videomode::type)
			;

	py::class_<PyPageFlipHandlerBase>(m, "PageFlipHandlerBase")
			.alias<PageFlipHandlerBase>()
			.def(py::init<>())
			.def("handle_page_flip", &PageFlipHandlerBase::handle_page_flip)
			;

	py::class_<AtomicReq>(m, "AtomicReq")
			.def(py::init<Card&>(),
			     py::keep_alive<1, 2>())	// Keep Card alive until this is destructed
			.def("add", (void (AtomicReq::*)(DrmObject*, const string&, uint64_t)) &AtomicReq::add)
			.def("test", &AtomicReq::test)
			.def("commit", &AtomicReq::commit)
			.def("commit_sync", &AtomicReq::commit_sync)
			;



	/* libkmstest */

	py::class_<RGB>(m, "RGB")
			.def(py::init<>())
			.def(py::init<uint8_t, uint8_t, uint8_t&>())
			.def(py::init<uint8_t, uint8_t, uint8_t, uint8_t&>())
			;

	// Use lambdas to handle IMappedFramebuffer
	m.def("draw_test_pattern", [](DumbFramebuffer& fb) { draw_test_pattern(fb); } );
	m.def("draw_color_bar", [](DumbFramebuffer& fb, int old_xpos, int xpos, int width) {
		draw_color_bar(fb, old_xpos, xpos, width);
	} );
	m.def("draw_rect", [](DumbFramebuffer& fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h, RGB color) {
		draw_rect(fb, x, y, w, h, color);
	} );

	return m.ptr();
}
