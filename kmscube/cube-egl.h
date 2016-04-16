#pragma once

#include <EGL/egl.h>

class EglState
{
public:
	EglState(void *native_display);
	~EglState();

	EGLDisplay display() const { return m_display; }
	EGLConfig config() const { return m_config; }
	EGLContext context() const { return m_context; }

private:
	EGLDisplay m_display;
	EGLConfig m_config;
	EGLContext m_context;
};

class EglSurface
{
public:
	EglSurface(const EglState& egl, void *native_window);
	~EglSurface();

	void make_current();
	void swap_buffers();

private:
	const EglState& egl;

	EGLSurface esurface;
};
