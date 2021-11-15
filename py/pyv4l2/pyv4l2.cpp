#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <v4l2++/videodevice.h>
#include <fmt/format.h>

namespace py = pybind11;

using namespace v4l2;
using namespace std;

PYBIND11_MODULE(pyv4l2, m)
{
	py::class_<VideoDevice>(m, "VideoDevice")
		.def(py::init<const string&>())
		.def_property_readonly("fd", &VideoDevice::fd)
		.def_property_readonly("has_capture", &VideoDevice::has_capture)
		.def_property_readonly("has_output", &VideoDevice::has_output)
		.def_property_readonly("has_m2m", &VideoDevice::has_m2m)
		.def_property_readonly("capture_streamer", &VideoDevice::get_capture_streamer)
		.def_property_readonly("output_streamer", &VideoDevice::get_output_streamer)
		.def_property_readonly("meta_capture_streamer", &VideoDevice::get_meta_capture_streamer)
		.def_property_readonly("discrete_frame_sizes", &VideoDevice::get_discrete_frame_sizes)
		.def_property_readonly("frame_sizes", &VideoDevice::get_frame_sizes)
		.def("get_capture_devices", &VideoDevice::get_capture_devices);

	py::enum_<VideoMemoryType>(m, "VideoMemoryType")
		.value("MMAP", VideoMemoryType::MMAP)
		.value("DMABUF", VideoMemoryType::DMABUF)
		;

	m.def("create_dmabuffer", [](int fd) {
		VideoBuffer buf {};
		buf.m_mem_type = VideoMemoryType::DMABUF;
		buf.m_fd = fd;
		return buf;
	});

	m.def("create_mmapbuffer", []() {
		VideoBuffer buf {};
		buf.m_mem_type = VideoMemoryType::MMAP;
		return buf;
	});

	py::class_<VideoBuffer>(m, "VideoBuffer")
		.def_readonly("index", &VideoBuffer::m_index)
		.def_readonly("offset", &VideoBuffer::m_offset)
		.def_readonly("fd", &VideoBuffer::m_fd)
		.def_readonly("length", &VideoBuffer::m_length)
		;

	py::class_<VideoStreamer>(m, "VideoStreamer")
		.def_property_readonly("fd", &VideoStreamer::fd)
		.def_property_readonly("ports", &VideoStreamer::get_ports)
		.def("set_port", &VideoStreamer::set_port)
		.def_property_readonly("formats", &VideoStreamer::get_formats)
		.def("get_format", [](VideoStreamer* self) {
			PixelFormat fmt;
			uint32_t w, h;

			int r = self->get_format(fmt, w, h);
			if (r)
				__throw_exception_again std::system_error(errno, std::generic_category(), "get_format failed");

			return make_tuple(w, h, fmt);
		})
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

	py::class_<MetaStreamer>(m, "MetaStreamer")
		.def_property_readonly("fd", &MetaStreamer::fd)
		.def("set_format", &MetaStreamer::set_format)
		.def("set_queue_size", &MetaStreamer::set_queue_size)
		.def("queue", &MetaStreamer::queue)
		.def("dequeue", &MetaStreamer::dequeue)
		.def("stream_on", &MetaStreamer::stream_on)
		.def("stream_off", &MetaStreamer::stream_off);

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
		.value("BGRA1010102", PixelFormat::BGRA1010102)

		.value("SBGGR12", PixelFormat::SBGGR12)
		.value("SRGGB12", PixelFormat::SRGGB12)

		.value("META_8", PixelFormat::META_8)
		.value("META_16", PixelFormat::META_16);


	m.def("fourcc_to_pixelformat", &FourCCToPixelFormat);
}
