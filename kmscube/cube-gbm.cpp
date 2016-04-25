#include <chrono>
#include <cstdio>
#include <vector>
#include <memory>
#include <algorithm>
#include <poll.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>

#include <kms++.h>
#include "test.h"
#include "cube-egl.h"
#include "cube-gles2.h"

using namespace kms;
using namespace std;

static int s_flip_pending;
static bool s_need_exit;

static bool s_support_planes;

class GbmDevice
{
public:
	GbmDevice(Card& card)
	{
		m_dev = gbm_create_device(card.fd());
		FAIL_IF(!m_dev, "failed to create gbm device");
	}

	~GbmDevice()
	{
		gbm_device_destroy(m_dev);
	}

	GbmDevice(const GbmDevice& other) = delete;
	GbmDevice& operator=(const GbmDevice& other) = delete;

	struct gbm_device* handle() const { return m_dev; }

private:
	struct gbm_device* m_dev;
};

class GbmSurface
{
public:
	GbmSurface(GbmDevice& gdev, int width, int height)
	{
		m_surface = gbm_surface_create(gdev.handle(), width, height,
					       GBM_FORMAT_XRGB8888,
					       GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
		FAIL_IF(!m_surface, "failed to create gbm surface");
	}

	~GbmSurface()
	{
		gbm_surface_destroy(m_surface);
	}

	GbmSurface(const GbmSurface& other) = delete;
	GbmSurface& operator=(const GbmSurface& other) = delete;

	bool has_free()
	{
		return gbm_surface_has_free_buffers(m_surface);
	}

	gbm_bo* lock_front_buffer()
	{
		return gbm_surface_lock_front_buffer(m_surface);
	}

	void release_buffer(gbm_bo *bo)
	{
		gbm_surface_release_buffer(m_surface, bo);
	}

	struct gbm_surface* handle() const { return m_surface; }

private:
	struct gbm_surface* m_surface;
};

class GbmEglSurface
{
public:
	GbmEglSurface(Card& card, GbmDevice& gdev, const EglState& egl, int width, int height)
		: card(card), egl(egl), m_width(width), m_height(height),
		  bo_prev(0), bo_next(0)
	{
		gsurface = unique_ptr<GbmSurface>(new GbmSurface(gdev, width, height));
		esurface = eglCreateWindowSurface(egl.display(), egl.config(), gsurface->handle(), NULL);
		FAIL_IF(esurface == EGL_NO_SURFACE, "failed to create egl surface");
	}

	~GbmEglSurface()
	{
		if (bo_next)
			gsurface->release_buffer(bo_next);
		eglDestroySurface(egl.display(), esurface);
	}

	void make_current()
	{
		FAIL_IF(!gsurface->has_free(), "No free buffers");

		eglMakeCurrent(egl.display(), esurface, esurface, egl.context());
	}

	void swap_buffers()
	{
		eglSwapBuffers(egl.display(), esurface);
	}

	static void drm_fb_destroy_callback(struct gbm_bo *bo, void *data)
	{
		auto fb = reinterpret_cast<Framebuffer*>(data);
		delete fb;
	}

	static Framebuffer* drm_fb_get_from_bo(struct gbm_bo *bo, Card& card)
	{
		auto fb = reinterpret_cast<Framebuffer*>(gbm_bo_get_user_data(bo));
		if (fb)
			return fb;

		uint32_t width = gbm_bo_get_width(bo);
		uint32_t height = gbm_bo_get_height(bo);
		uint32_t stride = gbm_bo_get_stride(bo);
		uint32_t handle = gbm_bo_get_handle(bo).u32;

		fb = new ExtFramebuffer(card, width, height, 24, 32, stride, handle);

		gbm_bo_set_user_data(bo, fb, drm_fb_destroy_callback);

		return fb;
	}

	Framebuffer* lock_next()
	{
		bo_prev = bo_next;
		bo_next = gsurface->lock_front_buffer();
		FAIL_IF(!bo_next, "could not lock gbm buffer");
		return drm_fb_get_from_bo(bo_next, card);
	}

	void free_prev()
	{
		if (bo_prev) {
			gsurface->release_buffer(bo_prev);
			bo_prev = 0;
		}
	}

	uint32_t width() const { return m_width; }
	uint32_t height() const { return m_height; }

private:
	Card& card;
	const EglState& egl;

	unique_ptr<GbmSurface> gsurface;
	EGLSurface esurface;

	int m_width;
	int m_height;

	struct gbm_bo* bo_prev;
	struct gbm_bo* bo_next;
};

class OutputHandler : private PageFlipHandlerBase
{
public:
	OutputHandler(Card& card, GbmDevice& gdev, const EglState& egl, Connector* connector, Crtc* crtc, Videomode& mode, Plane* plane, float rotation_mult)
		: m_frame_num(0), m_connector(connector), m_crtc(crtc), m_plane(plane), m_mode(mode),
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

		if (m_crtc->card().has_atomic()) {
			int r;

			AtomicReq req(m_crtc->card());

			req.add(m_root_plane, "FB_ID", fb->id());
			if (m_plane)
				req.add(m_plane, "FB_ID", planefb->id());

			r = req.test();
			FAIL_IF(r, "atomic test failed");

			r = req.commit(this);
			FAIL_IF(r, "atomic commit failed");
		} else {
			int ret;

			ret = m_crtc->page_flip(*fb, this);
			FAIL_IF(ret, "failed to queue page flip");

			if (m_plane) {
				ret = m_crtc->set_plane(m_plane, *planefb,
							0, 0, planefb->width(), planefb->height(),
							0, 0, planefb->width(), planefb->height());
				FAIL_IF(ret, "failed to set plane");
			}
		}

		s_flip_pending++;
	}

	int m_frame_num;
	chrono::steady_clock::time_point m_t1;

	Connector* m_connector;
	Crtc* m_crtc;
	Plane* m_plane;
	Videomode m_mode;
	Plane* m_root_plane;

	unique_ptr<GbmEglSurface> m_surface1;
	unique_ptr<GbmEglSurface> m_surface2;

	unique_ptr<GlScene> m_scene1;
	unique_ptr<GlScene> m_scene2;

	float m_rotation_mult;
};

void main_gbm()
{
	Card card;

	GbmDevice gdev(card);
	EglState egl(gdev.handle());

	vector<unique_ptr<OutputHandler>> outputs;
	vector<Plane*> used_planes;

	float rot_mult = 1;

	for (auto pipe : card.get_connected_pipelines()) {
		auto connector = pipe.connector;
		auto crtc = pipe.crtc;
		auto mode = connector->get_default_mode();

		Plane* plane = 0;

		if (s_support_planes) {
			for (Plane* p : crtc->get_possible_planes()) {
				if (find(used_planes.begin(), used_planes.end(), p) != used_planes.end())
					continue;

				if (p->plane_type() != PlaneType::Overlay)
					continue;

				plane = p;
				break;
			}
		}

		if (plane)
			used_planes.push_back(plane);

		auto out = new OutputHandler(card, gdev, egl, connector, crtc, mode, plane, rot_mult);
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
