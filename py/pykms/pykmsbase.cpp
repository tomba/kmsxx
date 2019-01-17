#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <kms++/kms++.h>

namespace py = pybind11;

using namespace kms;
using namespace std;

// Helper to convert vector<T*> to vector<unique_ptr<T, py::nodelete>>
template<typename T>
static vector<unique_ptr<T, py::nodelete>> convert_vector(const vector<T*>& source)
{
	vector<unique_ptr<T, py::nodelete>> v;
	for (T* p : source)
		v.push_back(unique_ptr<T, py::nodelete>(p));
	return v;
}

void init_pykmsbase(py::module &m)
{
	py::class_<Card>(m, "Card")
			.def(py::init<>())
			.def(py::init<const string&>())
			.def(py::init<const string&, uint32_t>())
			.def_property_readonly("fd", &Card::fd)
			.def_property_readonly("get_first_connected_connector", &Card::get_first_connected_connector)

			// XXX pybind11 can't handle vector<T*> where T is non-copyable, and complains:
			// RuntimeError: return_value_policy = move, but the object is neither movable nor copyable!
			// So we do this manually.
			.def_property_readonly("connectors", [](Card* self) {
				return convert_vector(self->get_connectors());
			})

			.def_property_readonly("crtcs", [](Card* self) {
				return convert_vector(self->get_crtcs());
			})

			.def_property_readonly("encoders", [](Card* self) {
				return convert_vector(self->get_encoders());
			})

			.def_property_readonly("planes", [](Card* self) {
				return convert_vector(self->get_planes());
			})

			.def_property_readonly("has_atomic", &Card::has_atomic)
			.def("get_prop", (Property* (Card::*)(uint32_t) const)&Card::get_prop)

			.def_property_readonly("version_name", &Card::version_name);
			;

	py::class_<DrmObject, unique_ptr<DrmObject, py::nodelete>>(m, "DrmObject")
			.def_property_readonly("id", &DrmObject::id)
			.def_property_readonly("idx", &DrmObject::idx)
			.def_property_readonly("card", &DrmObject::card)
			;

	py::class_<DrmPropObject, DrmObject, unique_ptr<DrmPropObject, py::nodelete>>(m, "DrmPropObject")
			.def("refresh_props", &DrmPropObject::refresh_props)
			.def_property_readonly("prop_map", &DrmPropObject::get_prop_map)
			.def("get_prop_value", (uint64_t (DrmPropObject::*)(const string&) const)&DrmPropObject::get_prop_value)
			.def("set_prop_value",(int (DrmPropObject::*)(const string&, uint64_t)) &DrmPropObject::set_prop_value)
			.def("get_prop_value_as_blob", &DrmPropObject::get_prop_value_as_blob)
			.def("get_prop", &DrmPropObject::get_prop)
			;

	py::class_<Connector, DrmPropObject, unique_ptr<Connector, py::nodelete>>(m, "Connector")
			.def_property_readonly("fullname", &Connector::fullname)
			.def("get_default_mode", &Connector::get_default_mode)
			.def("get_current_crtc", &Connector::get_current_crtc)
			.def("get_possible_crtcs", [](Connector* self) {
				return convert_vector(self->get_possible_crtcs());
			})
			.def("get_modes", &Connector::get_modes)
			.def("get_mode", (Videomode (Connector::*)(const string& mode) const)&Connector::get_mode)
			.def("get_mode", (Videomode (Connector::*)(unsigned xres, unsigned yres, float refresh, bool ilace) const)&Connector::get_mode)
			.def("connected", &Connector::connected)
			.def("__repr__", [](const Connector& o) { return "<pykms.Connector " + to_string(o.id()) + ">"; })
			.def("refresh", &Connector::refresh)
			;

	py::class_<Crtc, DrmPropObject, unique_ptr<Crtc, py::nodelete>>(m, "Crtc")
			.def("set_mode", (int (Crtc::*)(Connector*, const Videomode&))&Crtc::set_mode)
			.def("set_mode", (int (Crtc::*)(Connector*, Framebuffer&, const Videomode&))&Crtc::set_mode)
			.def("disable_mode", &Crtc::disable_mode)
			.def("page_flip",
			     [](Crtc* self, Framebuffer& fb, uint32_t data)
				{
					self->page_flip(fb, (void*)(intptr_t)data);
				}, py::arg("fb"), py::arg("data") = 0)
			.def("set_plane", &Crtc::set_plane)
			.def_property_readonly("possible_planes", &Crtc::get_possible_planes)
			.def_property_readonly("primary_plane", &Crtc::get_primary_plane)
			.def_property_readonly("mode", &Crtc::mode)
			.def_property_readonly("mode_valid", &Crtc::mode_valid)
			.def("__repr__", [](const Crtc& o) { return "<pykms.Crtc " + to_string(o.id()) + ">"; })
			.def("refresh", &Crtc::refresh)
			;

	py::class_<Encoder, DrmPropObject, unique_ptr<Encoder, py::nodelete>>(m, "Encoder")
			.def("refresh", &Encoder::refresh)
			;

	py::class_<Plane, DrmPropObject, unique_ptr<Plane, py::nodelete>>(m, "Plane")
			.def("supports_crtc", &Plane::supports_crtc)
			.def_property_readonly("formats", &Plane::get_formats)
			.def_property_readonly("plane_type", &Plane::plane_type)
			.def("__repr__", [](const Plane& o) { return "<pykms.Plane " + to_string(o.id()) + ">"; })
			;

	py::enum_<PlaneType>(m, "PlaneType")
			.value("Overlay", PlaneType::Overlay)
			.value("Primary", PlaneType::Primary)
			.value("Cursor", PlaneType::Cursor)
			;

	py::class_<Property, DrmObject, unique_ptr<Property, py::nodelete>>(m, "Property")
			.def_property_readonly("name", &Property::name)
			.def_property_readonly("enums", &Property::get_enums)
			;

	py::class_<Blob>(m, "Blob")
			.def("__init__", [](Blob& instance, Card& card, py::buffer buf) {
					py::buffer_info info = buf.request();
					if (info.ndim != 1)
						throw std::runtime_error("Incompatible buffer dimension!");

					new (&instance) Blob(card, info.ptr, info.size * info.itemsize);
				},
				py::keep_alive<1, 3>())	// Keep Card alive until this is destructed

			.def_property_readonly("data", &Blob::data)

			// XXX pybind11 doesn't support a base object (DrmObject) with custom holder-type,
			// and a subclass with standard holder-type.
			// So we just copy the DrmObject members here.
			// Note that this means that python thinks we don't derive from DrmObject
			.def_property_readonly("id", &DrmObject::id)
			.def_property_readonly("idx", &DrmObject::idx)
			.def_property_readonly("card", &DrmObject::card)
			;

	py::class_<Framebuffer>(m, "Framebuffer")
			.def_property_readonly("width", &Framebuffer::width)
			.def_property_readonly("height", &Framebuffer::height)

			// XXX pybind11 doesn't support a base object (DrmObject) with custom holder-type,
			// and a subclass with standard holder-type.
			// So we just copy the DrmObject members here.
			// Note that this means that python thinks we don't derive from DrmObject
			.def_property_readonly("id", &DrmObject::id)
			.def_property_readonly("idx", &DrmObject::idx)
			.def_property_readonly("card", &DrmObject::card)
			;

	py::class_<DumbFramebuffer, Framebuffer>(m, "DumbFramebuffer")
			.def(py::init<Card&, uint32_t, uint32_t, const string&>(),
			     py::keep_alive<1, 2>())	// Keep Card alive until this is destructed
			.def(py::init<Card&, uint32_t, uint32_t, PixelFormat>(),
			     py::keep_alive<1, 2>())	// Keep Card alive until this is destructed
			.def_property_readonly("format", &DumbFramebuffer::format)
			.def_property_readonly("num_planes", &DumbFramebuffer::num_planes)
			.def("fd", &DumbFramebuffer::prime_fd)
			.def("stride", &DumbFramebuffer::stride)
			.def("offset", &DumbFramebuffer::offset)
			;

	py::class_<ExtFramebuffer, Framebuffer>(m, "ExtFramebuffer")
			.def(py::init<Card&, uint32_t, uint32_t, PixelFormat, vector<int>, vector<uint32_t>, vector<uint32_t>>(),
			     py::keep_alive<1, 2>())	// Keep Card alive until this is destructed
			;

	py::enum_<PixelFormat>(m, "PixelFormat")
			.value("Undefined", PixelFormat::Undefined)

			.value("NV12", PixelFormat::NV12)
			.value("NV21", PixelFormat::NV21)

			.value("UYVY", PixelFormat::UYVY)
			.value("YUYV", PixelFormat::YUYV)
			.value("YVYU", PixelFormat::YVYU)
			.value("VYUY", PixelFormat::VYUY)

			.value("XRGB8888", PixelFormat::XRGB8888)
			.value("XBGR8888", PixelFormat::XBGR8888)
			.value("ARGB8888", PixelFormat::ARGB8888)
			.value("ABGR8888", PixelFormat::ABGR8888)

			.value("RGB888", PixelFormat::RGB888)
			.value("BGR888", PixelFormat::BGR888)

			.value("RGB565", PixelFormat::RGB565)
			.value("BGR565", PixelFormat::BGR565)
			;

	py::enum_<SyncPolarity>(m, "SyncPolarity")
			.value("Undefined", SyncPolarity::Undefined)
			.value("Positive", SyncPolarity::Positive)
			.value("Negative", SyncPolarity::Negative)
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

			.def("__repr__", [](const Videomode& vm) { return "<pykms.Videomode " + to_string(vm.hdisplay) + "x" + to_string(vm.vdisplay) + ">"; })

			.def("to_blob", &Videomode::to_blob)

			.def_property("hsync", &Videomode::hsync, &Videomode::set_hsync)
			.def_property("vsync", &Videomode::vsync, &Videomode::set_vsync)
			;


	m.def("videomode_from_timings", &videomode_from_timings);

	py::class_<AtomicReq>(m, "AtomicReq")
			.def(py::init<Card&>(),
			     py::keep_alive<1, 2>())	// Keep Card alive until this is destructed
			.def("add", (void (AtomicReq::*)(DrmPropObject*, const string&, uint64_t)) &AtomicReq::add)
			.def("add", (void (AtomicReq::*)(DrmPropObject*, const map<string, uint64_t>&)) &AtomicReq::add)
			.def("test", &AtomicReq::test, py::arg("allow_modeset") = false)
			.def("commit",
			     [](AtomicReq* self, uint32_t data, bool allow)
				{
					return self->commit((void*)(intptr_t)data, allow);
				}, py::arg("data") = 0, py::arg("allow_modeset") = false)
			.def("commit_sync", &AtomicReq::commit_sync, py::arg("allow_modeset") = false)
			;
}
