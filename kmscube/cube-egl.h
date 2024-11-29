#pragma once

#include <EGL/egl.h>

class EglState
{
public:
	EglState(void* native_display);
	EglState(void* native_display, EGLint native_visual_id);
	~EglState();

	EGLDisplay display() const { return m_display; }
	EGLConfig config() const { return m_config; }
	EGLContext context() const { return m_context; }
	EGLint native_visual_id() const { return m_native_visual_id; }

private:
	EGLDisplay m_display;
	EGLConfig m_config;
	EGLContext m_context;
	EGLint m_native_visual_id;
};

class EglSurface
{
public:
	EglSurface(const EglState& egl, void* native_window);
	~EglSurface();

	void make_current();
	void swap_buffers();

private:
	const EglState& egl;

	EGLSurface esurface;
};
