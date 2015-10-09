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

class OutputFlipHandler : private PageFlipHandlerBase
{
public:
	OutputFlipHandler(Connector* conn, Crtc* crtc, DumbFramebuffer* fb1, DumbFramebuffer* fb2)
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

	void start_flipping()
	{
		m_t1 = std::chrono::steady_clock::now();
		m_frame_num = 0;
		queue_next();
	}

private:
	void handle_page_flip(uint32_t frame, double time)
	{
		++m_frame_num;

		if (m_frame_num  % 100 == 0) {
			auto t2 = std::chrono::steady_clock::now();
			std::chrono::duration<float> fsec = t2 - m_t1;
			printf("Output %d: fps %f\n", m_connector->idx(), 100.0 / fsec.count());
			m_t1 = t2;
		}

		queue_next();
	}

	void queue_next()
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

			AtomicReq req(card);

			req.add(m_crtc, "FB_ID", fb->id());

			r = req.test();
			ASSERT(r == 0);

			r = req.commit(this);
			ASSERT(r == 0);
		} else {
			int r = crtc->page_flip(*fb, this);
			ASSERT(r == 0);
		}
	}

private:
	Connector* m_connector;
	Crtc* m_crtc;
	DumbFramebuffer* m_fbs[2];

	int m_front_buf;
	int m_bar_xpos;

	int m_frame_num;
	chrono::steady_clock::time_point m_t1;
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

		auto fb1 = new DumbFramebuffer(card, mode.hdisplay, mode.vdisplay, PixelFormat::XRGB8888);
		auto fb2 = new DumbFramebuffer(card, mode.hdisplay, mode.vdisplay, PixelFormat::XRGB8888);

		printf("conn %u, crtc %u, fb1 %u, fb2 %u\n", conn->id(), crtc->id(), fb1->id(), fb2->id());

		auto output = new OutputFlipHandler(conn, crtc, fb1, fb2);
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
