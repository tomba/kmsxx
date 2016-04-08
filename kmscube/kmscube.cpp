/*
 * Copyright (c) 2012 Arvin Schnell <arvin.schnell@gmail.com>
 * Copyright (c) 2012 Rob Clark <rob@ti.com>
 * Copyright (c) 2015 Tomi Valkeinen <tomi.valkeinen@ti.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/* Based on a egl cube test app originally written by Arvin Schnell */

#include <chrono>
#include <cstdio>
#include <vector>
#include <memory>
#include <algorithm>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>

#include "esUtil.h"

#include <kms++.h>
#include "test.h"

using namespace kms;
using namespace std;

static bool s_verbose;
static int s_flip_pending;
static bool s_need_exit;

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

class EglState
{
public:
	EglState(EGLNativeDisplayType display_id)
	{
		EGLBoolean b;
		EGLint major, minor, n;

		static const EGLint context_attribs[] = {
			EGL_CONTEXT_CLIENT_VERSION, 2,
			EGL_NONE
		};

		static const EGLint config_attribs[] = {
			EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
			EGL_RED_SIZE, 8,
			EGL_GREEN_SIZE, 8,
			EGL_BLUE_SIZE, 8,
			EGL_ALPHA_SIZE, 0,
			EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
			EGL_NONE
		};

		m_display = eglGetDisplay(display_id);
		FAIL_IF(!m_display, "failed to get egl display");

		b = eglInitialize(m_display, &major, &minor);
		FAIL_IF(!b, "failed to initialize");

		if (s_verbose) {
			printf("Using display %p with EGL version %d.%d\n", m_display, major, minor);

			printf("EGL_VENDOR:      %s\n", eglQueryString(m_display, EGL_VENDOR));
			printf("EGL_VERSION:     %s\n", eglQueryString(m_display, EGL_VERSION));
			printf("EGL_EXTENSIONS:  %s\n", eglQueryString(m_display, EGL_EXTENSIONS));
			printf("EGL_CLIENT_APIS: %s\n", eglQueryString(m_display, EGL_CLIENT_APIS));
		}

		b = eglBindAPI(EGL_OPENGL_ES_API);
		FAIL_IF(!b, "failed to bind api EGL_OPENGL_ES_API");

		b = eglChooseConfig(m_display, config_attribs, &m_config, 1, &n);
		FAIL_IF(!b || n != 1, "failed to choose config");

		auto getconf = [this](EGLint a) { EGLint v = -1; eglGetConfigAttrib(m_display, m_config, a, &v); return v; };

		if (s_verbose) {
			printf("EGL Config %d: color buf %d/%d/%d/%d = %d, depth %d, stencil %d\n",
			       getconf(EGL_CONFIG_ID),
			       getconf(EGL_ALPHA_SIZE),
			       getconf(EGL_RED_SIZE),
			       getconf(EGL_GREEN_SIZE),
			       getconf(EGL_BLUE_SIZE),
			       getconf(EGL_BUFFER_SIZE),
			       getconf(EGL_DEPTH_SIZE),
			       getconf(EGL_STENCIL_SIZE));
		}

		m_context = eglCreateContext(m_display, m_config, EGL_NO_CONTEXT, context_attribs);
		FAIL_IF(!m_context, "failed to create context");

		eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, m_context);
	}

	~EglState()
	{
		eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		eglTerminate(m_display);
	}

	EGLDisplay display() const { return m_display; }
	EGLConfig config() const { return m_config; }
	EGLContext context() const { return m_context; }

private:
	EGLDisplay m_display;
	EGLConfig m_config;
	EGLContext m_context;
};

