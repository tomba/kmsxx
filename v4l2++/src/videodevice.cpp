#include <string>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <system_error>

#include <v4l2++/videodevice.h>
#include <v4l2++/helpers.h>

using namespace std;
using namespace v4l2;

/*
 * V4L2 and DRM differ in their interpretation of YUV420::NV12
 *
 * V4L2 NV12 is a Y and UV co-located planes in a single plane buffer.
 * DRM NV12 is a Y and UV planes presented as dual plane buffer,
 * which is known as NM12 in V4L2.
 *
 * Since here we have hybrid DRM/V4L2 user space helper functions
 * we need to translate DRM::NV12 to V4L2:NM12 pixel format back
 * and forth to keep the data view consistent.
 */

static v4l2_memory get_mem_type(VideoMemoryType type)
{
	switch (type) {
	case VideoMemoryType::MMAP:
		return V4L2_MEMORY_MMAP;
	case VideoMemoryType::DMABUF:
		return V4L2_MEMORY_DMABUF;
	default:
		FAIL("Bad VideoMemoryType");
	}
}

/* V4L2 helper funcs */
static vector<PixelFormat> v4l2_get_formats(int fd, uint32_t buf_type)
{
	vector<PixelFormat> v;

	v4l2_fmtdesc desc{};
	desc.type = buf_type;

	while (ioctl(fd, VIDIOC_ENUM_FMT, &desc) == 0) {
		if (desc.pixelformat == V4L2_PIX_FMT_NV12M)
			v.push_back(PixelFormat::NV12);
		else if (desc.pixelformat != V4L2_PIX_FMT_NV12)
			v.push_back((PixelFormat)desc.pixelformat);

		desc.index++;
	}

	return v;
}

static int v4l2_get_format(int fd, uint32_t buf_type, PixelFormat& fmt, uint32_t& width, uint32_t& height)
{
	int r;

	v4l2_format v4lfmt{};

	v4lfmt.type = buf_type;
	r = ioctl(fd, VIDIOC_G_FMT, &v4lfmt);
	ASSERT(r == 0);

	bool mplane = buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE || buf_type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;

	FAIL_IF(mplane, "mplane not supported");

	fmt = (PixelFormat)v4lfmt.fmt.pix.pixelformat;
	width = v4lfmt.fmt.pix.width;
	height = v4lfmt.fmt.pix.height;

	return 0;
}

static void v4l2_set_format(int fd, PixelFormat fmt, uint32_t width, uint32_t height, uint32_t buf_type)
{
	int r;

	v4l2_format v4lfmt{};

	v4lfmt.type = buf_type;
	r = ioctl(fd, VIDIOC_G_FMT, &v4lfmt);
	ASSERT(r == 0);

	const PixelFormatInfo& pfi = get_pixel_format_info(fmt);

	bool mplane = buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE || buf_type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;

	if (mplane) {
		v4l2_pix_format_mplane& mp = v4lfmt.fmt.pix_mp;
		uint32_t used_fmt;

		if (fmt == PixelFormat::NV12)
			used_fmt = V4L2_PIX_FMT_NV12M;
		else
			used_fmt = (uint32_t)fmt;

		mp.pixelformat = used_fmt;
		mp.width = width;
		mp.height = height;

		mp.num_planes = pfi.num_planes;

		for (unsigned i = 0; i < pfi.num_planes; ++i) {
			const PixelFormatPlaneInfo& pfpi = pfi.planes[i];
			v4l2_plane_pix_format& p = mp.plane_fmt[i];

			p.bytesperline = width * pfpi.bitspp / 8;
			p.sizeimage = p.bytesperline * height / pfpi.ysub;
		}

		r = ioctl(fd, VIDIOC_S_FMT, &v4lfmt);
		ASSERT(r == 0);

		ASSERT(mp.pixelformat == used_fmt);
		ASSERT(mp.width == width);
		ASSERT(mp.height == height);

		ASSERT(mp.num_planes == pfi.num_planes);

		for (unsigned i = 0; i < pfi.num_planes; ++i) {
			const PixelFormatPlaneInfo& pfpi = pfi.planes[i];
			v4l2_plane_pix_format& p = mp.plane_fmt[i];

			ASSERT(p.bytesperline == width * pfpi.bitspp / 8);
			ASSERT(p.sizeimage == p.bytesperline * height / pfpi.ysub);
		}
	} else {
		ASSERT(pfi.num_planes == 1);

		v4lfmt.fmt.pix.pixelformat = (uint32_t)fmt;
		v4lfmt.fmt.pix.width = width;
		v4lfmt.fmt.pix.height = height;
		v4lfmt.fmt.pix.bytesperline = width * pfi.planes[0].bitspp / 8;
		v4lfmt.fmt.pix.field = V4L2_FIELD_NONE;

		r = ioctl(fd, VIDIOC_S_FMT, &v4lfmt);
		ASSERT(r == 0);

		ASSERT(v4lfmt.fmt.pix.pixelformat == (uint32_t)fmt);
		ASSERT(v4lfmt.fmt.pix.width == width);
		ASSERT(v4lfmt.fmt.pix.height == height);
		ASSERT(v4lfmt.fmt.pix.bytesperline == width * pfi.planes[0].bitspp / 8);
	}
}

