#include <chrono>
#include <cstdio>
#include <vector>
#include <memory>
#include <algorithm>
#include <poll.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>

#include <kms++/kms++.h>
#include <kms++util/kms++util.h>
#include "kms++gl/kms-egl.h"
#include "kms++gl/kms-gbm.h"
#include "cube-gles2.h"

using namespace kms;
using namespace std;

static int s_flip_pending;
static bool s_need_exit;

static bool s_support_planes;

class OutputHandler : private PageFlipHandlerBase
{
public:
	OutputHandler(Card& card, GbmDevice& gdev, const EglState& egl, Connector* connector, Crtc* crtc, Videomode& mode, Plane* root_plane, Plane* plane, float rotation_mult)
		: m_frame_num(0), m_connector(connector), m_crtc(crtc), m_root_plane(root_plane), m_plane(plane), m_mode(mode),
		  m_rotation_mult(rotation_mult)
	{
		m_surface1 = unique_ptr<GbmEglSurface>(new GbmEglSurface(card, gdev, egl, mode.hdisplay, mode.vdisplay));
		m_scene1 = unique_ptr<GlScene>(new GlScene());
		m_scene1->set_viewport(m_surface1->width(), m_surface1->height());

		if (m_plane) {
			m_surface2 = unique_ptr<GbmEglSurface>(new GbmEglSurface(card, gdev, egl, 400, 400));
			m_scene2 = unique_ptr<GlScene>(new GlScene());
			m_scene2->set_viewport(m_surface2->width(), m_surface2->height());
		}
	}

	OutputHandler(const OutputHandler& other) = delete;
	OutputHandler& operator=(const OutputHandler& other) = delete;

	void setup()
	{
		int ret;

		m_surface1->make_current();
		m_surface1->swap_buffers();
		Framebuffer* fb = m_surface1->lock_next();

		Framebuffer* planefb = 0;

		if (m_plane) {
			m_surface2->make_current();
			m_surface2->swap_buffers();
			planefb = m_surface2->lock_next();
		}

		ret = m_crtc->set_mode(m_connector, *fb, m_mode);
		FAIL_IF(ret, "failed to set mode");

		if (m_plane) {
			ret = m_crtc->set_plane(m_plane, *planefb,
						0, 0, planefb->width(), planefb->height(),
						0, 0, planefb->width(), planefb->height());
			FAIL_IF(ret, "failed to set plane");
		}
	}

	void start_flipping()
	{
		m_t1 = chrono::steady_clock::now();
		queue_next();
	}

private:
	void handle_page_flip(uint32_t frame, double time)
	{
		++m_frame_num;

		if (m_frame_num  % 100 == 0) {
			auto t2 = chrono::steady_clock::now();
			chrono::duration<float> fsec = t2 - m_t1;
			printf("fps: %f\n", 100.0 / fsec.count());
			m_t1 = t2;
		}

		s_flip_pending--;

		m_surface1->free_prev();
		if (m_plane)
			m_surface2->free_prev();

		if (s_need_exit)
			return;

		queue_next();
	}

	void queue_next()
	{
		m_surface1->make_current();
		m_scene1->draw(m_frame_num * m_rotation_mult);
		m_surface1->swap_buffers();
		Framebuffer* fb = m_surface1->lock_next();

		Framebuffer* planefb = 0;

		if (m_plane) {
			m_surface2->make_current();
			m_scene2->draw(m_frame_num * m_rotation_mult * 2);
			m_surface2->swap_buffers();
			planefb = m_surface2->lock_next();
		}

		int r;

		AtomicReq req(m_crtc->card());

		req.add(m_root_plane, "FB_ID", fb->id());
		if (m_plane)
			req.add(m_plane, "FB_ID", planefb->id());

		r = req.test();
		FAIL_IF(r, "atomic test failed");

		r = req.commit(this);
		FAIL_IF(r, "atomic commit failed");

		s_flip_pending++;
	}

	int m_frame_num;
	chrono::steady_clock::time_point m_t1;

	Connector* m_connector;
	Crtc* m_crtc;
	Plane* m_root_plane;
	Plane* m_plane;
	Videomode m_mode;

	unique_ptr<GbmEglSurface> m_surface1;
	unique_ptr<GbmEglSurface> m_surface2;

	unique_ptr<GlScene> m_scene1;
	unique_ptr<GlScene> m_scene2;

	float m_rotation_mult;
};

void main_gbm()
{
	Card card;

	FAIL_IF(!card.has_atomic(), "No atomic modesetting");

	GbmDevice gdev(card);
	EglState egl(gdev.handle());

	ResourceManager resman(card);

	vector<unique_ptr<OutputHandler>> outputs;

	float rot_mult = 1;

	for (Connector* conn : card.get_connectors()) {
		if (!conn->connected())
			continue;

		resman.reserve_connector(conn);

		Crtc* crtc = resman.reserve_crtc(conn);
		auto mode = conn->get_default_mode();

		Plane* root_plane = resman.reserve_generic_plane(crtc);
		FAIL_IF(!root_plane, "Root plane not available");

		Plane* plane = nullptr;

		if (s_support_planes)
			plane = resman.reserve_generic_plane(crtc);

		auto out = new OutputHandler(card, gdev, egl, conn, crtc, mode, root_plane, plane, rot_mult);
		outputs.emplace_back(out);

		rot_mult *= 1.33;
	}

	for (auto& out : outputs)
		out->setup();

	for (auto& out : outputs)
		out->start_flipping();

	struct pollfd fds[2] = { };
	fds[0].fd = 0;
	fds[0].events =  POLLIN;
	fds[1].fd = card.fd();
	fds[1].events =  POLLIN;

	while (!s_need_exit || s_flip_pending) {
		int r = poll(fds, ARRAY_SIZE(fds), -1);
		FAIL_IF(r < 0, "poll error %d", r);

		if (fds[0].revents)
			s_need_exit = true;

		if (fds[1].revents)
			card.call_page_flip_handlers();
	}
}
