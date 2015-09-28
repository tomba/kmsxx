#include <cstdio>
#include <algorithm>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

#include "kms++.h"
#include "utils/color.h"

#include "test.h"

using namespace std;
using namespace kms;

static void draw_color_bar(Framebuffer& buf, int old_xpos, int xpos, int width);

static void main_loop(Card& card);

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

class OutputFlipHandler
{
public:
	OutputFlipHandler(Connector* conn, Crtc* crtc, Framebuffer* fb1, Framebuffer* fb2)
		: m_connector(conn), m_crtc(crtc), m_fbs { fb1, fb2 }, m_front_buf(1), m_bar_xpos(0)
	{
	}

	~OutputFlipHandler()
	{
		delete m_fbs[0];
		delete m_fbs[1];
	}

	OutputFlipHandler(const OutputFlipHandler& other) = delete;
	OutputFlipHandler& operator=(const OutputFlipHandler& other) = delete;

	void set_mode()
	{
		auto mode = m_connector->get_default_mode();
		int r = m_crtc->set_mode(m_connector, *m_fbs[0], mode);
		ASSERT(r == 0);
	}

	void handle_event()
	{
		m_front_buf = (m_front_buf + 1) % 2;

		const int bar_width = 20;
		const int bar_speed = 8;

		auto crtc = m_crtc;
		auto fb = m_fbs[(m_front_buf + 1) % 2];

		ASSERT(crtc);
		ASSERT(fb);

		int current_xpos = m_bar_xpos;
		int old_xpos = (current_xpos + (fb->width() - bar_width - bar_speed)) % (fb->width() - bar_width);
		int new_xpos = (current_xpos + bar_speed) % (fb->width() - bar_width);

		draw_color_bar(*fb, old_xpos, new_xpos, bar_width);

		m_bar_xpos = new_xpos;

		auto& card = crtc->card();

		if (card.has_atomic()) {
			int r;

			AtomicReq ctx(card);

			ctx.add(m_crtc, card.get_prop("FB_ID"), fb->id());

			r = ctx.test();
			ASSERT(r == 0);

			r = ctx.commit(this);
			ASSERT(r == 0);
		} else {
			int r = drmModePageFlip(card.fd(), m_crtc->id(), fb->id(), DRM_MODE_PAGE_FLIP_EVENT, this);
			ASSERT(r == 0);
		}
	}

private:
	Connector* m_connector;
	Crtc* m_crtc;
	Framebuffer* m_fbs[2];

	int m_front_buf;
	int m_bar_xpos;
};

int main()
{
	Card card;

	if (card.master() == false)
		printf("Not DRM master, modeset may fail\n");

	//card.print_short();

	vector<OutputFlipHandler*> outputs;

	for (auto pipe : card.get_connected_pipelines())
	{
		auto conn = pipe.connector;
		auto crtc = pipe.crtc;

		auto mode = conn->get_default_mode();

		auto fb1 = new Framebuffer(card, mode.hdisplay, mode.vdisplay, "XR24");
		auto fb2 = new Framebuffer(card, mode.hdisplay, mode.vdisplay, "XR24");

		printf("conn %u, crtc %u, fb1 %u, fb2 %u\n", conn->id(), crtc->id(), fb1->id(), fb2->id());

		auto output = new OutputFlipHandler(conn, crtc, fb1, fb2);
		outputs.push_back(output);
	}

	for(auto out : outputs)
		out->set_mode();

	for(auto out : outputs)
		out->handle_event();

	main_loop(card);

	for(auto out : outputs)
		delete out;
}

static void page_flip_handler(int fd, unsigned int frame,
			      unsigned int sec, unsigned int usec,
			      void *data)
{
	//printf("flip event %d, %d, %u, %u, %p\n", fd, frame, sec, usec, data);

	auto out = (OutputFlipHandler*)data;

	out->handle_event();
}