static void v4l2_get_selection(int fd, uint32_t& left, uint32_t& top, uint32_t& width, uint32_t& height, uint32_t buf_type)
{
	int r;
	struct v4l2_selection selection;

	if (buf_type == V4L2_BUF_TYPE_VIDEO_OUTPUT ||
	    buf_type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		selection.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		selection.target = V4L2_SEL_TGT_CROP;
	} else if (buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE ||
		   buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		selection.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		selection.target = V4L2_SEL_TGT_COMPOSE;
	} else {
		FAIL("buf_type (%d) is not valid\n", buf_type);
	}

	r = ioctl(fd, VIDIOC_G_SELECTION, &selection);
	ASSERT(r == 0);

	left = selection.r.left;
	top = selection.r.top;
	width = selection.r.width;
	height = selection.r.height;
}

static void v4l2_set_selection(int fd, uint32_t& left, uint32_t& top, uint32_t& width, uint32_t& height, uint32_t buf_type)
{
	int r;
	struct v4l2_selection selection;

	if (buf_type == V4L2_BUF_TYPE_VIDEO_OUTPUT ||
	    buf_type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		selection.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		selection.target = V4L2_SEL_TGT_CROP;
	} else if (buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE ||
		   buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		selection.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		selection.target = V4L2_SEL_TGT_COMPOSE;
	} else {
		FAIL("buf_type (%d) is not valid\n", buf_type);
	}

	selection.r.left = left;
	selection.r.top = top;
	selection.r.width = width;
	selection.r.height = height;

	r = ioctl(fd, VIDIOC_S_SELECTION, &selection);
	ASSERT(r == 0);

	left = selection.r.left;
	top = selection.r.top;
	width = selection.r.width;
	height = selection.r.height;
}

static void v4l2_request_bufs(int fd, uint32_t queue_size, uint32_t buf_type, uint32_t mem_type)
{
	v4l2_requestbuffers v4lreqbuf{};
	v4lreqbuf.type = buf_type;
	v4lreqbuf.memory = mem_type;
	v4lreqbuf.count = queue_size;
	int r = ioctl(fd, VIDIOC_REQBUFS, &v4lreqbuf);
	FAIL_IF(r != 0, "VIDIOC_REQBUFS failed: %d", errno);
	ASSERT(v4lreqbuf.count == queue_size);
}

static void v4l2_queue(int fd, VideoBuffer& fb, uint32_t buf_type)
{
	v4l2_buffer buf{};
	buf.type = buf_type;
	buf.memory = get_mem_type(fb.m_mem_type);
	buf.index = fb.m_index;

	bool mplane = buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE || buf_type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;

	if (mplane) {
		ASSERT(false);
		/*
		const PixelFormatInfo& pfi = get_pixel_format_info(fb->m_format);

		buf.length = pfi.num_planes;

		v4l2_plane planes[4]{};
		buf.m.planes = planes;

		for (unsigned i = 0; i < pfi.num_planes; ++i) {
			planes[i].m.fd = fb->prime_fd(i);
			planes[i].bytesused = fb->size(i);
			planes[i].length = fb->size(i);
		}

		int r = ioctl(fd, VIDIOC_QBUF, &buf);
		ASSERT(r == 0);
	*/
	} else {
		if (fb.m_mem_type == VideoMemoryType::DMABUF)
			buf.m.fd = fb.m_fd;

		int r = ioctl(fd, VIDIOC_QBUF, &buf);
		ASSERT(r == 0);
	}
}

