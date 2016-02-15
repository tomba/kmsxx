#include <cstdio>
#include <algorithm>
#include <chrono>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

#include "kms++.h"

#include "test.h"

using namespace std;
using namespace kms;

static void main_loop(Card& card);

class Flipper
{
public:
	Flipper(Card& card, unsigned width, unsigned height)
		: m_current(0), m_bar_xpos(0)
	{
		auto format = PixelFormat::XRGB8888;
		m_fbs[0] = new DumbFramebuffer(card, width, height, format);
		m_fbs[1] = new DumbFramebuffer(card, width, height, format);
	}

	~Flipper()
	{
		delete m_fbs[0];
		delete m_fbs[1];
	}

	Framebuffer* get_next()
	{
		m_current ^= 1;

		const int bar_width = 20;
		const int bar_speed = 8;

		auto fb = m_fbs[m_current];

		int current_xpos = m_bar_xpos;
		int old_xpos = (current_xpos + (fb->width() - bar_width - bar_speed)) % (fb->width() - bar_width);
		int new_xpos = (current_xpos + bar_speed) % (fb->width() - bar_width);

		draw_color_bar(*fb, old_xpos, new_xpos, bar_width);

		m_bar_xpos = new_xpos;

		return fb;
	}

private:
	DumbFramebuffer* m_fbs[2];

	int m_current;
	int m_bar_xpos;
};

class OutputFlipHandler : private PageFlipHandlerBase
{
public:
	OutputFlipHandler(Connector* conn, Crtc* crtc, const Videomode& mode)
		: m_connector(conn), m_crtc(crtc), m_mode(mode),
		  m_flipper(conn->card(), mode.hdisplay, mode.vdisplay),
		  m_plane(0), m_plane_flipper(0)
	{
	}

	OutputFlipHandler(Connector* conn, Crtc* crtc, const Videomode& mode,
			  Plane* plane, unsigned pwidth, unsigned pheight)
		: m_connector(conn), m_crtc(crtc), m_mode(mode),
		  m_flipper(conn->card(), mode.hdisplay, mode.vdisplay),
		  m_plane(plane)
	{
		m_plane_flipper = new Flipper(conn->card(), pwidth, pheight);
	}

	~OutputFlipHandler()
	{
		if (m_plane_flipper)
			delete m_plane_flipper;
	}

	OutputFlipHandler(const OutputFlipHandler& other) = delete;
	OutputFlipHandler& operator=(const OutputFlipHandler& other) = delete;

	void set_mode()
	{
		auto fb = m_flipper.get_next();
		int r = m_crtc->set_mode(m_connector, *fb, m_mode);
		ASSERT(r == 0);

		if (m_crtc->card().has_atomic()) {
			Plane* root_plane = 0;
			for (Plane* p : m_crtc->get_possible_planes()) {
				if (p->crtc_id() == m_crtc->id()) {
					root_plane = p;
					break;
				}
			}

			FAIL_IF(!root_plane, "No primary plane for crtc %d", m_crtc->id());

			m_root_plane = root_plane;
		}

		if (m_plane) {
			auto planefb = m_plane_flipper->get_next();
			r = m_crtc->set_plane(m_plane, *planefb,
					      0, 0, planefb->width(), planefb->height(),
					      0, 0, planefb->width(), planefb->height());
			ASSERT(r == 0);
		}
	}

	void start_flipping()
	{
		m_time_last = m_t1 = std::chrono::steady_clock::now();
		m_slowest_frame = std::chrono::duration<float>::min();
		m_frame_num = 0;
		queue_next();
	}

private:
	void handle_page_flip(uint32_t frame, double time)
	{
		++m_frame_num;

		auto now = std::chrono::steady_clock::now();

		std::chrono::duration<float> diff = now - m_time_last;
		if (diff > m_slowest_frame)
			m_slowest_frame = diff;

		if (m_frame_num  % 100 == 0) {
			std::chrono::duration<float> fsec = now - m_t1;
			printf("Output %d: fps %f, slowest %.2f ms\n",
			       m_connector->idx(), 100.0 / fsec.count(),
			       m_slowest_frame.count() * 1000);
			m_t1 = now;
			m_slowest_frame = std::chrono::duration<float>::min();
		}

		m_time_last = now;

		queue_next();
	}

	void queue_next()
	{
		auto crtc = m_crtc;
		auto& card = crtc->card();

		auto fb = m_flipper.get_next();
		Framebuffer* planefb = m_plane ? m_plane_flipper->get_next() : 0;

		if (card.has_atomic()) {
			int r;

			AtomicReq req(card);

			req.add(m_root_plane, "FB_ID", fb->id());
			if (m_plane)
				req.add(m_plane, "FB_ID", planefb->id());

			r = req.test();
			ASSERT(r == 0);

			r = req.commit(this);
			ASSERT(r == 0);
		} else {
			int r = crtc->page_flip(*fb, this);
			ASSERT(r == 0);

			if (m_plane) {
				r = m_crtc->set_plane(m_plane, *planefb,
						      0, 0, planefb->width(), planefb->height(),
						      0, 0, planefb->width(), planefb->height());
				ASSERT(r == 0);
			}
		}
	}

private:
	Connector* m_connector;
	Crtc* m_crtc;
	Videomode m_mode;
	Plane* m_root_plane;

	int m_frame_num;
	chrono::steady_clock::time_point m_t1;
	chrono::steady_clock::time_point m_time_last;
	chrono::duration<float> m_slowest_frame;

	Flipper m_flipper;

	Plane* m_plane;
	Flipper* m_plane_flipper;
};

int main()
{
	Card card;

	if (card.master() == false)
		printf("Not DRM master, modeset may fail\n");

	vector<OutputFlipHandler*> outputs;

	for (auto pipe : card.get_connected_pipelines())
	{
		auto conn = pipe.connector;
		auto crtc = pipe.crtc;
		auto mode = conn->get_default_mode();


		Plane* plane = 0;
#if 0 // disable the plane for now
		for (Plane* p : crtc->get_possible_planes()) {
			if (p->plane_type() == PlaneType::Overlay) {
				plane = p;
				break;
			}
		}
#endif
		OutputFlipHandler* output;
		if (plane)
			output = new OutputFlipHandler(conn, crtc, mode, plane, 500, 400);
		else
			output = new OutputFlipHandler(conn, crtc, mode);
		outputs.push_back(output);
	}

	for(auto out : outputs)
		out->set_mode();

	for(auto out : outputs)
		out->start_flipping();

	main_loop(card);

	for(auto out : outputs)
		delete out;
}

static void main_loop(Card& card)
{
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
			card.call_page_flip_handlers();
		}
	}
}
