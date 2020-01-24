#include <kms++/kms++.h>
#include <kms++util/kms++util.h>
#include <kms++util/videodevice.h>
#include <kms++util/kms++util.h>

#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#include <GLES2/gl2ext.h>
#include "esTransform.h"
#include <EGL/eglext.h>

#include <cstring>
#include <cassert>
#include <unistd.h>
#include <poll.h>
#include <cstdio>
#include <cstdlib>

#include <kms++gl/kms-egl.h>
#include <kms++gl/kms-gbm.h>
#include <kms++gl/ion.h>
#include <kms++gl/ti-pat.h>

using namespace std;
using namespace kms;

void copy_wb(Framebuffer* src, Framebuffer* dst)
{
	VideoDevice vid("/dev/video10");

	VideoStreamer* out = vid.get_output_streamer();
	VideoStreamer* in = vid.get_capture_streamer();

	out->set_format(src->format(), src->width(), src->height());
	in->set_format(dst->format(), dst->width(), dst->height());

	out->set_queue_size(1);
	in->set_queue_size(1);

	out->queue(src);
	in->queue(dst);

	out->stream_on();
	in->stream_on();

	vector<pollfd> fds(1);

	fds[0].fd = vid.fd();
	fds[0].events =  POLLIN;

	while (true) {
		int r = poll(fds.data(), fds.size(), -1);
		ASSERT(r > 0);

		if (fds[0].revents) {
			break;
		}
	}

	in->dequeue();
	out->dequeue();
}

static EGLImage createimg(EglState& egl, Framebuffer* fb)
{
	EGLAttrib attrs[] = {
		EGL_DMA_BUF_PLANE0_FD_EXT,      (EGLAttrib)fb->prime_fd(0),
		EGL_DMA_BUF_PLANE0_PITCH_EXT,   (EGLAttrib)fb->stride(0),
		EGL_DMA_BUF_PLANE0_OFFSET_EXT,  (EGLAttrib)0,
		EGL_WIDTH,                      (EGLAttrib)fb->width(),
		EGL_HEIGHT,                     (EGLAttrib)fb->height(),
		EGL_LINUX_DRM_FOURCC_EXT,       (EGLAttrib)fb->format(),
		EGL_NONE, /* end of list */
	};

	EGLImage img = eglCreateImage(egl.display(), EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, attrs);
	FAIL_IF(!img, "no image");

	return img;
}

static void glerr(const string& str)
{
	GLenum err;
	while((err = glGetError()) != GL_NO_ERROR)
		printf("%s: %d\n", str.c_str(), err);
}

void draw_gl(EglState& egl, Framebuffer* src_fb, Framebuffer* dst_fb)
{
	PFNGLEGLIMAGETARGETTEXTURE2DOESPROC target_tex_2d = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
	assert(target_tex_2d);

	EGLImage src_img = createimg(egl, src_fb);
	glerr("srcimg");

	GLuint src_tex_id;

	glGenTextures(1, &src_tex_id);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, src_tex_id);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glerr("srctex");

	target_tex_2d(GL_TEXTURE_2D, src_img);

	glerr("srcteximg");

	GLuint src_fbo_id;
	glGenFramebuffers(1, &src_fbo_id);
	glBindFramebuffer(GL_FRAMEBUFFER, src_fbo_id);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, src_tex_id, 0);
	assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

	glerr("srcfb");



	EGLImage dst_img = createimg(egl, dst_fb);

	GLuint dst_tex_id;

	glGenTextures(1, &dst_tex_id);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, dst_tex_id);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glerr("dsttex");

	target_tex_2d(GL_TEXTURE_2D, dst_img);

	glerr("dstteximg");

	GLuint dst_fbo_id;
	glGenFramebuffers(1, &dst_fbo_id);
	glBindFramebuffer(GL_FRAMEBUFFER, dst_fbo_id);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dst_tex_id, 0);
	assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);



	glBindFramebuffer(GL_READ_FRAMEBUFFER, src_fbo_id);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst_fbo_id);

	glBlitFramebuffer(0, 0, src_fb->width(), src_fb->height(), 0, 0, dst_fb->width(), dst_fb->height(), GL_COLOR_BUFFER_BIT, GL_LINEAR);
	glerr("blit");
}