static uint32_t v4l2_dequeue(int fd, VideoBuffer& fb, uint32_t buf_type)
{
	v4l2_buffer buf{};
	buf.type = buf_type;
	buf.memory = get_mem_type(fb.m_mem_type);

	// V4L2 crashes if planes are not set
	v4l2_plane planes[4]{};
	buf.m.planes = planes;
	buf.length = 4;

	int r = ioctl(fd, VIDIOC_DQBUF, &buf);
	if (r)
		__throw_exception_again system_error(errno, generic_category());

	fb.m_index = buf.index;
	fb.m_length = buf.length;

	if (fb.m_mem_type == VideoMemoryType::DMABUF)
		fb.m_fd = buf.m.fd;
	else
		fb.m_offset = buf.m.offset;

	return buf.index;
}

VideoDevice::VideoDevice(const string& dev)
	: VideoDevice(::open(dev.c_str(), O_RDWR | O_NONBLOCK))
{
}

VideoDevice::VideoDevice(int fd)
	: m_fd(fd)
{
	if (fd < 0)
		__throw_exception_again runtime_error("bad fd");

	struct v4l2_capability cap = {};
	int r = ioctl(fd, VIDIOC_QUERYCAP, &cap);
	ASSERT(r == 0);

	if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE) {
		m_has_capture = true;
		m_has_mplane_capture = true;
	} else if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) {
		m_has_capture = true;
		m_has_mplane_capture = false;
	}

	if (cap.capabilities & V4L2_CAP_VIDEO_OUTPUT_MPLANE) {
		m_has_output = true;
		m_has_mplane_output = true;
	} else if (cap.capabilities & V4L2_CAP_VIDEO_OUTPUT) {
		m_has_output = true;
		m_has_mplane_output = false;
	}

	if (cap.capabilities & V4L2_CAP_VIDEO_M2M_MPLANE) {
		m_has_m2m = true;
		m_has_capture = true;
		m_has_output = true;
		m_has_mplane_m2m = true;
		m_has_mplane_capture = true;
		m_has_mplane_output = true;
	} else if (cap.capabilities & V4L2_CAP_VIDEO_M2M) {
		m_has_m2m = true;
		m_has_capture = true;
		m_has_output = true;
		m_has_mplane_m2m = false;
		m_has_mplane_capture = false;
		m_has_mplane_output = false;
	}

	if (cap.capabilities & V4L2_CAP_META_CAPTURE) {
		m_has_meta_capture = true;
	}
}

VideoDevice::~VideoDevice()
{
	::close(m_fd);
}

VideoStreamer* VideoDevice::get_capture_streamer()
{
	ASSERT(m_has_capture);

	if (!m_capture_streamer) {
		auto type = m_has_mplane_capture ? VideoStreamer::StreamerType::CaptureMulti : VideoStreamer::StreamerType::CaptureSingle;
		m_capture_streamer = std::unique_ptr<VideoStreamer>(new VideoStreamer(m_fd, type));
	}

	return m_capture_streamer.get();
}

VideoStreamer* VideoDevice::get_output_streamer()
{
	ASSERT(m_has_output);

	if (!m_output_streamer) {
		auto type = m_has_mplane_output ? VideoStreamer::StreamerType::OutputMulti : VideoStreamer::StreamerType::OutputSingle;
		m_output_streamer = std::unique_ptr<VideoStreamer>(new VideoStreamer(m_fd, type));
	}

	return m_output_streamer.get();
}

MetaStreamer* VideoDevice::get_meta_capture_streamer()
{
	ASSERT(m_has_meta_capture);

	if (!m_meta_capture_streamer)
		m_meta_capture_streamer = make_unique<MetaStreamer>(m_fd, MetaStreamer::StreamerType::CaptureMeta);

	return m_meta_capture_streamer.get();
}

