
#include <X11/Xlib-xcb.h>

#include "cube.h"
#include "cube-egl.h"
#include "cube-gles2.h"

#include "test.h"

using namespace std;

void main_x11()
{
	Display* display = XOpenDisplay(NULL);

	xcb_connection_t *c = XGetXCBConnection(display);

	/* Get the first screen */
	const xcb_setup_t      *setup  = xcb_get_setup (c);
	xcb_screen_t           *screen = xcb_setup_roots_iterator (setup).data;

	/* Create the window */

	uint32_t width;
	uint32_t height;

	if (s_fullscreen) {
		width = screen->width_in_pixels;
		height = screen->height_in_pixels;
	} else {
		width = 600;
		height = 600;
	}

	const uint32_t xcb_window_attrib_mask = XCB_CW_EVENT_MASK;
	const uint32_t xcb_window_attrib_list[] = {
		XCB_EVENT_MASK_EXPOSURE,
	};

	xcb_window_t window = xcb_generate_id (c);
	xcb_create_window (c,                    /* Connection          */
			   XCB_COPY_FROM_PARENT,          /* depth (same as root)*/
			   window,                        /* window Id           */
			   screen->root,                  /* parent window       */
			   0, 0,                          /* x, y                */
			   width, height,                 /* width, height       */
			   0,                             /* border_width        */
			   XCB_WINDOW_CLASS_INPUT_OUTPUT, /* class               */
			   screen->root_visual,           /* visual              */
			   xcb_window_attrib_mask,
			   xcb_window_attrib_list);


#if 0 // Doesn't work
	if (s_fullscreen) {

		xcb_intern_atom_cookie_t cookie = xcb_intern_atom(c, 0, 12, "_NET_WM_STATE");
		xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(c, cookie, 0);

		xcb_intern_atom_cookie_t cookie2 = xcb_intern_atom(c, 0, 24, "_NET_WM_STATE_FULLSCREEN");
		xcb_intern_atom_reply_t* reply2 = xcb_intern_atom_reply(c, cookie2, 0);

		xcb_change_property(c, XCB_PROP_MODE_REPLACE, window, reply->atom, XCB_ATOM_ATOM , 32, 1, (void*)&reply2->atom);
	}
#endif

	xcb_map_window (c, window);
	xcb_flush (c);

	EglState egl(display);
	EglSurface surface(egl, (void*)(uintptr_t)window);
	GlScene scene;

	scene.set_viewport(width, height);

	int framenum = 0;

	surface.make_current();
	surface.swap_buffers();

	xcb_generic_event_t *event;
	while ( (event = xcb_poll_for_event (c)) ) {

		surface.make_current();
		scene.draw(framenum++);
		surface.swap_buffers();
	}

	xcb_disconnect (c);
}