static void main_loop(Card& card)
{
	drmEventContext ev = {
		.version = DRM_EVENT_CONTEXT_VERSION,
		.vblank_handler = 0,
		.page_flip_handler = page_flip_handler,
	};

	fd_set fds;

	FD_ZERO(&fds);

	int fd = card.fd();

	printf("press enter to exit\n");

	while (true) {
		int r;

		FD_SET(0, &fds);
		FD_SET(fd, &fds);

		r = select(fd + 1, &fds, NULL, NULL, NULL);
		if (r < 0) {
			fprintf(stderr, "select() failed with %d: %m\n", errno);
			break;
		} else if (FD_ISSET(0, &fds)) {
			fprintf(stderr, "exit due to user-input\n");
			break;
		} else if (FD_ISSET(fd, &fds)) {
			drmHandleEvent(fd, &ev);
		}
	}
}


static const RGB colors32[] = {
	RGB(255, 255, 255),
	RGB(255, 0, 0),
	RGB(255, 255, 255),
	RGB(0, 255, 0),
	RGB(255, 255, 255),
	RGB(0, 0, 255),
	RGB(255, 255, 255),
	RGB(200, 200, 200),
	RGB(255, 255, 255),
	RGB(100, 100, 100),
	RGB(255, 255, 255),
	RGB(50, 50, 50),
};

static const uint16_t colors16[] = {
	colors32[0].rgb565(),
	colors32[1].rgb565(),
	colors32[2].rgb565(),
	colors32[3].rgb565(),
	colors32[4].rgb565(),
	colors32[5].rgb565(),
	colors32[6].rgb565(),
	colors32[7].rgb565(),
	colors32[8].rgb565(),
	colors32[9].rgb565(),
	colors32[10].rgb565(),
	colors32[11].rgb565(),
};

static void drm_draw_color_bar_rgb888(Framebuffer& buf, int old_xpos, int xpos, int width)
{
	for (unsigned y = 0; y < buf.height(); ++y) {
		RGB bcol = colors32[y * ARRAY_SIZE(colors32) / buf.height()];
		uint32_t *line = (uint32_t*)(buf.map(0) + buf.stride(0) * y);

		if (old_xpos >= 0) {
			for (int x = old_xpos; x < old_xpos + width; ++x)
				line[x] = 0;
		}

		for (int x = xpos; x < xpos + width; ++x)
			line[x] = bcol.raw;
	}
}

static void drm_draw_color_bar_rgb565(Framebuffer& buf, int old_xpos, int xpos, int width)
{
	static_assert(ARRAY_SIZE(colors32) == ARRAY_SIZE(colors16), "bad colors arrays");

	for (unsigned y = 0; y < buf.height(); ++y) {
		uint16_t bcol = colors16[y * ARRAY_SIZE(colors16) / buf.height()];
		uint16_t *line = (uint16_t*)(buf.map(0) + buf.stride(0) * y);

		if (old_xpos >= 0) {
			for (int x = old_xpos; x < old_xpos + width; ++x)
				line[x] = 0;
		}

		for (int x = xpos; x < xpos + width; ++x)
			line[x] = bcol;
	}
}

static void drm_draw_color_bar_semiplanar_yuv(Framebuffer& buf, int old_xpos, int xpos, int width)
{
	const uint8_t colors[] = {
		0xff,
		0x00,
		0xff,
		0x20,
		0xff,
		0x40,
		0xff,
		0x80,
		0xff,
	};

	for (unsigned y = 0; y < buf.height(); ++y) {
		unsigned int bcol = colors[y * ARRAY_SIZE(colors) / buf.height()];
		uint8_t *line = (uint8_t*)(buf.map(0) + buf.stride(0) * y);

		if (old_xpos >= 0) {
			for (int x = old_xpos; x < old_xpos + width; ++x)
				line[x] = 0;
		}

		for (int x = xpos; x < xpos + width; ++x)
			line[x] = bcol;
	}
}

static void draw_color_bar(Framebuffer& buf, int old_xpos, int xpos, int width)
{
	switch (buf.format()) {
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
		// XXX not right but gets something on the screen
		drm_draw_color_bar_semiplanar_yuv(buf, old_xpos, xpos, width);
		break;

	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_UYVY:
		// XXX not right but gets something on the screen
		drm_draw_color_bar_rgb565(buf, old_xpos, xpos, width);
		break;

	case DRM_FORMAT_RGB565:
		drm_draw_color_bar_rgb565(buf, old_xpos, xpos, width);
		break;

	case DRM_FORMAT_XRGB8888:
		drm_draw_color_bar_rgb888(buf, old_xpos, xpos, width);
		break;

	default:
		ASSERT(false);
	}
}
