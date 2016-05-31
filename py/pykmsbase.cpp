#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <kms++.h>

namespace py = pybind11;

using namespace kms;
using namespace std;

void init_pykmsbase(py::module &m)
{
	py::class_<Card>(m, "Card")
			.def(py::init<>())
			.def_property_readonly("fd", &Card::fd)
			.def("get_first_connected_connector", &Card::get_first_connected_connector)
			.def("get_object", &Card::get_object)
			.def("get_connector", &Card::get_connector)
			.def("get_crtc", &Card::get_crtc)
			.def("get_crtc_by_index", &Card::get_crtc_by_index)
			.def("get_encoder", &Card::get_encoder)
			.def("get_plane", &Card::get_plane)
			.def_property_readonly("connectors", &Card::get_connectors)
			.def_property_readonly("crtcs", &Card::get_crtcs)
			.def_property_readonly("encoders", &Card::get_encoders)
			.def_property_readonly("planes", &Card::get_planes)
			.def_property_readonly("properties", &Card::get_properties)
			.def_property_readonly("objects", &Card::get_objects)
			.def_property_readonly("master", &Card::master)
			.def_property_readonly("has_atomic", &Card::has_atomic)
			.def_property_readonly("has_universal_planes", &Card::has_has_universal_planes)
			.def_property_readonly("connected_pipelines", &Card::get_connected_pipelines)
			.def("call_page_flip_handlers", &Card::call_page_flip_handlers)
			.def("get_prop", (Property* (Card::*)(uint32_t) const)&Card::get_prop)
			.def("get_prop", (Property* (Card::*)(const string&) const)&Card::get_prop)
			;

	py::class_<DrmObject, DrmObject*>(m, "DrmObject")
			.def_property_readonly("id", &DrmObject::id)
			.def_property_readonly("card", &DrmObject::card)
			.def_property_readonly("object_type", &DrmObject::object_type)
			.def_property_readonly("idx", &DrmObject::idx)
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
			.def_property_readonly("connector_type", &Connector::connector_type)
			.def_property_readonly("connector_type_id", &Connector::connector_type_id)
			.def_property_readonly("mmWidth", &Connector::mmWidth)
			.def_property_readonly("mmHeight", &Connector::mmHeight)
			.def_property_readonly("subpixel", &Connector::subpixel)
			.def_property_readonly("subpixel_str", &Connector::subpixel_str)
			.def("get_modes", &Connector::get_modes)
			.def("get_encoders", &Connector::get_encoders)
			.def("__repr__", [](const Connector& o) { return "<pykms.Connector " + to_string(o.id()) + ">"; })
			;

	py::class_<Crtc, Crtc*>(m, "Crtc",  py::base<DrmPropObject>())
			.def("set_mode", &Crtc::set_mode)
			.def("page_flip", &Crtc::page_flip)
			.def("set_plane", &Crtc::set_plane)
			.def_property_readonly("possible_planes", &Crtc::get_possible_planes)
			.def_property_readonly("primary_plane", &Crtc::get_primary_plane)
			.def_property_readonly("buffer_id", &Crtc::buffer_id)
			.def_property_readonly("x", &Crtc::x)
			.def_property_readonly("y", &Crtc::y)
			.def_property_readonly("width", &Crtc::width)
			.def_property_readonly("height", &Crtc::height)
			.def_property_readonly("mode_valid", &Crtc::mode_valid)
			.def_property_readonly("mode", &Crtc::mode)
			.def_property_readonly("gamma_size", &Crtc::gamma_size)
			.def("__repr__", [](const Crtc& o) { return "<pykms.Crtc " + to_string(o.id()) + ">"; })
			;

	py::class_<Encoder, Encoder*>(m, "Encoder",  py::base<DrmPropObject>())
			.def_property_readonly("crtc", &Encoder::get_crtc)
			.def_property_readonly("possible_crtcs", &Encoder::get_possible_crtcs)
			.def_property_readonly("encoder_type", &Encoder::get_encoder_type)
			;

	py::class_<Plane, Plane*>(m, "Plane",  py::base<DrmPropObject>())
			.def("supports_crtc", &Plane::supports_crtc)
			.def("supports_format", &Plane::supports_format)
			.def_property_readonly("plane_type", &Plane::plane_type)
			.def_property_readonly("formats", &Plane::get_formats)
			.def_property_readonly("crtc_id", &Plane::crtc_id)
			.def_property_readonly("fb_id", &Plane::fb_id)
			.def_property_readonly("crtc_x", &Plane::crtc_x)
			.def_property_readonly("crtc_y", &Plane::crtc_y)
			.def_property_readonly("x", &Plane::x)
			.def_property_readonly("y", &Plane::y)
			.def_property_readonly("gamma_size", &Plane::gamma_size)
			.def("__repr__", [](const Plane& o) { return "<pykms.Plane " + to_string(o.id()) + ">"; })
			;

	py::enum_<PlaneType>(m, "PlaneType")
			.value("Overlay", PlaneType::Overlay)
			.value("Primary", PlaneType::Primary)
			.value("Cursor", PlaneType::Cursor)
			;

	py::class_<Property, Property*>(m, "Property",  py::base<DrmObject>())
			.def_property_readonly("name", &Property::name)
			.def("to_str", &Property::to_str)
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
			.def_property_readonly("width", &Framebuffer::width)
			.def_property_readonly("height", &Framebuffer::height)
			;

	py::class_<DumbFramebuffer>(m, "DumbFramebuffer",  py::base<Framebuffer>())
			.def(py::init<Card&, uint32_t, uint32_t, const string&>(),
			     py::keep_alive<1, 2>())	// Keep Card alive until this is destructed
			.def_property_readonly("width", &DumbFramebuffer::width)
			.def_property_readonly("height", &DumbFramebuffer::height)
			.def_property_readonly("format", &DumbFramebuffer::format)
			.def_property_readonly("num_planes", &DumbFramebuffer::num_planes)
			.def_property_readonly("handle", &DumbFramebuffer::handle)
			.def_property_readonly("stride", &DumbFramebuffer::stride)
			.def_property_readonly("size", &DumbFramebuffer::size)
			.def_property_readonly("offset", &DumbFramebuffer::offset)
			.def_property_readonly("map", &DumbFramebuffer::map)
			.def_property_readonly("prime_fd", &DumbFramebuffer::prime_fd)
			;

	py::class_<Videomode>(m, "Videomode")
			.def(py::init<>())

			.def_readwrite("name", &Videomode::name)

			.def_readwrite("clock", &Videomode::clock)

			.def_readwrite("hdisplay", &Videomode::hdisplay)
			.def_readwrite("hsync_start", &Videomode::hsync_start)
			.def_readwrite("hsync_end", &Videomode::hsync_end)
			.def_readwrite("htotal", &Videomode::htotal)
			.def_readwrite("hskew", &Videomode::hskew)

			.def_readwrite("vdisplay", &Videomode::vdisplay)
			.def_readwrite("vsync_start", &Videomode::vsync_start)
			.def_readwrite("vsync_end", &Videomode::vsync_end)
			.def_readwrite("vtotal", &Videomode::vtotal)
			.def_readwrite("vscan", &Videomode::vscan)

			.def_readwrite("vrefresh", &Videomode::vrefresh)

			.def_readwrite("flags", &Videomode::flags)
			.def_readwrite("type", &Videomode::type)
			;

	py::class_<AtomicReq>(m, "AtomicReq")
			.def(py::init<Card&>(),
			     py::keep_alive<1, 2>())	// Keep Card alive until this is destructed
			.def("add", (void (AtomicReq::*)(uint32_t, uint32_t, uint64_t)) &AtomicReq::add)
			.def("add", (void (AtomicReq::*)(DrmObject*, Property*, uint64_t)) &AtomicReq::add)
			.def("add", (void (AtomicReq::*)(DrmObject*, const string&, uint64_t)) &AtomicReq::add)
			.def("test", &AtomicReq::test)
			.def("commit", &AtomicReq::commit)
			.def("commit_sync", &AtomicReq::commit_sync)
			;
}
