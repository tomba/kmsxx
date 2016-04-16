
#include <X11/Xlib-xcb.h>

#include "cube-egl.h"
#include "cube-gles2.h"

#include "test.h"

using namespace std;

void main_x11()
{
	Display* display = XOpenDisplay(NULL);

	xcb_connection_t *connection = XGetXCBConnection(display);

	/* Get the first screen */
	const xcb_setup_t      *setup  = xcb_get_setup (connection);
	xcb_screen_t           *screen = xcb_setup_roots_iterator (setup).data;

	/* Create the window */

	uint32_t width = 600;
	uint32_t height = 600;

	const uint32_t xcb_window_attrib_mask = XCB_CW_EVENT_MASK;
	const uint32_t xcb_window_attrib_list[] = {
		XCB_EVENT_MASK_EXPOSURE,
	};

	xcb_window_t window = xcb_generate_id (connection);
	xcb_create_window (connection,                    /* Connection          */
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

	xcb_map_window (connection, window);
	xcb_flush (connection);

	EglState egl(display);
	EglSurface surface(egl, (void*)(uintptr_t)window);
	GlScene scene;

	scene.set_viewport(width, height);

	int framenum = 0;

	surface.make_current();
	surface.swap_buffers();

	xcb_generic_event_t *event;
	while ( (event = xcb_poll_for_event (connection)) ) {

		surface.make_current();
		scene.draw(framenum++);
		surface.swap_buffers();
	}

	xcb_disconnect (connection);
}
