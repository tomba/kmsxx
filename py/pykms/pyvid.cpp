#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <kms++/kms++.h>
#include <kms++util/kms++util.h>
#include <kms++util/videodevice.h>
#include <kms++util/mediadevice.h>
#include <kms++util/videosubdev.h>
#include <fmt/format.h>

namespace py = pybind11;

using namespace kms;
using namespace std;

void init_pyvid(py::module& m)
{
	py::class_<VideoDevice>(m, "VideoDevice")
		.def(py::init<const string&>())
		.def_property_readonly("fd", &VideoDevice::fd)
		.def_property_readonly("has_capture", &VideoDevice::has_capture)
		.def_property_readonly("has_output", &VideoDevice::has_output)
		.def_property_readonly("has_m2m", &VideoDevice::has_m2m)
		.def_property_readonly("capture_streamer", &VideoDevice::get_capture_streamer)
		.def_property_readonly("output_streamer", &VideoDevice::get_output_streamer)
		.def_property_readonly("discrete_frame_sizes", &VideoDevice::get_discrete_frame_sizes)
		.def_property_readonly("frame_sizes", &VideoDevice::get_frame_sizes)
		.def("get_capture_devices", &VideoDevice::get_capture_devices);

	py::class_<VideoStreamer>(m, "VideoStreamer")
		.def_property_readonly("fd", &VideoStreamer::fd)
		.def_property_readonly("ports", &VideoStreamer::get_ports)
		.def("set_port", &VideoStreamer::set_port)
		.def_property_readonly("formats", &VideoStreamer::get_formats)
		.def("set_format", &VideoStreamer::set_format)
		.def("get_selection", [](VideoStreamer* self) {
			uint32_t left, top, width, height;
			self->get_selection(left, top, width, height);
			return make_tuple(left, top, width, height);
		})
		.def("set_selection", [](VideoStreamer* self, uint32_t left, uint32_t top, uint32_t width, uint32_t height) {
			self->set_selection(left, top, width, height);
			return make_tuple(left, top, width, height);
		})
		.def("set_queue_size", &VideoStreamer::set_queue_size)
		.def("queue", &VideoStreamer::queue)
		.def("dequeue", &VideoStreamer::dequeue)
		.def("stream_on", &VideoStreamer::stream_on)
		.def("stream_off", &VideoStreamer::stream_off);

#define BUS_FMT_ENUM(fmt) value(#fmt, BusFormat::fmt)

	py::enum_<BusFormat>(m, "BusFormat")
		.BUS_FMT_ENUM(FIXED)

		.BUS_FMT_ENUM(RGB444_2X8_PADHI_BE)
		.BUS_FMT_ENUM(RGB444_2X8_PADHI_LE)
		.BUS_FMT_ENUM(RGB555_2X8_PADHI_BE)
		.BUS_FMT_ENUM(RGB555_2X8_PADHI_LE)
		.BUS_FMT_ENUM(BGR565_2X8_BE)
		.BUS_FMT_ENUM(BGR565_2X8_LE)
		.BUS_FMT_ENUM(RGB565_2X8_BE)
		.BUS_FMT_ENUM(RGB565_2X8_LE)
		.BUS_FMT_ENUM(RGB666_1X18)
		.BUS_FMT_ENUM(RGB888_1X24)
		.BUS_FMT_ENUM(RGB888_2X12_BE)
		.BUS_FMT_ENUM(RGB888_2X12_LE)
		.BUS_FMT_ENUM(ARGB8888_1X32)

		.BUS_FMT_ENUM(Y8_1X8)
		.BUS_FMT_ENUM(UV8_1X8)
		.BUS_FMT_ENUM(UYVY8_1_5X8)
		.BUS_FMT_ENUM(VYUY8_1_5X8)
		.BUS_FMT_ENUM(YUYV8_1_5X8)
		.BUS_FMT_ENUM(YVYU8_1_5X8)
		.BUS_FMT_ENUM(UYVY8_2X8)
		.BUS_FMT_ENUM(VYUY8_2X8)
		.BUS_FMT_ENUM(YUYV8_2X8)
		.BUS_FMT_ENUM(YVYU8_2X8)
		.BUS_FMT_ENUM(Y10_1X10)
		.BUS_FMT_ENUM(UYVY10_2X10)
		.BUS_FMT_ENUM(VYUY10_2X10)
		.BUS_FMT_ENUM(YUYV10_2X10)
		.BUS_FMT_ENUM(YVYU10_2X10)
		.BUS_FMT_ENUM(Y12_1X12)
		.BUS_FMT_ENUM(UYVY8_1X16)
		.BUS_FMT_ENUM(VYUY8_1X16)
		.BUS_FMT_ENUM(YUYV8_1X16)
		.BUS_FMT_ENUM(YVYU8_1X16)
		.BUS_FMT_ENUM(YDYUYDYV8_1X16)
		.BUS_FMT_ENUM(UYVY10_1X20)
		.BUS_FMT_ENUM(VYUY10_1X20)
		.BUS_FMT_ENUM(YUYV10_1X20)
		.BUS_FMT_ENUM(YVYU10_1X20)
		.BUS_FMT_ENUM(YUV10_1X30)
		.BUS_FMT_ENUM(AYUV8_1X32)
		.BUS_FMT_ENUM(UYVY12_2X12)
		.BUS_FMT_ENUM(VYUY12_2X12)
		.BUS_FMT_ENUM(YUYV12_2X12)
		.BUS_FMT_ENUM(YVYU12_2X12)
		.BUS_FMT_ENUM(UYVY12_1X24)
		.BUS_FMT_ENUM(VYUY12_1X24)
		.BUS_FMT_ENUM(YUYV12_1X24)
		.BUS_FMT_ENUM(YVYU12_1X24)

		.BUS_FMT_ENUM(SBGGR8_1X8)
		.BUS_FMT_ENUM(SGBRG8_1X8)
		.BUS_FMT_ENUM(SGRBG8_1X8)
		.BUS_FMT_ENUM(SRGGB8_1X8)
		.BUS_FMT_ENUM(SBGGR10_ALAW8_1X8)
		.BUS_FMT_ENUM(SGBRG10_ALAW8_1X8)
		.BUS_FMT_ENUM(SGRBG10_ALAW8_1X8)
		.BUS_FMT_ENUM(SRGGB10_ALAW8_1X8)
		.BUS_FMT_ENUM(SBGGR10_DPCM8_1X8)
		.BUS_FMT_ENUM(SGBRG10_DPCM8_1X8)
		.BUS_FMT_ENUM(SGRBG10_DPCM8_1X8)
		.BUS_FMT_ENUM(SRGGB10_DPCM8_1X8)
		.BUS_FMT_ENUM(SBGGR10_2X8_PADHI_BE)
		.BUS_FMT_ENUM(SBGGR10_2X8_PADHI_LE)
		.BUS_FMT_ENUM(SBGGR10_2X8_PADLO_BE)
		.BUS_FMT_ENUM(SBGGR10_2X8_PADLO_LE)
		.BUS_FMT_ENUM(SBGGR10_1X10)
		.BUS_FMT_ENUM(SGBRG10_1X10)
		.BUS_FMT_ENUM(SGRBG10_1X10)
		.BUS_FMT_ENUM(SRGGB10_1X10)
		.BUS_FMT_ENUM(SBGGR12_1X12)
		.BUS_FMT_ENUM(SGBRG12_1X12)
		.BUS_FMT_ENUM(SGRBG12_1X12)
		.BUS_FMT_ENUM(SRGGB12_1X12)

		.BUS_FMT_ENUM(JPEG_1X8)

		.BUS_FMT_ENUM(S5C_UYVY_JPEG_1X8)

		.BUS_FMT_ENUM(AHSV8888_1X32);

	py::class_<VideoSubdev>(m, "ViodeSubdev")
		.def("set_format", &VideoSubdev::set_format)
		.def("get_format", [](VideoSubdev& self, uint32_t pad) {
			uint32_t w, h;
			BusFormat fmt;

			self.get_format(pad, w, h, fmt);

			return make_tuple(w, h, fmt);
		})
		.def("get_routing", &VideoSubdev::get_routing)
	;

	py::class_<SubdevRoute>(m, "SubdevRoute")
		.def_readwrite("sink_pad", &SubdevRoute::sink_pad)
		.def_readwrite("sink_stream", &SubdevRoute::sink_stream)
		.def_readwrite("source_pad", &SubdevRoute::source_pad)
		.def_readwrite("source_stream", &SubdevRoute::source_stream)
		.def_readwrite("active", &SubdevRoute::active)
		.def_readonly("immutable", &SubdevRoute::immutable)
		.def("__repr__", [](const SubdevRoute& r) {
			return fmt::format("<pykms.SubdevRoute {}/{} -> {}/{}{}{}>",
					   r.sink_pad, r.sink_stream, r.source_pad, r.source_stream,
					   r.active ? " active" : "", r.immutable ? " immutable" : "");
		})
		;

	py::class_<MediaDevice>(m, "MediaDevice")
		.def(py::init<const string&>())
		.def("find_entity", &MediaDevice::find_entity, py::return_value_policy::reference_internal)
		.def("print", &MediaDevice::print)
		;

	py::class_<MediaEntity>(m, "MediaEntity")
		.def_property_readonly("id", &MediaEntity::id)
		.def_property_readonly("name", &MediaEntity::name)
		.def_readonly("dev_path", &MediaEntity::dev_path)
		.def_property_readonly(
			"subdev", [](MediaEntity& self) { return self.subdev.get(); }, py::return_value_policy::reference_internal)
		.def("get_linked_entities", [](MediaEntity& self, uint32_t pad_idx) {
			py::list l;
			for (auto e : self.get_linked_entities(pad_idx)) {
				py::object py_owner = py::cast(self.m_media_device);
				py::object py_cam = py::cast(e);
				py::detail::keep_alive_impl(py_cam, py_owner);
				l.append(py_cam);
			}
			return l;
		})
		;
}
