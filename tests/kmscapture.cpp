#include <linux/videodev2.h>
#include <cstdio>
#include <string.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <fstream>
#include <sys/ioctl.h>
#include <xf86drm.h>

#include "kms++.h"
#include "test.h"
#include "opts.h"

#define CAMERA_BUF_QUEUE_SIZE	3
#define MAX_CAMERA		9

using namespace std;
using namespace kms;

enum class BufferProvider {
	DRM,
	V4L2,
};

class Camera
{
public:
	Camera(int camera_id, Card& card, Plane* plane, uint32_t x, uint32_t y,
	       uint32_t iw, uint32_t ih, PixelFormat pixfmt,
	       BufferProvider buffer_provider);
	~Camera();

	Camera(const Camera& other) = delete;
	Camera& operator=(const Camera& other) = delete;

	void show_next_frame(Crtc* crtc);
	int fd() const { return m_fd; }
private:
	ExtFramebuffer* GetExtFrameBuffer(Card& card, uint32_t i, PixelFormat pixfmt);
	int m_fd;	/* camera file descriptor */
	Plane* m_plane;
	BufferProvider m_buffer_provider;
	vector<DumbFramebuffer*> m_fb; /* framebuffers for DRM buffers */
	vector<ExtFramebuffer*> m_extfb; /* framebuffers for V4L2 buffers */
	int m_prev_fb_index;
	uint32_t m_in_width, m_in_height; /* camera capture resolution */
	/* image properties for display */
	uint32_t m_out_width, m_out_height;
	uint32_t m_out_x, m_out_y;
};

static int buffer_export(int v4lfd, enum v4l2_buf_type bt, uint32_t index, int *dmafd)
{
	struct v4l2_exportbuffer expbuf;

	memset(&expbuf, 0, sizeof(expbuf));
	expbuf.type = bt;
	expbuf.index = index;
	if (ioctl(v4lfd, VIDIOC_EXPBUF, &expbuf) == -1) {
		perror("VIDIOC_EXPBUF");
		return -1;
	}

	*dmafd = expbuf.fd;

	return 0;
}

ExtFramebuffer* Camera::GetExtFrameBuffer(Card& card, uint32_t i, PixelFormat pixfmt)
{
	int r, dmafd;

	r = buffer_export(m_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, i, &dmafd);
	ASSERT(r == 0);

	uint32_t handle;
	r = drmPrimeFDToHandle(card.fd(), dmafd, &handle);
	ASSERT(r == 0);

	const PixelFormatInfo& format_info = get_pixel_format_info(pixfmt);
	ASSERT(format_info.num_planes == 1);

	uint32_t handles[4] { handle };
	uint32_t pitches[4] { m_in_width * (format_info.planes[0].bitspp / 8) };
	uint32_t offsets[4] { };

	return new ExtFramebuffer(card, m_in_width, m_in_height, pixfmt,
				  handles, pitches, offsets);
}

bool inline better_size(struct v4l2_frmsize_discrete* v4ldisc,
			uint32_t iw, uint32_t ih,
			uint32_t best_w, uint32_t best_h)
{
	if (v4ldisc->width <= iw && v4ldisc->height <= ih &&
	    (v4ldisc->width >= best_w || v4ldisc->height >= best_h))
		return true;

	return false;
}

