
#include <wayland-client.h>
#include <wayland-egl.h>

#include <string.h>
#include <stdio.h>

#include "cube.h"
#include "cube-egl.h"
#include "cube-gles2.h"

static struct wl_compositor *s_compositor = NULL;
static struct wl_shell *s_shell = NULL;
static char s_running = 1;

struct window {
	struct wl_surface *surface;
	struct wl_shell_surface *shell_surface;
	struct wl_egl_window *egl_window;
};

// listeners
static void registry_add_object(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
	if (!strcmp(interface, "wl_compositor"))
		s_compositor = (struct wl_compositor*)wl_registry_bind(registry, name, &wl_compositor_interface, 0);
	else if (!strcmp(interface, "wl_shell"))
		s_shell = (struct wl_shell*)wl_registry_bind(registry, name, &wl_shell_interface, 0);
}

static void registry_remove_object(void *data, struct wl_registry *registry, uint32_t name)
{

}

static struct wl_registry_listener registry_listener = { &registry_add_object, &registry_remove_object };

static void shell_surface_ping(void *data, struct wl_shell_surface *shell_surface, uint32_t serial)
{
	wl_shell_surface_pong(shell_surface, serial);
}

static void shell_surface_configure(void *data, struct wl_shell_surface *shell_surface, uint32_t edges, int32_t width, int32_t height)
{
	struct window *window = (struct window*)data;

	wl_egl_window_resize(window->egl_window, width, height, 0, 0);
}

static void shell_surface_popup_done(void *data, struct wl_shell_surface *shell_surface)
{

}

static struct wl_shell_surface_listener shell_surface_listener = {
	&shell_surface_ping, &shell_surface_configure, &shell_surface_popup_done
};

static void create_window(struct window *window, int32_t width, int32_t height)
{
	window->surface = wl_compositor_create_surface(s_compositor);
	window->shell_surface = wl_shell_get_shell_surface(s_shell, window->surface);
	wl_shell_surface_add_listener(window->shell_surface, &shell_surface_listener, window);
	wl_shell_surface_set_toplevel(window->shell_surface);
	window->egl_window = wl_egl_window_create(window->surface, width, height);
}

static void delete_window(struct window *window)
{
	wl_egl_window_destroy(window->egl_window);
	wl_shell_surface_destroy(window->shell_surface);
	wl_surface_destroy(window->surface);
}

void main_wl()
{
	struct wl_display *display = wl_display_connect(NULL);
	struct wl_registry *registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	wl_display_roundtrip(display);

	uint32_t width = 600;
	uint32_t height = 600;

	struct window window;
	create_window(&window, width, height);

	{
		EglState egl(display);
		EglSurface surface(egl, window.egl_window);
		GlScene scene;

		scene.set_viewport(width, height);

		int framenum = 0;

		while (s_running) {
			wl_display_dispatch_pending(display);

			surface.make_current();
			scene.draw(framenum++);
			surface.swap_buffers();
		}
	}

	delete_window(&window);
	wl_display_disconnect(display);
}