vector<tuple<uint32_t, uint32_t>> VideoDevice::get_discrete_frame_sizes(PixelFormat fmt)
{
	vector<tuple<uint32_t, uint32_t>> v;

	v4l2_frmsizeenum v4lfrms{};
	v4lfrms.pixel_format = (uint32_t)fmt;

	int r = ioctl(m_fd, VIDIOC_ENUM_FRAMESIZES, &v4lfrms);
	ASSERT(r);

	FAIL_IF(v4lfrms.type != V4L2_FRMSIZE_TYPE_DISCRETE, "No discrete frame sizes");

	while (ioctl(m_fd, VIDIOC_ENUM_FRAMESIZES, &v4lfrms) == 0) {
		v.emplace_back(v4lfrms.discrete.width, v4lfrms.discrete.height);
		v4lfrms.index++;
	};

	return v;
}

VideoDevice::VideoFrameSize VideoDevice::get_frame_sizes(PixelFormat fmt)
{
	v4l2_frmsizeenum v4lfrms{};
	v4lfrms.pixel_format = (uint32_t)fmt;

	int r = ioctl(m_fd, VIDIOC_ENUM_FRAMESIZES, &v4lfrms);
	ASSERT(r);

	FAIL_IF(v4lfrms.type == V4L2_FRMSIZE_TYPE_DISCRETE, "No continuous frame sizes");

	VideoFrameSize s;

	s.min_w = v4lfrms.stepwise.min_width;
	s.max_w = v4lfrms.stepwise.max_width;
	s.step_w = v4lfrms.stepwise.step_width;

	s.min_h = v4lfrms.stepwise.min_height;
	s.max_h = v4lfrms.stepwise.max_height;
	s.step_h = v4lfrms.stepwise.step_height;

	return s;
}

vector<string> VideoDevice::get_capture_devices()
{
	vector<string> v;

	for (int i = 0; i < 20; ++i) {
		string name = "/dev/video" + to_string(i);

		struct stat buffer;
		if (stat(name.c_str(), &buffer) != 0)
			continue;

		__try {
			VideoDevice vid(name);

			if (vid.has_capture() && !vid.has_m2m())
				v.push_back(name);
		} __catch (...) {
		}
	}

	return v;
}

vector<string> VideoDevice::get_m2m_devices()
{
	vector<string> v;

	for (int i = 0; i < 20; ++i) {
		string name = "/dev/video" + to_string(i);

		struct stat buffer;
		if (stat(name.c_str(), &buffer) != 0)
			continue;

		__try {
			VideoDevice vid(name);

			if (vid.has_m2m())
				v.push_back(name);
		} __catch (...) {
		}
	}

	return v;
}

VideoStreamer::VideoStreamer(int fd, StreamerType type)
	: m_fd(fd), m_type(type)
{
}

std::vector<string> VideoStreamer::get_ports()
{
	vector<string> v;

	switch (m_type) {
	case StreamerType::CaptureSingle:
	case StreamerType::CaptureMulti: {
		struct v4l2_input input {
		};

		while (ioctl(m_fd, VIDIOC_ENUMINPUT, &input) == 0) {
			v.push_back(string((char*)&input.name));
			input.index++;
		}

		break;
	}

	case StreamerType::OutputSingle:
	case StreamerType::OutputMulti: {
		struct v4l2_output output {
		};

		while (ioctl(m_fd, VIDIOC_ENUMOUTPUT, &output) == 0) {
			v.push_back(string((char*)&output.name));
			output.index++;
		}

		break;
	}

	default:
		FAIL("Bad StreamerType");
	}

	return v;
}

void VideoStreamer::set_port(uint32_t index)
{
	unsigned long req;

	switch (m_type) {
	case StreamerType::CaptureSingle:
	case StreamerType::CaptureMulti:
		req = VIDIOC_S_INPUT;
		break;

	case StreamerType::OutputSingle:
	case StreamerType::OutputMulti:
		req = VIDIOC_S_OUTPUT;
		break;

	default:
		FAIL("Bad StreamerType");
	}

	int r = ioctl(m_fd, req, &index);
	ASSERT(r == 0);
}

