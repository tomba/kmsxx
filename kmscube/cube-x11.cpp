
#include <kms++util/kms++util.h>

#include "cube.h"
#include "cube-egl.h"
#include "cube-gles2.h"

#include <X11/Xlib-xcb.h>
#include <X11/Xlibint.h>

using namespace std;

static void main_loop(Display* dpy, xcb_connection_t *c, xcb_window_t window, uint32_t width, uint32_t height)
{
	EglState egl(dpy);
	EglSurface surface(egl, (void*)(uintptr_t)window);
	GlScene scene;

	scene.set_viewport(width, height);

	unsigned framenum = 0;

	surface.make_current();
	surface.swap_buffers();

	bool need_exit = false;

	xcb_generic_event_t *event;
	while (true) {

		while ((event = xcb_poll_for_event (c))) {
			bool handled = false;
			uint8_t response_type = event->response_type & ~0x80;

			switch (response_type) {
			case XCB_EXPOSE: {
				handled = true;
				break;
			}
			case XCB_KEY_PRESS: {
				handled = true;

				xcb_key_press_event_t *kp = (xcb_key_press_event_t *)event;
				if (kp->detail == 24 || kp->detail == 9) {
					printf("Exit due to keypress\n");
					need_exit = true;
				}

				break;
			}
			}

			if (!handled) {
				// Check if a custom XEvent constructor was registered in xlib for this event type, and call it discarding the constructed XEvent if any.
				// XESetWireToEvent might be used by libraries to intercept messages from the X server e.g. the OpenGL lib waiting for DRI2 events.

				XLockDisplay(dpy);
				Bool (*proc)(Display*, XEvent*, xEvent*) = XESetWireToEvent(dpy, response_type, NULL);
				if (proc) {
					XESetWireToEvent(dpy, response_type, proc);
					XEvent dummy;
					event->sequence = LastKnownRequestProcessed(dpy);
					proc(dpy, &dummy, (xEvent*)event);
				}
				XUnlockDisplay(dpy);
			}

			free(event);
		}

		if (s_num_frames && framenum >= s_num_frames)
			need_exit = true;

		if (need_exit)
			break;

		// this should be in XCB_EXPOSE, but we don't get the event after swaps...
		scene.draw(framenum++);
		surface.swap_buffers();
	}
}

void main_x11()
{
	Display* dpy = XOpenDisplay(NULL);
	FAIL_IF(!dpy, "Failed to connect to the X server");

	xcb_connection_t *c = XGetXCBConnection(dpy);

	/* Acquire event queue ownership */
	XSetEventQueueOwner(dpy, XCBOwnsEventQueue);

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

	const uint32_t xcb_window_attrib_mask = XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
	const uint32_t xcb_window_attrib_list[] = {
		// OVERRIDE_REDIRECT
		0,
		// EVENT_MASK
		XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS,
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

	if (s_fullscreen)
	{
		const char *net_wm_state = "_NET_WM_STATE";
		const char *net_wm_state_fullscreen = "_NET_WM_STATE_FULLSCREEN";

		xcb_intern_atom_cookie_t cookie = xcb_intern_atom(c, 0, strlen(net_wm_state), net_wm_state);
		xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(c, cookie, 0);

		xcb_intern_atom_cookie_t cookie2 = xcb_intern_atom(c, 0, strlen(net_wm_state_fullscreen), net_wm_state_fullscreen);
		xcb_intern_atom_reply_t* reply2 = xcb_intern_atom_reply(c, cookie2, 0);

		xcb_change_property(c, XCB_PROP_MODE_REPLACE, window, reply->atom, XCB_ATOM_ATOM , 32, 1, (void*)&reply2->atom);
	}

	xcb_map_window (c, window);
	xcb_flush (c);

	main_loop(dpy, c, window, width, height);

	xcb_flush(c);
	xcb_unmap_window(c, window);
	xcb_destroy_window(c, window);

	XCloseDisplay(dpy);
}
