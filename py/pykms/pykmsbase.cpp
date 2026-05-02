#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/map.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/tuple.h>
#include <nanobind/stl/unique_ptr.h>
#include <nanobind/stl/vector.h>
#include <new>
#include <kms++/kms++.h>

namespace py = nanobind;

using namespace kms;
using namespace std;

template<typename T, typename Owner>
static py::list convert_vector(const vector<T*>& source, Owner& owner)
{
	py::list v;
	py::object parent = py::find(owner);
	for (T* p : source)
		v.append(py::cast(p, py::rv_policy::reference_internal, parent));
	return v;
}

struct PyBuffer {
	Py_buffer view;

	PyBuffer(py::handle obj)
	{
		if (PyObject_GetBuffer(obj.ptr(), &view, PyBUF_STRIDES) != 0)
			throw py::python_error();
	}

	~PyBuffer()
	{
		PyBuffer_Release(&view);
	}
};

void init_pykmsbase(py::module_& m)
{
	py::class_<Card>(m, "Card", R"doc(
DRM/KMS card handle.
)doc")
		.def(py::init<>())
		.def(py::init<const string&>(), py::arg("dev_path"))
		.def(py::init<const string&, uint32_t>(), py::arg("driver"), py::arg("idx"))
		.def_prop_ro("fd", &Card::fd)
		.def_prop_ro("minor", &Card::dev_minor)
		.def_prop_ro("get_first_connected_connector", &Card::get_first_connected_connector,
		             py::rv_policy::reference_internal)

		// Return borrowed KMS objects as Python references without transferring ownership.
		.def_prop_ro("connectors", [](Card* self) {
			return convert_vector(self->get_connectors(), *self);
		})

		.def_prop_ro("crtcs", [](Card* self) {
			return convert_vector(self->get_crtcs(), *self);
		})

		.def_prop_ro("encoders", [](Card* self) {
			return convert_vector(self->get_encoders(), *self);
		})

		.def_prop_ro("planes", [](Card* self) {
			return convert_vector(self->get_planes(), *self);
		})

		.def_prop_ro("properties", [](Card* self) {
			return convert_vector(self->get_properties(), *self);
		})

		.def_prop_ro("has_atomic", &Card::has_atomic)
		.def("get_prop", (Property * (Card::*)(uint32_t) const) & Card::get_prop,
		     py::arg("id"), py::rv_policy::reference_internal)

		.def_prop_ro("version_name", &Card::version_name);
	;

	py::class_<DrmObject>(m, "DrmObject", py::never_destruct(), R"doc(
Base class for DRM objects owned by a Card.
)doc")
		.def_prop_ro("id", &DrmObject::id)
		.def_prop_ro("idx", &DrmObject::idx)
		.def_prop_ro("card", &DrmObject::card,
		             py::rv_policy::reference_internal);

	py::class_<DrmPropObject, DrmObject>(m, "DrmPropObject", py::never_destruct(), R"doc(
DRM object with properties.
)doc")
		.def("refresh_props", &DrmPropObject::refresh_props)
		.def_prop_ro("prop_map", &DrmPropObject::get_prop_map)
		.def("get_prop_value", (uint64_t(DrmPropObject::*)(const string&) const) & DrmPropObject::get_prop_value,
		     py::arg("name"))
		.def("set_prop_value", (int(DrmPropObject::*)(const string&, uint64_t)) & DrmPropObject::set_prop_value,
		     py::arg("name"), py::arg("value"))
		.def("get_prop_value_as_blob", &DrmPropObject::get_prop_value_as_blob,
		     py::arg("name"), py::keep_alive<0, 1>())
		.def("get_prop", &DrmPropObject::get_prop,
		     py::arg("name"), py::rv_policy::reference_internal)
		.def("has_prop", &DrmPropObject::has_prop, py::arg("name"));

	py::class_<Connector, DrmPropObject>(m, "Connector", py::never_destruct(), R"doc(
DRM connector such as HDMI, eDP, or DisplayPort.
)doc")
		.def_prop_ro("fullname", &Connector::fullname)
		.def("get_default_mode", &Connector::get_default_mode)
		.def("get_current_crtc", &Connector::get_current_crtc,
		     py::rv_policy::reference_internal)
		.def("get_possible_crtcs", [](Connector* self) {
			return convert_vector(self->get_possible_crtcs(), *self);
		})
		.def("get_modes", &Connector::get_modes)
		.def("get_mode", (Videomode(Connector::*)(const string& mode) const) & Connector::get_mode,
		     py::arg("mode"))
		.def("get_mode", (Videomode(Connector::*)(unsigned xres, unsigned yres, float refresh, bool ilace) const) & Connector::get_mode,
		     py::arg("xres"), py::arg("yres"), py::arg("refresh"), py::arg("ilace"))
		.def("connected", &Connector::connected)
		.def("__repr__", [](const Connector& o) { return "<pykms.Connector " + to_string(o.id()) + ">"; })
		.def("refresh", &Connector::refresh);

	py::class_<Crtc, DrmPropObject>(m, "Crtc", py::never_destruct(), R"doc(
DRM CRTC object controlling scanout timing.
)doc")
		.def("set_mode", (int(Crtc::*)(Connector*, const Videomode&)) & Crtc::set_mode,
		     py::arg("connector"), py::arg("mode"))
		.def("set_mode", (int(Crtc::*)(Connector*, Framebuffer&, const Videomode&)) & Crtc::set_mode,
		     py::arg("connector"), py::arg("fb"), py::arg("mode"))
		.def("disable_mode", &Crtc::disable_mode)
		.def(
			"page_flip",
			[](Crtc* self, Framebuffer& fb, uint32_t data) {
				self->page_flip(fb, (void*)(intptr_t)data);
			},
			py::arg("fb"), py::arg("data") = 0)
		.def("set_plane", &Crtc::set_plane,
		     py::arg("plane"), py::arg("fb"), py::arg("dst_x"), py::arg("dst_y"),
		     py::arg("dst_w"), py::arg("dst_h"), py::arg("src_x"), py::arg("src_y"),
		     py::arg("src_w"), py::arg("src_h"))
		.def_prop_ro("possible_planes", [](Crtc* self) {
			return convert_vector(self->get_possible_planes(), *self);
		})
		.def_prop_ro("primary_plane", &Crtc::get_primary_plane,
		             py::rv_policy::reference_internal)
		.def_prop_ro("mode", &Crtc::mode)
		.def_prop_ro("mode_valid", &Crtc::mode_valid)
		.def("__repr__", [](const Crtc& o) { return "<pykms.Crtc " + to_string(o.id()) + ">"; })
		.def("refresh", &Crtc::refresh)
		.def("legacy_gamma_size", &Crtc::legacy_gamma_size)
		.def("legacy_gamma_set", &Crtc::legacy_gamma_set, py::arg("gamma"));

	py::class_<Encoder, DrmPropObject>(m, "Encoder", py::never_destruct(), R"doc(
DRM encoder object.
)doc")
		.def("refresh", &Encoder::refresh);

	py::class_<Plane, DrmPropObject>(m, "Plane", py::never_destruct(), R"doc(
DRM plane object.
)doc")
		.def("supports_crtc", &Plane::supports_crtc, py::arg("crtc"))
		.def_prop_ro("formats", &Plane::get_formats)
		.def_prop_ro("plane_type", &Plane::plane_type)
		.def("__repr__", [](const Plane& o) { return "<pykms.Plane " + to_string(o.id()) + ">"; });

	py::enum_<PlaneType>(m, "PlaneType")
		.value("Overlay", PlaneType::Overlay)
		.value("Primary", PlaneType::Primary)
		.value("Cursor", PlaneType::Cursor);

	py::class_<Property, DrmObject>(m, "Property", py::never_destruct(), R"doc(
DRM property descriptor.
)doc")
		.def_prop_ro("name", &Property::name)
		.def_prop_ro("type", &Property::type)
		.def_prop_ro("enums", &Property::get_enums)
		.def_prop_ro("values", &Property::get_values)
		.def("__repr__", [](const Property& o) { return "<pykms.Property " + to_string(o.id()) + " '" + o.name() + "'>"; });

	py::enum_<PropertyType>(m, "PropertyType")
		.value("Range", PropertyType::Range)
		.value("Enum", PropertyType::Enum)
		.value("Blob", PropertyType::Blob)
		.value("Bitmask", PropertyType::Bitmask)
		.value("Object", PropertyType::Object)
		.value("SignedRange", PropertyType::SignedRange);

	py::class_<Blob>(m, "Blob", R"doc(
DRM property blob.
)doc")
		.def("__init__", [](Blob* self, Card& card, py::handle buf) {
			     PyBuffer info(buf);
			     if (info.view.ndim != 1)
				     throw std::runtime_error("Incompatible buffer dimension!");

			     new (self) Blob(card, info.view.buf, (size_t)info.view.len);
		     },
		     py::arg("card"), py::arg("buf"),
		     py::keep_alive<1, 2>(), // Keep Card alive until this is destructed
		     R"doc(
Create a DRM property blob from a one-dimensional Python buffer.

Args:
    card (Card): DRM card that owns the blob.
    buf (Buffer): One-dimensional source buffer.
Raises:
    RuntimeError: If the buffer is not one-dimensional.
)doc")

		.def_prop_ro("data", &Blob::data)

		// Keep the historical Python API where Blob exposes DrmObject members
		// without deriving from DrmObject on the Python side.
		.def_prop_ro("id", &DrmObject::id)
		.def_prop_ro("idx", &DrmObject::idx)
		.def_prop_ro("card", &DrmObject::card,
		             py::rv_policy::reference_internal);

	py::class_<Framebuffer>(m, "Framebuffer", R"doc(
Base class for DRM framebuffer objects.
)doc")
		.def_prop_ro("width", &Framebuffer::width)
		.def_prop_ro("height", &Framebuffer::height)
		.def_prop_ro("format", &Framebuffer::format)
		.def_prop_ro("num_planes", &Framebuffer::num_planes)
		.def("stride", &Framebuffer::stride, py::arg("plane"))
		.def("size", &Framebuffer::size, py::arg("plane"))
		.def("offset", &Framebuffer::offset, py::arg("plane"))
		.def("fd", &Framebuffer::prime_fd, py::arg("plane"))

		.def("flush", (void(Framebuffer::*)(void)) & Framebuffer::flush)
		.def("flush", (void(Framebuffer::*)(uint32_t x, uint32_t y, uint32_t width, uint32_t height)) & Framebuffer::flush,
		     py::arg("x"), py::arg("y"), py::arg("width"), py::arg("height"))

		// Keep the historical Python API where Framebuffer exposes DrmObject
		// members without deriving from DrmObject on the Python side.
		.def_prop_ro("id", &DrmObject::id)
		.def_prop_ro("idx", &DrmObject::idx)
		.def_prop_ro("card", &DrmObject::card,
		             py::rv_policy::reference_internal)
		.def("map", [](Framebuffer& self, uint32_t plane) {
			const auto& format_info = get_pixel_format_info(self.format());

			if (plane >= format_info.num_planes)
				throw runtime_error("map: bad plane number");

			return py::ndarray<uint8_t, py::memview, py::ndim<2>>(
				self.map(plane),
				{ self.height(), format_info.stride(self.width(), plane) },
				py::find(self),
				{ self.stride(plane), 1 });
		},
		py::arg("plane"),
		R"doc(
Map a framebuffer plane as a writable two-dimensional memoryview.

Args:
    plane (int): Plane index to map.
Raises:
    RuntimeError: If the plane index is out of range.
)doc");


	py::class_<DumbFramebuffer, Framebuffer>(m, "DumbFramebuffer", R"doc(
Framebuffer backed by a DRM dumb buffer.
)doc")
		.def(py::init<Card&, uint32_t, uint32_t, const string&>(),
		     py::arg("card"), py::arg("width"), py::arg("height"), py::arg("fourcc"),
		     py::keep_alive<1, 2>()) // Keep Card alive until this is destructed
		.def(py::init<Card&, uint32_t, uint32_t, PixelFormat>(),
		     py::arg("card"), py::arg("width"), py::arg("height"), py::arg("format"),
		     py::keep_alive<1, 2>()) // Keep Card alive until this is destructed
		.def("__repr__", [](const DumbFramebuffer& o) { return "<pykms.DumbFramebuffer " + to_string(o.id()) + ">"; });

	py::class_<DmabufFramebuffer, Framebuffer>(m, "DmabufFramebuffer", R"doc(
Framebuffer backed by imported dma-buf file descriptors.
)doc")
		.def(py::init<Card&, uint32_t, uint32_t, const string&, vector<int>, vector<uint32_t>, vector<uint32_t>>(),
		     py::arg("card"), py::arg("width"), py::arg("height"), py::arg("fourcc"),
		     py::arg("fds"), py::arg("pitches"), py::arg("offsets"),
		     py::keep_alive<1, 2>()) // Keep Card alive until this is destructed
		.def(py::init<Card&, uint32_t, uint32_t, PixelFormat, vector<int>, vector<uint32_t>, vector<uint32_t>>(),
		     py::arg("card"), py::arg("width"), py::arg("height"), py::arg("format"),
		     py::arg("fds"), py::arg("pitches"), py::arg("offsets"),
		     py::keep_alive<1, 2>()) // Keep Card alive until this is destructed
		.def("__repr__", [](const DmabufFramebuffer& o) { return "<pykms.DmabufFramebuffer " + to_string(o.id()) + ">"; });

	py::enum_<PixelFormat>(m, "PixelFormat")
		.value("Undefined", PixelFormat::Undefined)

		.value("NV12", PixelFormat::NV12)
		.value("NV21", PixelFormat::NV21)
		.value("NV16", PixelFormat::NV16)
		.value("NV61", PixelFormat::NV61)

		.value("YUV420", PixelFormat::YUV420)
		.value("YVU420", PixelFormat::YVU420)
		.value("YUV422", PixelFormat::YUV422)
		.value("YVU422", PixelFormat::YVU422)
		.value("YUV444", PixelFormat::YUV444)
		.value("YVU444", PixelFormat::YVU444)

		.value("UYVY", PixelFormat::UYVY)
		.value("YUYV", PixelFormat::YUYV)
		.value("YVYU", PixelFormat::YVYU)
		.value("VYUY", PixelFormat::VYUY)

		.value("XRGB8888", PixelFormat::XRGB8888)
		.value("XBGR8888", PixelFormat::XBGR8888)
		.value("RGBX8888", PixelFormat::RGBX8888)
		.value("BGRX8888", PixelFormat::BGRX8888)

		.value("ARGB8888", PixelFormat::ARGB8888)
		.value("ABGR8888", PixelFormat::ABGR8888)
		.value("RGBA8888", PixelFormat::RGBA8888)
		.value("BGRA8888", PixelFormat::BGRA8888)

		.value("RGB888", PixelFormat::RGB888)
		.value("BGR888", PixelFormat::BGR888)

		.value("RGB332", PixelFormat::RGB332)

		.value("RGB565", PixelFormat::RGB565)
		.value("BGR565", PixelFormat::BGR565)

		.value("XRGB4444", PixelFormat::XRGB4444)
		.value("XRGB1555", PixelFormat::XRGB1555)

		.value("ARGB4444", PixelFormat::ARGB4444)
		.value("ARGB1555", PixelFormat::ARGB1555)

		.value("XRGB2101010", PixelFormat::XRGB2101010)
		.value("XBGR2101010", PixelFormat::XBGR2101010)
		.value("RGBX1010102", PixelFormat::RGBX1010102)
		.value("BGRX1010102", PixelFormat::BGRX1010102)

		.value("ARGB2101010", PixelFormat::ARGB2101010)
		.value("ABGR2101010", PixelFormat::ABGR2101010)
		.value("RGBA1010102", PixelFormat::RGBA1010102)
		.value("BGRA1010102", PixelFormat::BGRA1010102);

	m.def("fourcc_to_pixelformat", &fourcc_str_to_pixel_format, py::arg("fourcc"),
	      R"doc(
Convert a fourcc string to a PixelFormat value.

Args:
    fourcc (str): Four-character DRM format code.
)doc");
	m.def("pixelformat_to_fourcc", &pixel_format_to_fourcc_str, py::arg("format"),
	      R"doc(
Convert a PixelFormat value to a fourcc string.

Args:
    format (PixelFormat): Pixel format value.
)doc");

	py::enum_<SyncPolarity>(m, "SyncPolarity")
		.value("Undefined", SyncPolarity::Undefined)
		.value("Positive", SyncPolarity::Positive)
		.value("Negative", SyncPolarity::Negative);

	py::class_<Videomode>(m, "Videomode", R"doc(
Video timing mode.
)doc")
		.def(py::init<>())

		.def_rw("name", &Videomode::name)

		.def_rw("clock", &Videomode::clock)

		.def_rw("hdisplay", &Videomode::hdisplay)
		.def_rw("hsync_start", &Videomode::hsync_start)
		.def_rw("hsync_end", &Videomode::hsync_end)
		.def_rw("htotal", &Videomode::htotal)

		.def_rw("vdisplay", &Videomode::vdisplay)
		.def_rw("vsync_start", &Videomode::vsync_start)
		.def_rw("vsync_end", &Videomode::vsync_end)
		.def_rw("vtotal", &Videomode::vtotal)

		.def_rw("vrefresh", &Videomode::vrefresh)

		.def_rw("flags", &Videomode::flags)
		.def_rw("type", &Videomode::type)

		.def("__repr__", [](const Videomode& vm) { return "<pykms.Videomode " + to_string(vm.hdisplay) + "x" + to_string(vm.vdisplay) + ">"; })

		.def("to_blob", &Videomode::to_blob,
		     py::arg("card"), py::keep_alive<0, 2>())

		.def_prop_rw("hsync", &Videomode::hsync, &Videomode::set_hsync)
		.def_prop_rw("vsync", &Videomode::vsync, &Videomode::set_vsync)

		.def("to_string_short", &Videomode::to_string_short)
		.def("to_string_long", &Videomode::to_string_long);

	m.def("videomode_from_timings", &videomode_from_timings,
	      py::arg("clock_khz"), py::arg("hact"), py::arg("hfp"), py::arg("hsw"), py::arg("hbp"),
	      py::arg("vact"), py::arg("vfp"), py::arg("vsw"), py::arg("vbp"));

	py::class_<AtomicReq>(m, "AtomicReq", R"doc(
DRM atomic commit request.
)doc")
		.def(py::init<Card&>(),
		     py::arg("card"), py::keep_alive<1, 2>()) // Keep Card alive until this is destructed
		.def("add", (void(AtomicReq::*)(DrmPropObject*, const string&, uint64_t)) & AtomicReq::add,
		     py::arg("obj"), py::arg("prop"), py::arg("value"))
		.def("add", (void(AtomicReq::*)(DrmPropObject*, Property*, uint64_t)) & AtomicReq::add,
		     py::arg("obj"), py::arg("prop"), py::arg("value"))
		.def("add", (void(AtomicReq::*)(DrmPropObject*, const map<string, uint64_t>&)) & AtomicReq::add,
		     py::arg("obj"), py::arg("values"))
		.def("test", &AtomicReq::test, py::arg("allow_modeset") = false)
		.def(
			"commit",
			[](AtomicReq* self, uint32_t data, bool allow) {
				return self->commit((void*)(intptr_t)data, allow);
			},
			py::arg("data") = 0, py::arg("allow_modeset") = false)
		.def("commit_sync", &AtomicReq::commit_sync, py::arg("allow_modeset") = false);

	py::class_<PixelFormatPlaneInfo>(m, "PixelFormatPlaneInfo", R"doc(
Per-plane pixel format information.
)doc")
		.def_ro("bytes_per_block", &PixelFormatPlaneInfo::bytes_per_block)
		.def_ro("pixels_per_block", &PixelFormatPlaneInfo::pixels_per_block)
		.def_ro("hsub", &PixelFormatPlaneInfo::hsub)
		.def_ro("vsub", &PixelFormatPlaneInfo::vsub);

	py::class_<PixelFormatInfo>(m, "PixelFormatInfo", R"doc(
Pixel format layout information.
)doc")
		.def_ro("num_planes", &PixelFormatInfo::num_planes)
		.def(
			"plane", [](const PixelFormatInfo& self, uint32_t idx) {
				if (idx >= self.num_planes)
					throw runtime_error("invalid plane number");
				return self.planes[idx];
			},
			py::arg("idx"));

	m.def("get_pixel_format_info", &get_pixel_format_info, py::arg("format"),
	      py::rv_policy::reference,
	      R"doc(
Return layout metadata for a pixel format.

Args:
    format (PixelFormat): Pixel format value.
Raises:
    RuntimeError: If the format is unknown.
)doc");
}
