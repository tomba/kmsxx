#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <kms++/kms++.h>

namespace py = pybind11;

using namespace kms;
using namespace std;

void init_pykmsbase(py::module &m)
{
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
			.def_property_readonly("card", &DrmObject::card)
			;

	py::class_<DrmPropObject, DrmPropObject*>(m, "DrmPropObject", py::base<DrmObject>())
			.def("refresh_props", &DrmPropObject::refresh_props)
			.def_property_readonly("prop_map", &DrmPropObject::get_prop_map)
			.def("get_prop_value", (uint64_t (DrmPropObject::*)(const string&) const)&DrmPropObject::get_prop_value)
			.def("set_prop_value",(int (DrmPropObject::*)(const string&, uint64_t)) &DrmPropObject::set_prop_value)
			.def("get_prop_value_as_blob", &DrmPropObject::get_prop_value_as_blob)
			;

	py::class_<Connector, Connector*>(m, "Connector",  py::base<DrmPropObject>())
			.def_property_readonly("fullname", &Connector::fullname)
			.def("get_default_mode", &Connector::get_default_mode)
			.def("get_current_crtc", &Connector::get_current_crtc)
			.def("get_possible_crtcs", &Connector::get_possible_crtcs)
			.def("get_modes", &Connector::get_modes)
			.def("__repr__", [](const Connector& o) { return "<pykms.Connector " + to_string(o.id()) + ">"; })
			;

	py::class_<Crtc, Crtc*>(m, "Crtc",  py::base<DrmPropObject>())
			.def("set_mode", &Crtc::set_mode)
			.def("page_flip", &Crtc::page_flip)
			.def("set_plane", &Crtc::set_plane)
			.def_property_readonly("possible_planes", &Crtc::get_possible_planes)
			.def_property_readonly("primary_plane", &Crtc::get_primary_plane)
			.def_property_readonly("mode", &Crtc::mode)
			.def_property_readonly("mode_valid", &Crtc::mode_valid)
			.def("__repr__", [](const Crtc& o) { return "<pykms.Crtc " + to_string(o.id()) + ">"; })
			;

	py::class_<Encoder, Encoder*>(m, "Encoder",  py::base<DrmPropObject>())
			;

	py::class_<Plane, Plane*>(m, "Plane",  py::base<DrmPropObject>())
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

	py::class_<Blob>(m, "Blob", py::base<DrmObject>())
			.def("__init__", [](Blob& instance, Card& card, py::buffer buf) {
				py::buffer_info info = buf.request();
				if (info.ndim != 1)
					throw std::runtime_error("Incompatible buffer dimension!");

				new (&instance) Blob(card, info.ptr, info.size * info.itemsize);
			})
			.def_property_readonly("data", &Blob::data)
			;

	py::class_<Framebuffer>(m, "Framebuffer",  py::base<DrmObject>())
			;

	py::class_<DumbFramebuffer>(m, "DumbFramebuffer",  py::base<Framebuffer>())
			.def(py::init<Card&, uint32_t, uint32_t, const string&>(),
			     py::keep_alive<1, 2>())	// Keep Card alive until this is destructed
			.def(py::init<Card&, uint32_t, uint32_t, PixelFormat>(),
			     py::keep_alive<1, 2>())	// Keep Card alive until this is destructed
			.def_property_readonly("width", &DumbFramebuffer::width)
			.def_property_readonly("height", &DumbFramebuffer::height)
			;

	py::enum_<PixelFormat>(m, "PixelFormat")
			.value("XRGB8888", PixelFormat::XRGB8888)
			.value("YUYV", PixelFormat::YUYV)
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

	py::class_<AtomicReq>(m, "AtomicReq")
			.def(py::init<Card&>(),
			     py::keep_alive<1, 2>())	// Keep Card alive until this is destructed
			.def("add", (void (AtomicReq::*)(DrmObject*, const string&, uint64_t)) &AtomicReq::add)
			.def("test", &AtomicReq::test, py::arg("allow_modeset") = false)
			.def("commit", &AtomicReq::commit, py::arg("data"), py::arg("allow_modeset") = false)
			.def("commit_sync", &AtomicReq::commit_sync, py::arg("allow_modeset") = false)
			;
}