int main()
{
	Card card;

	Ion s_ion;
	Pat s_pat;

	GbmDevice gdev(card);
	EglState egl(gdev.handle());
	//EglSurface surface(egl, 0);
	//surface.make_current();

	ResourceManager resman(card);

	Connector* conn = resman.reserve_connector();
	Crtc* crtc = resman.reserve_crtc(conn);
	auto mode = conn->get_default_mode();
	auto mode_blob = mode.to_blob(card);

	const int num_planes = 3;

	Plane* planes[num_planes];
	for (int i = 0; i < num_planes; ++i)
		planes[i] = resman.reserve_generic_plane(crtc);

	uint32_t w = mode.hdisplay / 2 - 8;
	uint32_t h = mode.vdisplay / 2 - 8;

	Framebuffer* fbs[num_planes];
	for (int i = 0; i < num_planes; ++i) {
		const auto format = PixelFormat::XRGB8888;

		const PixelFormatInfo& format_info = get_pixel_format_info(format);

		if (0) {
			printf("Using DumbBuffer\n");
			fbs[i] = new DumbFramebuffer(card, w, h, format);
		} else {
			bool cached = false;

			printf("Using ION, cached %d\n", cached);

			int ion_buf_fd = s_ion.alloc(w * h *  format_info.planes[0].bitspp / 8, Ion::IonHeapType::SYSTEM_CONTIG, cached);

			vector<int> fds = { ion_buf_fd };
			vector<uint32_t> pitches = { w * format_info.planes[0].bitspp / 8 };
			vector<uint32_t> offsets = { 0 };

			auto fb = new DmabufFramebuffer(card, w, h, format, fds, pitches, offsets);
			fbs[i] = fb;
		}
	}

	AtomicReq req(card);

	req.add(conn, {
			{ "CRTC_ID", crtc->id() },
		});

	req.add(crtc, {
			{ "ACTIVE", 1 },
			{ "MODE_ID", mode_blob->id() },
		});

	for (int i = 0; i < num_planes; ++i) {
		req.add(planes[i], {
				{ "FB_ID", fbs[i]->id() },
				{ "CRTC_ID", crtc->id() },
				{ "SRC_X", 0 << 16 },
				{ "SRC_Y", 0 << 16 },
				{ "SRC_W", fbs[i]->width() << 16 },
				{ "SRC_H", fbs[i]->height() << 16 },
				{ "CRTC_X", (i % 2) * mode.hdisplay / 2 },
				{ "CRTC_Y", (i / 2) * mode.vdisplay / 2 },
				{ "CRTC_W", fbs[i]->width() },
				{ "CRTC_H", fbs[i]->height() },
			});
	}

	int r;

	r = req.commit_sync(true);
	if (r)
		EXIT("Atomic commit failed: %d\n", r);

	const bool wait_enter = false;

	printf("blank fbs\n");

	if (wait_enter)
		getchar();

	printf("green fbs\n");

	for (int i = 0; i < num_planes; ++i)
		draw_rect(*fbs[i], 0, 0, fbs[i]->width(), fbs[i]->height(), RGB(0, 0xff, 0));

	if (wait_enter)
		getchar();

	printf("read fbs\n");

	{
		fbs[0]->begin_cpu_access(CpuAccess::Read);
		fbs[1]->begin_cpu_access(CpuAccess::Read);

		uint32_t* p1 = (uint32_t*)fbs[0]->map(0);
		uint32_t* p2 = (uint32_t*)fbs[1]->map(0);
		for (uint32_t i = 0; i < fbs[0]->size(0) / 4; ++i) {
			if (p1[i] != p2[i])
				printf("FB0/1: %u: %x != %x\n", i, p1[i], p2[i]);
		}

		fbs[0]->end_cpu_access();
		fbs[1]->end_cpu_access();
	}

	{
		fbs[0]->begin_cpu_access(CpuAccess::Read);
		fbs[2]->begin_cpu_access(CpuAccess::Read);

		uint32_t* p1 = (uint32_t*)fbs[0]->map(0);
		uint32_t* p2 = (uint32_t*)fbs[2]->map(0);
		for (uint32_t i = 0; i < fbs[0]->size(0) / 4; ++i) {
			if (p1[i] != p2[i])
				printf("FB0/2: %u: %x != %x\n", i, p1[i], p2[i]);
		}

		fbs[0]->end_cpu_access();
		fbs[2]->end_cpu_access();
	}

	if (wait_enter)
		getchar();

	printf("blue fbs\n");

	for (int i = 0; i < num_planes; ++i)
		draw_rect(*fbs[i], 0, 0, fbs[i]->width(), fbs[i]->height(), RGB(0, 0, 0xff));

	if (wait_enter)
		getchar();

	printf("test pattern on fb0\n");

	draw_test_pattern(*fbs[0]);

	if (wait_enter)
		getchar();

	printf("copy fb0 -> fb1 with GPU\n");

	draw_gl(egl, fbs[0], fbs[1]);

	if (wait_enter)
		getchar();

	printf("copy fb1 -> fb2 with WB\n");

	copy_wb(fbs[1], fbs[2]);

	if (1) {
		printf("check FBs\n");
		{
			fbs[0]->begin_cpu_access(CpuAccess::Read);
			fbs[1]->begin_cpu_access(CpuAccess::Read);

			uint32_t* p1 = (uint32_t*)fbs[0]->map(0);
			uint32_t* p2 = (uint32_t*)fbs[1]->map(0);
			for (uint32_t i = 0; i < fbs[0]->size(0) / 4; ++i) {
				if (p1[i] != p2[i])
					printf("FB0/1: %u: %x != %x\n", i, p1[i], p2[i]);
			}

			fbs[0]->end_cpu_access();
			fbs[1]->end_cpu_access();
		}

		{
			fbs[0]->begin_cpu_access(CpuAccess::Read);
			fbs[2]->begin_cpu_access(CpuAccess::Read);

			uint32_t* p1 = (uint32_t*)fbs[0]->map(0);
			uint32_t* p2 = (uint32_t*)fbs[2]->map(0);
			for (uint32_t i = 0; i < fbs[0]->size(0) / 4; ++i) {
				// note: ignore alpha as WB does not copy it
				if ((p1[i] & 0xffffff) != p2[i])
					printf("FB0/2: %u: %x != %x\n", i, p1[i], p2[i]);
			}

			fbs[0]->end_cpu_access();
			fbs[2]->end_cpu_access();
		}
	}

	printf("done\n");
	getchar();

	return 0;
}
