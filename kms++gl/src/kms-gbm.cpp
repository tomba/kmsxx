#include <gbm.h>

#include <kms++/kms++.h>
#include "kms++gl/kms-egl.h"
#include "kms++gl/kms-gbm.h"

using namespace kms;
using namespace std;

#define FAIL_IF(x, fmt, ...) \
	if (x) { \
		fprintf(stderr, "%s:%d: %s:\n" fmt "\n", __FILE__, __LINE__, __PRETTY_FUNCTION__, ##__VA_ARGS__); \
		abort(); \
	}

GbmDevice::GbmDevice(Card& card)
{
	m_dev = gbm_create_device(card.fd());
	FAIL_IF(!m_dev, "failed to create gbm device");
}

GbmDevice::~GbmDevice()
{
	gbm_device_destroy(m_dev);
}

GbmSurface::GbmSurface(GbmDevice& gdev, int width, int height)
{
	m_surface = gbm_surface_create(gdev.handle(), width, height,
				       GBM_FORMAT_XRGB8888,
				       GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
	FAIL_IF(!m_surface, "failed to create gbm surface");
}

GbmSurface::~GbmSurface()
{
	gbm_surface_destroy(m_surface);
}

bool GbmSurface::has_free()
{
	return gbm_surface_has_free_buffers(m_surface);
}

gbm_bo* GbmSurface::lock_front_buffer()
{
	return gbm_surface_lock_front_buffer(m_surface);
}

void GbmSurface::release_buffer(gbm_bo* bo)
{
	gbm_surface_release_buffer(m_surface, bo);
}

gbm_surface* GbmSurface::handle() const { return m_surface; }

GbmEglSurface::GbmEglSurface(Card& card, GbmDevice& gdev, const EglState& egl, int width, int height)
	: card(card), egl(egl), m_width(width), m_height(height),
	  bo_prev(0), bo_next(0)
{
	gsurface = unique_ptr<GbmSurface>(new GbmSurface(gdev, width, height));
	esurface = eglCreateWindowSurface(egl.display(), egl.config(), gsurface->handle(), NULL);
	FAIL_IF(esurface == EGL_NO_SURFACE, "failed to create egl surface");
}

GbmEglSurface::~GbmEglSurface()
{
	if (bo_next)
		gsurface->release_buffer(bo_next);
	eglDestroySurface(egl.display(), esurface);
}

void GbmEglSurface::make_current()
{
	FAIL_IF(!gsurface->has_free(), "No free buffers");

	eglMakeCurrent(egl.display(), esurface, esurface, egl.context());
}

void GbmEglSurface::swap_buffers()
{
	eglSwapBuffers(egl.display(), esurface);
}

void GbmEglSurface::drm_fb_destroy_callback(gbm_bo* bo, void* data)
{
	auto fb = reinterpret_cast<Framebuffer*>(data);
	delete fb;
}

Framebuffer* GbmEglSurface::drm_fb_get_from_bo(gbm_bo* bo, Card& card)
{
	auto fb = reinterpret_cast<Framebuffer*>(gbm_bo_get_user_data(bo));
	if (fb)
		return fb;

	uint32_t width = gbm_bo_get_width(bo);
	uint32_t height = gbm_bo_get_height(bo);
	uint32_t stride = gbm_bo_get_stride(bo);
	uint32_t handle = gbm_bo_get_handle(bo).u32;
	PixelFormat format = (PixelFormat)gbm_bo_get_format(bo);

	vector<uint32_t> handles { handle };
	vector<uint32_t> strides { stride };
	vector<uint32_t> offsets { 0 };

	fb = new ExtFramebuffer(card, width, height, format, handles, strides, offsets);

	gbm_bo_set_user_data(bo, fb, drm_fb_destroy_callback);

	return fb;
}

Framebuffer* GbmEglSurface::lock_next()
{
	bo_prev = bo_next;
	bo_next = gsurface->lock_front_buffer();
	FAIL_IF(!bo_next, "could not lock gbm buffer");
	return drm_fb_get_from_bo(bo_next, card);
}

void GbmEglSurface::free_prev()
{
	if (bo_prev) {
		gsurface->release_buffer(bo_prev);
		bo_prev = 0;
	}
}

uint32_t GbmEglSurface::width() const { return m_width; }

uint32_t GbmEglSurface::height() const { return m_height; }
