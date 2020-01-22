#pragma once

#include <kms++/kms++.h>
#include "kms++gl/kms-egl.h"

struct gbm_device;
struct gbm_bo;

namespace kms
{

class GbmDevice
{
public:
	GbmDevice(Card& card);

	~GbmDevice();

	GbmDevice(const GbmDevice& other) = delete;
	GbmDevice& operator=(const GbmDevice& other) = delete;

	struct gbm_device* handle() const { return m_dev; }

private:
	struct gbm_device* m_dev;
};

class GbmSurface
{
public:
	GbmSurface(GbmDevice& gdev, int width, int height);

	~GbmSurface();

	GbmSurface(const GbmSurface& other) = delete;
	GbmSurface& operator=(const GbmSurface& other) = delete;

	bool has_free();

	gbm_bo* lock_front_buffer();

	void release_buffer(gbm_bo *bo);

	struct gbm_surface* handle() const;

private:
	struct gbm_surface* m_surface;
};

class GbmEglSurface
{
public:
	GbmEglSurface(Card& card, GbmDevice& gdev, const EglState& egl, int width, int height);

	~GbmEglSurface();

	void make_current();

	void swap_buffers();

	static void drm_fb_destroy_callback(struct gbm_bo *bo, void *data);

	static Framebuffer* drm_fb_get_from_bo(struct gbm_bo *bo, Card& card);

	Framebuffer* lock_next();

	void free_prev();

	uint32_t width() const;
	uint32_t height() const;

private:
	Card& card;
	const EglState& egl;

	std::unique_ptr<GbmSurface> gsurface;
	EGLSurface esurface;

	int m_width;
	int m_height;

	struct gbm_bo* bo_prev;
	struct gbm_bo* bo_next;
};

}
