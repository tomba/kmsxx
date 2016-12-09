#include "cube-egl.h"
#include "cube.h"

#include <kms++util/kms++util.h>

using namespace std;

static void print_egl_config(EGLDisplay dpy, EGLConfig cfg)
{
	auto getconf = [dpy, cfg](EGLint a) { EGLint v = -1; eglGetConfigAttrib(dpy, cfg, a, &v); return v; };

	printf("EGL Config %d: color buf %d/%d/%d/%d = %d, depth %d, stencil %d, native visualid %d, native visualtype %d\n",
	       getconf(EGL_CONFIG_ID),
	       getconf(EGL_ALPHA_SIZE),
	       getconf(EGL_RED_SIZE),
	       getconf(EGL_GREEN_SIZE),
	       getconf(EGL_BLUE_SIZE),
	       getconf(EGL_BUFFER_SIZE),
	       getconf(EGL_DEPTH_SIZE),
	       getconf(EGL_STENCIL_SIZE),
	       getconf(EGL_NATIVE_VISUAL_ID),
	       getconf(EGL_NATIVE_VISUAL_TYPE));
}

EglState::EglState(void *native_display)
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

	m_display = eglGetDisplay((EGLNativeDisplayType)native_display);
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


	if (s_verbose) {
		EGLint numConfigs;
		b = eglGetConfigs(m_display, nullptr, 0, &numConfigs);
		FAIL_IF(!b, "failed to get number of configs");

		EGLConfig configs[numConfigs];
		b = eglGetConfigs(m_display, configs, numConfigs, &numConfigs);

		printf("Available configs:\n");

		for (int i = 0; i < numConfigs; ++i)
			print_egl_config(m_display, configs[i]);
	}

	b = eglChooseConfig(m_display, config_attribs, &m_config, 1, &n);
	FAIL_IF(!b || n != 1, "failed to choose config");

	if (s_verbose) {
		printf("Chosen config:\n");
		print_egl_config(m_display, m_config);
	}

	m_context = eglCreateContext(m_display, m_config, EGL_NO_CONTEXT, context_attribs);
	FAIL_IF(!m_context, "failed to create context");

	EGLBoolean ok = eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, m_context);
	FAIL_IF(!ok, "eglMakeCurrent() failed");
}

EglState::~EglState()
{
	eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglTerminate(m_display);
}

EglSurface::EglSurface(const EglState &egl, void *native_window)
	: egl(egl)
{
	esurface = eglCreateWindowSurface(egl.display(), egl.config(), (EGLNativeWindowType)native_window, NULL);
	FAIL_IF(esurface == EGL_NO_SURFACE, "failed to create egl surface");
}

EglSurface::~EglSurface()
{
	eglDestroySurface(egl.display(), esurface);
}

void EglSurface::make_current()
{
	EGLBoolean ok = eglMakeCurrent(egl.display(), esurface, esurface, egl.context());
	FAIL_IF(!ok, "eglMakeCurrent() failed");
}

void EglSurface::swap_buffers()
{
	EGLBoolean ok = eglSwapBuffers(egl.display(), esurface);
	FAIL_IF(!ok, "eglMakeCurrent() failed");
}