class GlScene
{
public:
	GlScene()
	{
		GLuint vertex_shader, fragment_shader;
		GLint ret;

#include "cube.h"

		static const char *vertex_shader_source =
				"uniform mat4 modelviewMatrix;      \n"
				"uniform mat4 modelviewprojectionMatrix;\n"
				"uniform mat3 normalMatrix;         \n"
				"                                   \n"
				"attribute vec4 in_position;        \n"
				"attribute vec3 in_normal;          \n"
				"attribute vec4 in_color;           \n"
				"\n"
				"vec4 lightSource = vec4(2.0, 2.0, 20.0, 0.0);\n"
				"                                   \n"
				"varying vec4 vVaryingColor;        \n"
				"                                   \n"
				"void main()                        \n"
				"{                                  \n"
				"    gl_Position = modelviewprojectionMatrix * in_position;\n"
				"    vec3 vEyeNormal = normalMatrix * in_normal;\n"
				"    vec4 vPosition4 = modelviewMatrix * in_position;\n"
				"    vec3 vPosition3 = vPosition4.xyz / vPosition4.w;\n"
				"    vec3 vLightDir = normalize(lightSource.xyz - vPosition3);\n"
				"    float diff = max(0.0, dot(vEyeNormal, vLightDir));\n"
				"    vVaryingColor = vec4(diff * in_color.rgb, 1.0);\n"
				"}                                  \n";

		static const char *fragment_shader_source =
				"precision mediump float;           \n"
				"                                   \n"
				"varying vec4 vVaryingColor;        \n"
				"                                   \n"
				"void main()                        \n"
				"{                                  \n"
				"    gl_FragColor = vVaryingColor;  \n"
				"}                                  \n";


		if (s_verbose) {
			printf("GL_VENDOR:       %s\n", glGetString(GL_VENDOR));
			printf("GL_VERSION:      %s\n", glGetString(GL_VERSION));
			printf("GL_RENDERER:     %s\n", glGetString(GL_RENDERER));
			printf("GL_EXTENSIONS:   %s\n", glGetString(GL_EXTENSIONS));
		}

		vertex_shader = glCreateShader(GL_VERTEX_SHADER);

		glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
		glCompileShader(vertex_shader);

		glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &ret);
		FAIL_IF(!ret, "vertex shader compilation failed!");

		fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

		glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
		glCompileShader(fragment_shader);

		glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &ret);
		FAIL_IF(!ret, "fragment shader compilation failed!");

		GLuint program = glCreateProgram();

		glAttachShader(program, vertex_shader);
		glAttachShader(program, fragment_shader);

		glBindAttribLocation(program, 0, "in_position");
		glBindAttribLocation(program, 1, "in_normal");
		glBindAttribLocation(program, 2, "in_color");

		glLinkProgram(program);

		glGetProgramiv(program, GL_LINK_STATUS, &ret);
		FAIL_IF(!ret, "program linking failed!");

		glUseProgram(program);

		m_modelviewmatrix = glGetUniformLocation(program, "modelviewMatrix");
		m_modelviewprojectionmatrix = glGetUniformLocation(program, "modelviewprojectionMatrix");
		m_normalmatrix = glGetUniformLocation(program, "normalMatrix");

		glEnable(GL_CULL_FACE);

		GLintptr positionsoffset = 0;
		GLintptr colorsoffset = sizeof(vVertices);
		GLintptr normalsoffset = sizeof(vVertices) + sizeof(vColors);
		GLuint vbo;

		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vVertices) + sizeof(vColors) + sizeof(vNormals), 0, GL_STATIC_DRAW);
		glBufferSubData(GL_ARRAY_BUFFER, positionsoffset, sizeof(vVertices), &vVertices[0]);
		glBufferSubData(GL_ARRAY_BUFFER, colorsoffset, sizeof(vColors), &vColors[0]);
		glBufferSubData(GL_ARRAY_BUFFER, normalsoffset, sizeof(vNormals), &vNormals[0]);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (const GLvoid*)positionsoffset);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (const GLvoid*)normalsoffset);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, (const GLvoid*)colorsoffset);
		glEnableVertexAttribArray(2);
	}

	GlScene(const GlScene& other) = delete;
	GlScene& operator=(const GlScene& other) = delete;

	void set_viewport(uint32_t width, uint32_t height)
	{
		m_width = width;
		m_height = height;
	}

	void draw(uint32_t framenum)
	{
		glViewport(0, 0, m_width, m_height);

		glClearColor(0.5, 0.5, 0.5, 1.0);
		glClear(GL_COLOR_BUFFER_BIT);

		ESMatrix modelview;

		esMatrixLoadIdentity(&modelview);
		esTranslate(&modelview, 0.0f, 0.0f, -8.0f);
		esRotate(&modelview, 45.0f + (0.75f * framenum), 1.0f, 0.0f, 0.0f);
		esRotate(&modelview, 45.0f - (0.5f * framenum), 0.0f, 1.0f, 0.0f);
		esRotate(&modelview, 10.0f + (0.45f * framenum), 0.0f, 0.0f, 1.0f);

		GLfloat aspect = (float)m_height / m_width;

		ESMatrix projection;
		esMatrixLoadIdentity(&projection);
		esFrustum(&projection, -2.8f, +2.8f, -2.8f * aspect, +2.8f * aspect, 6.0f, 10.0f);

		ESMatrix modelviewprojection;
		esMatrixLoadIdentity(&modelviewprojection);
		esMatrixMultiply(&modelviewprojection, &modelview, &projection);

		float normal[9];
		normal[0] = modelview.m[0][0];
		normal[1] = modelview.m[0][1];
		normal[2] = modelview.m[0][2];
		normal[3] = modelview.m[1][0];
		normal[4] = modelview.m[1][1];
		normal[5] = modelview.m[1][2];
		normal[6] = modelview.m[2][0];
		normal[7] = modelview.m[2][1];
		normal[8] = modelview.m[2][2];

		glUniformMatrix4fv(m_modelviewmatrix, 1, GL_FALSE, &modelview.m[0][0]);
		glUniformMatrix4fv(m_modelviewprojectionmatrix, 1, GL_FALSE, &modelviewprojection.m[0][0]);
		glUniformMatrix3fv(m_normalmatrix, 1, GL_FALSE, normal);

		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glDrawArrays(GL_TRIANGLE_STRIP, 4, 4);
		glDrawArrays(GL_TRIANGLE_STRIP, 8, 4);
		glDrawArrays(GL_TRIANGLE_STRIP, 12, 4);
		glDrawArrays(GL_TRIANGLE_STRIP, 16, 4);
		glDrawArrays(GL_TRIANGLE_STRIP, 20, 4);
	}

private:
	GLint m_modelviewmatrix, m_modelviewprojectionmatrix, m_normalmatrix;

	uint32_t m_width;
	uint32_t m_height;
};

class GbmEglSurface
{
public:
	GbmEglSurface(Card& card, GbmDevice& gdev, const EglState& egl, int width, int height)
		: card(card), gdev(gdev), egl(egl), m_width(width), m_height(height),
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

	struct Framebuffer* lock_next()
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
	GbmDevice& gdev;
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
		struct Framebuffer* fb = m_surface1->lock_next();

		struct Framebuffer* planefb = 0;

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
		struct Framebuffer* fb = m_surface1->lock_next();

		struct Framebuffer* planefb = 0;

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

int main(int argc, char *argv[])
{
	for (int i = 1; i < argc; ++i) {
		if (argv[i] == string("-v"))
			s_verbose = true;
	}

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

		for (Plane* p : crtc->get_possible_planes()) {
			if (find(used_planes.begin(), used_planes.end(), p) != used_planes.end())
				continue;

			if (p->plane_type() != PlaneType::Overlay)
				continue;

			plane = p;
			break;
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

	while (!s_need_exit || s_flip_pending) {
		fd_set fds;
		FD_ZERO(&fds);
		if (!s_need_exit)
			FD_SET(0, &fds);
		FD_SET(card.fd(), &fds);

		int ret = select(card.fd() + 1, &fds, NULL, NULL, NULL);

		FAIL_IF(ret < 0, "select error: %d", ret);
		FAIL_IF(ret == 0, "select timeout");

		if (FD_ISSET(0, &fds))
			s_need_exit = true;

		if (FD_ISSET(card.fd(), &fds))
			card.call_page_flip_handlers();
	}

	return 0;
}