Camera::Camera(int camera_id, Card& card, Plane* plane, uint32_t x, uint32_t y,
	       uint32_t iw, uint32_t ih, PixelFormat pixfmt,
	       BufferProvider buffer_provider)
{
	char dev_name[20];
	int r;
	uint32_t best_w = 320;
	uint32_t best_h = 240;
	uint32_t v4l_mem;

	m_buffer_provider = buffer_provider;
	if (m_buffer_provider == BufferProvider::V4L2)
		v4l_mem = V4L2_MEMORY_MMAP;
	else
		v4l_mem = V4L2_MEMORY_DMABUF;

	sprintf(dev_name, "/dev/video%d", camera_id);
	m_fd = ::open(dev_name, O_RDWR | O_NONBLOCK);

	ASSERT(m_fd >= 0);

	struct v4l2_frmsizeenum v4lfrms = { };
	v4lfrms.pixel_format = (uint32_t)pixfmt;
	while (ioctl(m_fd, VIDIOC_ENUM_FRAMESIZES, &v4lfrms) == 0) {
		if (v4lfrms.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
			if (better_size(&v4lfrms.discrete, iw, ih,
					best_w, best_h)) {
				best_w = v4lfrms.discrete.width;
				best_h = v4lfrms.discrete.height;
			}
		} else {
			break;
		}
		v4lfrms.index++;
	};

	m_out_width = m_in_width = best_w;
	m_out_height = m_in_height = best_h;
	/* Move it to the middle of the requested area */
	m_out_x = x + iw / 2 - m_out_width / 2;
	m_out_y = y + ih / 2 - m_out_height / 2;

	struct v4l2_format v4lfmt = { };
	v4lfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	r = ioctl(m_fd, VIDIOC_G_FMT, &v4lfmt);
	ASSERT(r == 0);

	v4lfmt.fmt.pix.pixelformat = (uint32_t)pixfmt;
	v4lfmt.fmt.pix.width = m_in_width;
	v4lfmt.fmt.pix.height = m_in_height;

	r = ioctl(m_fd, VIDIOC_S_FMT, &v4lfmt);
	ASSERT(r == 0);

	struct v4l2_requestbuffers v4lreqbuf = { };
	v4lreqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	v4lreqbuf.memory = v4l_mem;
	v4lreqbuf.count = CAMERA_BUF_QUEUE_SIZE;
	r = ioctl(m_fd, VIDIOC_REQBUFS, &v4lreqbuf);
	ASSERT(r == 0);
	ASSERT(v4lreqbuf.count == CAMERA_BUF_QUEUE_SIZE);

	struct v4l2_buffer v4lbuf = { };
	v4lbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	v4lbuf.memory = v4l_mem;

	for (unsigned i = 0; i < CAMERA_BUF_QUEUE_SIZE; i++) {
		DumbFramebuffer *fb = NULL;
		ExtFramebuffer *extfb = NULL;

		if (m_buffer_provider == BufferProvider::V4L2)
			extfb = GetExtFrameBuffer(card, i, pixfmt);
		else
			fb = new DumbFramebuffer(card, m_in_width,
						 m_in_height, pixfmt);

		v4lbuf.index = i;
		if (m_buffer_provider == BufferProvider::DRM)
			v4lbuf.m.fd = fb->prime_fd(0);
		r = ioctl(m_fd, VIDIOC_QBUF, &v4lbuf);
		ASSERT(r == 0);

		if (m_buffer_provider == BufferProvider::V4L2)
			m_extfb.push_back(extfb);
		else
			m_fb.push_back(fb);
	}
	m_prev_fb_index = -1;
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	r = ioctl(m_fd, VIDIOC_STREAMON, &type);
		ASSERT(r == 0);

	m_plane = plane;
}


Camera::~Camera()
{
	for (unsigned i = 0; i < m_fb.size(); i++)
		delete m_fb[i];

	for (unsigned i = 0; i < m_extfb.size(); i++)
		delete m_extfb[i];

	::close(m_fd);
}

void Camera::show_next_frame(Crtc* crtc)
{
	int r;
	uint32_t v4l_mem;

	if (m_buffer_provider == BufferProvider::V4L2)
		v4l_mem = V4L2_MEMORY_MMAP;
	else
		v4l_mem = V4L2_MEMORY_DMABUF;

	struct v4l2_buffer v4l2buf = { };
	v4l2buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	v4l2buf.memory = v4l_mem;
	r = ioctl(m_fd, VIDIOC_DQBUF, &v4l2buf);
	if (r != 0) {
		printf("VIDIOC_DQBUF ioctl failed with %d\n", errno);
		return;
	}

	unsigned fb_index = v4l2buf.index;
	if (m_buffer_provider == BufferProvider::V4L2)
		r = crtc->set_plane(m_plane, *m_extfb[fb_index],
				    m_out_x, m_out_y, m_out_width, m_out_height,
				    0, 0, m_in_width, m_in_height);
	else
		r = crtc->set_plane(m_plane, *m_fb[fb_index],
				    m_out_x, m_out_y, m_out_width, m_out_height,
				    0, 0, m_in_width, m_in_height);

	ASSERT(r == 0);

	if (m_prev_fb_index >= 0) {
		memset(&v4l2buf, 0, sizeof(v4l2buf));
		v4l2buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		v4l2buf.memory = v4l_mem;
		v4l2buf.index = m_prev_fb_index;
		if (m_buffer_provider == BufferProvider::DRM)
			v4l2buf.m.fd = m_fb[m_prev_fb_index]->prime_fd(0);
		r = ioctl(m_fd, VIDIOC_QBUF, &v4l2buf);
		ASSERT(r == 0);

	}
	m_prev_fb_index = fb_index;
}