static v4l2_buf_type get_buf_type(VideoStreamer::StreamerType type)
{
	switch (type) {
	case VideoStreamer::StreamerType::CaptureSingle:
		return V4L2_BUF_TYPE_VIDEO_CAPTURE;
	case VideoStreamer::StreamerType::CaptureMulti:
		return V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	case VideoStreamer::StreamerType::OutputSingle:
		return V4L2_BUF_TYPE_VIDEO_OUTPUT;
	case VideoStreamer::StreamerType::OutputMulti:
		return V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	case MetaStreamer::StreamerType::CaptureMeta:
		return V4L2_BUF_TYPE_META_CAPTURE;
	case MetaStreamer::StreamerType::OutputMeta:
		return (v4l2_buf_type)14; // XXX V4L2_BUF_TYPE_META_OUTPUT;
	default:
		FAIL("Bad StreamerType");
	}
}

std::vector<PixelFormat> VideoStreamer::get_formats()
{
	return v4l2_get_formats(m_fd, get_buf_type(m_type));
}

int VideoStreamer::get_format(PixelFormat &fmt, uint32_t &width, uint32_t &height)
{
	return v4l2_get_format(m_fd, get_buf_type(m_type), fmt, width, height);
}

void VideoStreamer::set_format(PixelFormat fmt, uint32_t width, uint32_t height)
{
	v4l2_set_format(m_fd, fmt, width, height, get_buf_type(m_type));
}

void VideoStreamer::get_selection(uint32_t& left, uint32_t& top, uint32_t& width, uint32_t& height)
{
	v4l2_get_selection(m_fd, left, top, width, height, get_buf_type(m_type));
}

void VideoStreamer::set_selection(uint32_t& left, uint32_t& top, uint32_t& width, uint32_t& height)
{
	v4l2_set_selection(m_fd, left, top, width, height, get_buf_type(m_type));
}

void VideoStreamer::set_queue_size(uint32_t queue_size, VideoMemoryType mem_type)
{
	m_mem_type = mem_type;

	v4l2_request_bufs(m_fd, queue_size, get_buf_type(m_type), get_mem_type(m_mem_type));

	m_fbs.resize(queue_size);
}

void VideoStreamer::queue(VideoBuffer &fb)
{
	uint32_t idx;

	for (idx = 0; idx < m_fbs.size(); ++idx) {
		if (m_fbs[idx] == false)
			break;
	}

	FAIL_IF(idx == m_fbs.size(), "queue full");

	fb.m_index = idx;

	m_fbs[idx] = true;

	v4l2_queue(m_fd, fb, get_buf_type(m_type));
}

VideoBuffer VideoStreamer::dequeue()
{
	VideoBuffer fb {};
	fb.m_mem_type = m_mem_type;

	uint32_t idx = v4l2_dequeue(m_fd, fb, get_buf_type(m_type));

	m_fbs[idx] = false;

	return fb;
}

void VideoStreamer::stream_on()
{
	uint32_t buf_type = get_buf_type(m_type);
	int r = ioctl(m_fd, VIDIOC_STREAMON, &buf_type);
	FAIL_IF(r, "Failed to enable stream: %d", r);
}

void VideoStreamer::stream_off()
{
	uint32_t buf_type = get_buf_type(m_type);
	int r = ioctl(m_fd, VIDIOC_STREAMOFF, &buf_type);
	FAIL_IF(r, "Failed to disable stream: %d", r);
}





MetaStreamer::MetaStreamer(int fd, StreamerType type)
	: VideoStreamer(fd, type)
{
}

void MetaStreamer::set_format(PixelFormat fmt, uint32_t size)
{
	int r;

	v4l2_format v4lfmt {};

	v4lfmt.type = get_buf_type(m_type);
	//r = ioctl(m_fd, VIDIOC_G_FMT, &v4lfmt);
	//ASSERT(r == 0);

	v4lfmt.fmt.meta.dataformat = (uint32_t)fmt;
	v4lfmt.fmt.meta.buffersize = size;

	r = ioctl(m_fd, VIDIOC_S_FMT, &v4lfmt);
	ASSERT(r == 0);
}
