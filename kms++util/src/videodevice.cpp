#include <string>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <system_error>

#include <kms++/kms++.h>
#include <kms++util/kms++util.h>
#include <kms++util/videodevice.h>

using namespace std;
using namespace kms;

/* V4L2 helper funcs */
static vector<PixelFormat> v4l2_get_formats(int fd, uint32_t buf_type)
{
	vector<PixelFormat> v;

	v4l2_fmtdesc desc { };
	desc.type = buf_type;

	while (ioctl(fd, VIDIOC_ENUM_FMT, &desc) == 0) {
		v.push_back((PixelFormat)desc.pixelformat);
		desc.index++;
	}

	return v;
}

static void v4l2_set_format(int fd, PixelFormat fmt, uint32_t width, uint32_t height, uint32_t buf_type)
{
	int r;

	v4l2_format v4lfmt { };

	v4lfmt.type = buf_type;
	r = ioctl(fd, VIDIOC_G_FMT, &v4lfmt);
	ASSERT(r == 0);

	const PixelFormatInfo& pfi = get_pixel_format_info(fmt);

	bool mplane = buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE || buf_type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;

	if (mplane) {
		v4l2_pix_format_mplane& mp = v4lfmt.fmt.pix_mp;

		mp.pixelformat = (uint32_t)fmt;
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

		ASSERT(mp.pixelformat == (uint32_t)fmt);
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

static void v4l2_request_bufs(int fd, uint32_t queue_size, uint32_t buf_type)
{
	v4l2_requestbuffers v4lreqbuf { };
	v4lreqbuf.type = buf_type;
	v4lreqbuf.memory = V4L2_MEMORY_DMABUF;
	v4lreqbuf.count = queue_size;
	int r = ioctl(fd, VIDIOC_REQBUFS, &v4lreqbuf);
	ASSERT(r == 0);
	ASSERT(v4lreqbuf.count == queue_size);
}

static void v4l2_queue_dmabuf(int fd, uint32_t index, DumbFramebuffer* fb, uint32_t buf_type)
{
	v4l2_buffer buf { };
	buf.type = buf_type;
	buf.memory = V4L2_MEMORY_DMABUF;
	buf.index = index;

	const PixelFormatInfo& pfi = get_pixel_format_info(fb->format());

	bool mplane = buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE || buf_type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;

	if (mplane) {
		buf.length = pfi.num_planes;

		v4l2_plane planes[4] { };
		buf.m.planes = planes;

		for (unsigned i = 0; i < pfi.num_planes; ++i) {
			planes[i].m.fd = fb->prime_fd(i);
			planes[i].bytesused = fb->size(i);
			planes[i].length = fb->size(i);
		}

		int r = ioctl(fd, VIDIOC_QBUF, &buf);
		ASSERT(r == 0);
	} else {
		buf.m.fd = fb->prime_fd(0);

		int r = ioctl(fd, VIDIOC_QBUF, &buf);
		ASSERT(r == 0);
	}
}

static uint32_t v4l2_dequeue(int fd, uint32_t buf_type)
{
	v4l2_buffer buf { };
	buf.type = buf_type;
	buf.memory = V4L2_MEMORY_DMABUF;

	// V4L2 crashes if planes are not set
	v4l2_plane planes[4] { };
	buf.m.planes = planes;
	buf.length = 4;

	int r = ioctl(fd, VIDIOC_DQBUF, &buf);
	if (r)
		throw system_error(errno, generic_category());

	return buf.index;
}




VideoDevice::VideoDevice(const string& dev)
	:VideoDevice(::open(dev.c_str(), O_RDWR | O_NONBLOCK))
{
}

VideoDevice::VideoDevice(int fd)
	: m_fd(fd), m_has_capture(false), m_has_output(false), m_has_m2m(false), m_capture_streamer(0), m_output_streamer(0)
{
	FAIL_IF(fd < 0, "Bad fd");

	struct v4l2_capability cap = { };
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
		m_capture_streamer = new VideoStreamer(m_fd, type);
	}

	return m_capture_streamer;
}

VideoStreamer* VideoDevice::get_output_streamer()
{
	ASSERT(m_has_output);

	if (!m_output_streamer) {
		auto type = m_has_mplane_output ? VideoStreamer::StreamerType::OutputMulti : VideoStreamer::StreamerType::OutputSingle;
		m_output_streamer = new VideoStreamer(m_fd, type);
	}

	return m_output_streamer;
}

vector<tuple<uint32_t, uint32_t>> VideoDevice::get_discrete_frame_sizes(PixelFormat fmt)
{
	vector<tuple<uint32_t, uint32_t>> v;

	v4l2_frmsizeenum v4lfrms { };
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
	v4l2_frmsizeenum v4lfrms { };
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

		VideoDevice vid(name);

		if (vid.has_capture() && !vid.has_m2m())
			v.push_back(name);
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

		VideoDevice vid(name);

		if (vid.has_m2m())
			v.push_back(name);
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
	case StreamerType::CaptureMulti:
	{
		struct v4l2_input input { };

		while (ioctl(m_fd, VIDIOC_ENUMINPUT, &input) == 0) {
			v.push_back(string((char*)&input.name));
			input.index++;
		}

		break;
	}

	case StreamerType::OutputSingle:
	case StreamerType::OutputMulti:
	{
		struct v4l2_output output { };

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
	default:
		FAIL("Bad StreamerType");
	}
}

std::vector<PixelFormat> VideoStreamer::get_formats()
{
	return v4l2_get_formats(m_fd, get_buf_type(m_type));
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

void VideoStreamer::set_queue_size(uint32_t queue_size)
{
	v4l2_request_bufs(m_fd, queue_size, get_buf_type(m_type));
	m_fbs.resize(queue_size);
}

void VideoStreamer::queue(DumbFramebuffer* fb)
{
	uint32_t idx;

	for (idx = 0; idx < m_fbs.size(); ++idx) {
		if (m_fbs[idx] == nullptr)
			break;
	}

	FAIL_IF(idx == m_fbs.size(), "queue full");

	m_fbs[idx] = fb;

	v4l2_queue_dmabuf(m_fd, idx, fb, get_buf_type(m_type));
}

DumbFramebuffer* VideoStreamer::dequeue()
{
	uint32_t idx = v4l2_dequeue(m_fd, get_buf_type(m_type));

	auto fb = m_fbs[idx];
	m_fbs[idx] = nullptr;

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