static bool is_capture_dev(int fd)
{
	struct v4l2_capability cap = { };
	int r;

	r = ioctl(fd, VIDIOC_QUERYCAP, &cap);
	ASSERT(r == 0);
	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
		return false;

	return true;
}

static vector<int> count_cameras()
{
	int i, fd;
	vector<int> camera_idx;
	char dev_name[20];

	for (i = 0; i < MAX_CAMERA; i++) {
		sprintf(dev_name, "/dev/video%d", i);
		fd = ::open(dev_name, O_RDWR | O_NONBLOCK);
		if (fd >= 0) {
			if (is_capture_dev(fd))
				camera_idx.push_back(i);
			close(fd);
		}
	}

	return camera_idx;
}

static const char* usage_str =
"Usage: kmscapture [OPTIONS]\n\n"
"Options:\n"
"  -s, --single                Single camera mode. Open only /dev/video0\n"
"      --buffer-type=<drm|v4l> Use DRM or V4L provided buffers. Default: DRM\n"
"  -h, --help                  Print this help\n"
;

int main(int argc, char** argv)
{
	uint32_t w;
	BufferProvider buffer_provider = BufferProvider::DRM;

	auto camera_idx = count_cameras();
	unsigned nr_cameras = camera_idx.size();

	FAIL_IF(!nr_cameras, "Not a single camera has been found.");

	OptionSet optionset = {
		Option("s|single", [&](string s)
		{
			nr_cameras = 1;
		}),
		Option("|buffer-type=", [&](string s)
		{
			if (!s.compare("v4l"))
				buffer_provider = BufferProvider::V4L2;
			else if (s.compare("drm"))
				printf("invalid buffer-type: %s\n", s.c_str());
		}),
		Option("h|help", [&]()
		{
			puts(usage_str);
			exit(-1);
		}),
	};

	optionset.parse(argc, argv);

	if (optionset.params().size() > 0) {
		puts(usage_str);
		exit(-1);
	}

	auto pixfmt = FourCCToPixelFormat("YUYV");

	Card card;

	auto conn = card.get_first_connected_connector();
	auto crtc = conn->get_current_crtc();
	printf("Display: %dx%d\n", crtc->width(), crtc->height());
	printf("Buffer provider: %s\n",
	       buffer_provider == BufferProvider::V4L2? "V4L" : "DRM");

	w = crtc->width() / nr_cameras;
	vector<Camera*> cameras;

	unsigned cam_idx = 0;
	for (Plane* p : crtc->get_possible_planes()) {
		if (p->plane_type() != PlaneType::Overlay)
			continue;

		if (!p->supports_format(pixfmt))
			continue;

		auto cam = new Camera(camera_idx[cam_idx], card, p, cam_idx * w, 0,
				      w, crtc->height(), pixfmt, buffer_provider);
		cameras.push_back(cam);
		if (++cam_idx == nr_cameras)
			break;
	}

	FAIL_IF(cam_idx < nr_cameras, "available plane not found");

	vector<pollfd> fds(nr_cameras + 1);

	for (unsigned i = 0; i < nr_cameras; i++) {
		fds[i].fd = cameras[i]->fd();
		fds[i].events =  POLLIN;
	}
	fds[nr_cameras].fd = 0;
	fds[nr_cameras].events =  POLLIN;

	while (true) {
		int r = poll(fds.data(), nr_cameras + 1, -1);
		ASSERT(r > 0);

		if (fds[nr_cameras].revents != 0)
			break;

		for (unsigned i = 0; i < nr_cameras; i++) {
			if (!fds[i].revents)
				continue;
			cameras[i]->show_next_frame(crtc);
			fds[i].revents = 0;
		}
	}

	for (auto cam : cameras)
		delete cam;
}
