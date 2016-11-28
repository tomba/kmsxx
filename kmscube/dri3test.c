#include <X11/Xlib-xcb.h>
#include <X11/Xlibint.h>
#include <xcb/dri3.h>
#include <xcb/present.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>
#include <drm.h>
#include <drm_mode.h>
#include <gbm.h>

#ifdef HAS_LIBDRM_ETNAVIV
#include <libdrm/etnaviv_drmif.h>
#endif

#define FAIL(fmt, ...) \
	do { \
	fprintf(stderr, "%s:%d: %s:\n" fmt "\n", __FILE__, __LINE__, __PRETTY_FUNCTION__, ##__VA_ARGS__); \
	abort(); \
	} while(0)

#define FAIL_IF(x, fmt, ...) \
	if (x) { \
	fprintf(stderr, "%s:%d: %s:\n" fmt "\n", __FILE__, __LINE__, __PRETTY_FUNCTION__, ##__VA_ARGS__); \
	abort(); \
	}

static bool s_fullscreen = false;

struct buffer;
struct drawable;

struct display
{
	xcb_window_t window;
	xcb_connection_t *connection;
	xcb_screen_t *screen;

	xcb_special_event_t *special_ev;
	uint32_t special_ev_stamp;

	// Just one drawable
	struct drawable *drawable;

	uint32_t current_msc;

	int drm_fd;

	union {
		struct gbm_device *gbm;
#ifdef HAS_LIBDRM_ETNAVIV
		struct etna_device *etna_dev;
#endif
	};
};

struct drawable
{
	struct display *display;

	uint32_t width;
	uint32_t height;

	struct buffer *buffers[3];

	uint32_t current_idx;
};

struct buffer
{
	struct drawable *drawable;

	bool busy;

	xcb_pixmap_t pixmap;

	int dmabuf_fd;

	union {
		struct gbm_bo *gbm_bo;
#ifdef HAS_LIBDRM_ETNAVIV
		struct etna_bo *etna_bo;
#endif
	};
};

struct buf_ops
{
	void (*create_device)(struct display *display);
	void (*create_pixmap)(struct buffer *buffer);
	void (*destroy_pixmap)(struct buffer *buffer);
};

static const struct buf_ops *s_buf_ops;

#ifdef HAS_LIBDRM_ETNAVIV

static void create_etnaviv_device(struct display *display)
{
	display->etna_dev = etna_device_new(display->drm_fd);
	FAIL_IF(!display->etna_dev, "no etna device");
}

static void create_etnaviv_pixmap(struct buffer *buffer)
{
	struct drawable *drawable = buffer->drawable;
	struct display *display = drawable->display;
	struct etna_bo* bo;

	bo = etna_bo_new(display->etna_dev, drawable->width * drawable->height * 4, DRM_ETNA_GEM_CACHE_WC);
	FAIL_IF(!bo, "bo fail");

	int bo_fd = etna_bo_dmabuf(bo);
	FAIL_IF(bo_fd < 0, "dmabuf fail");

	uint32_t width = drawable->width;
	uint32_t height = drawable->height;
	uint32_t stride = width * 4;

	xcb_pixmap_t pixmap = xcb_generate_id(display->connection);
	xcb_void_cookie_t pixmap_cookie = xcb_dri3_pixmap_from_buffer_checked(display->connection, pixmap, display->screen->root,
									      stride * height,
									      width, height,
									      stride, 24, 32, bo_fd);
	xcb_generic_error_t *error;
	if ((error = xcb_request_check(display->connection, pixmap_cookie))) {
		FAIL("create pixmap failed");
	}

	buffer->etna_bo = bo;
	buffer->dmabuf_fd = bo_fd;
	buffer->pixmap = pixmap;
}

static void destroy_etnaviv_pixmap(struct buffer *buffer)
{
	xcb_free_pixmap(buffer->drawable->display->connection, buffer->pixmap);
	close(buffer->dmabuf_fd);
	etna_bo_del(buffer->etna_bo);
}

static const struct buf_ops etnaviv_buf_ops = {
	.create_device = create_etnaviv_device,
	.create_pixmap = create_etnaviv_pixmap,
	.destroy_pixmap = destroy_etnaviv_pixmap,
};
#endif

static void create_x11_device(struct display *display)
{

}

static void create_x11_pixmap(struct buffer *buffer)
{
	struct drawable *drawable = buffer->drawable;
	struct display *display = drawable->display;
	struct xcb_connection_t *c = display->connection;

	uint32_t width = drawable->width;
	uint32_t height = drawable->height;

	xcb_pixmap_t pixmap = xcb_generate_id(c);
	xcb_create_pixmap(c, 24, pixmap, display->window, width, height);

	buffer->pixmap = pixmap;
}

static void destroy_x11_pixmap(struct buffer *buffer)
{
	xcb_free_pixmap(buffer->drawable->display->connection, buffer->pixmap);
}

static const struct buf_ops x11_buf_ops = {
	.create_device = create_x11_device,
	.create_pixmap = create_x11_pixmap,
	.destroy_pixmap = destroy_x11_pixmap,
};

static void create_gbm_device(struct display *display)
{
	display->gbm = gbm_create_device(display->drm_fd);
}

static void create_gbm_pixmap(struct buffer *buffer)
{
	struct drawable *drawable = buffer->drawable;
	struct display *display = drawable->display;
	struct xcb_connection_t *c = display->connection;
	struct gbm_device *gbm = display->gbm;

	uint32_t width = drawable->width;
	uint32_t height = drawable->height;

	struct gbm_bo *bo = gbm_bo_create(gbm, width, height, GBM_FORMAT_XRGB8888, GBM_BO_USE_RENDERING);
	FAIL_IF(!bo, "no bo");

	uint32_t stride = gbm_bo_get_stride(bo);

	int bo_fd = gbm_bo_get_fd(bo);
	FAIL_IF(bo_fd < 0, "bad bo fd %d: %s\n", bo_fd, strerror(errno));

	printf("BO fd %d, %ux%u, stride %u\n", bo_fd, width, height, gbm_bo_get_stride(bo));

	xcb_pixmap_t pixmap = xcb_generate_id(c);

	xcb_void_cookie_t pixmap_cookie = xcb_dri3_pixmap_from_buffer_checked(c, pixmap, display->screen->root,
									      stride * height,
									      width, height,
									      stride, 24, 32, bo_fd);
	xcb_generic_error_t *error;
	if ((error = xcb_request_check(c, pixmap_cookie))) {
		FAIL("create pixmap failed");
	}

	buffer->dmabuf_fd = bo_fd;
	buffer->gbm_bo = bo;
	buffer->pixmap = pixmap;
}

static void destroy_gbm_pixmap(struct buffer *buffer)
{
	xcb_free_pixmap(buffer->drawable->display->connection, buffer->pixmap);
	close(buffer->dmabuf_fd);
	gbm_bo_destroy(buffer->gbm_bo);
}

static const struct buf_ops gbm_buf_ops = {
	.create_device = create_gbm_device,
	.create_pixmap = create_gbm_pixmap,
	.destroy_pixmap = destroy_gbm_pixmap,
};



static void check_dri3_ext(xcb_connection_t *c, xcb_screen_t *screen)
{
	xcb_prefetch_extension_data (c, &xcb_dri3_id);

	const xcb_query_extension_reply_t *extension =
			xcb_get_extension_data(c, &xcb_dri3_id);
	FAIL_IF (!(extension && extension->present), "No DRI3");

	xcb_dri3_query_version_cookie_t cookie =
			xcb_dri3_query_version(c, XCB_DRI3_MAJOR_VERSION, XCB_DRI3_MINOR_VERSION);
	xcb_dri3_query_version_reply_t *reply =
			xcb_dri3_query_version_reply(c, cookie, NULL);
	FAIL_IF(!reply, "xcb_dri3_query_version failed");
	printf("DRI3 %u.%u\n", reply->major_version, reply->minor_version);
	free(reply);
}

static void check_present_ext(xcb_connection_t *c, xcb_screen_t *screen)
{
	xcb_prefetch_extension_data (c, &xcb_present_id);

	const xcb_query_extension_reply_t *extension =
			xcb_get_extension_data(c, &xcb_present_id);
	FAIL_IF (!(extension && extension->present), "No present");

	xcb_present_query_version_cookie_t cookie =
			xcb_present_query_version(c, XCB_PRESENT_MAJOR_VERSION, XCB_PRESENT_MINOR_VERSION);
	xcb_present_query_version_reply_t *reply =
			xcb_present_query_version_reply(c, cookie, NULL);
	FAIL_IF(!reply, "xcb_present_query_version failed");
	printf("present %u.%u\n", reply->major_version, reply->minor_version);
	free(reply);
}

static int dri3_open(xcb_connection_t *c, xcb_screen_t *screen)
{
	xcb_dri3_open_cookie_t cookie =
			xcb_dri3_open(c, screen->root, 0);
	xcb_dri3_open_reply_t *reply =
			xcb_dri3_open_reply(c, cookie, NULL);
	FAIL_IF(!reply, "dri3 open failed");

	int nfds = reply->nfd;
	FAIL_IF(nfds != 1, "bad number of fds");

	int *fds = xcb_dri3_open_reply_fds(c, reply);

	int fd = fds[0];

	free(reply);

	fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);

	printf("DRM FD %d\n", fd);

	return fd;
}

static xcb_window_t create_window(xcb_connection_t *c, xcb_screen_t *screen, uint32_t width, uint32_t height)
{

	const uint32_t xcb_window_attrib_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
	const uint32_t xcb_window_attrib_list[] = {
		// OVERRIDE_REDIRECT
		screen->white_pixel,
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

	if (s_fullscreen) {
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

	return window;
}



static void draw_to_pixmap(xcb_connection_t *c, xcb_screen_t *screen, xcb_pixmap_t pixmap, uint32_t i)
{
	xcb_gcontext_t    foreground;
	uint32_t          mask;
	uint32_t          values[2];

	foreground = xcb_generate_id (c);
	mask = XCB_GC_FOREGROUND | XCB_GC_GRAPHICS_EXPOSURES;
	values[0] = screen->white_pixel;
	values[1] = 0;
	xcb_create_gc (c, foreground, screen->root, mask, values);

	xcb_rectangle_t rectangles[] = {
		{ 10 * i, 50, 40, 20},
		{ 80 * i, 50, 10, 40}};

	xcb_poly_rectangle (c, pixmap, foreground, 2, rectangles);
}

static struct buffer *create_buffer(struct drawable *drawable, int i)
{
	struct display *display = drawable->display;

	struct buffer *buffer = calloc(1, sizeof(struct buffer));
	buffer->drawable = drawable;

	s_buf_ops->create_pixmap(buffer);

	draw_to_pixmap(display->connection, display->screen, buffer->pixmap, i);

	return buffer;
}

static void destroy_buffer(struct buffer *buffer)
{
	s_buf_ops->destroy_pixmap(buffer);

	free(buffer);
}

static void create_buffers(struct drawable *drawable)
{
	for (int i = 0; i < 3; ++i) {
		if (drawable->buffers[i])
			destroy_buffer(drawable->buffers[i]);
	}

	for (int i = 0; i < 3; ++i) {
		drawable->buffers[i] = create_buffer(drawable, i);
	}
}

// media stamp counter (MSC)
// swap buffer count (SBC)
// unadjusted system time (UST)

static void present_next(struct drawable *drawable);

static void handle_event(struct display *display, xcb_present_generic_event_t *ge)
{
	struct drawable *drawable = display->drawable;

	switch (ge->evtype) {
	case XCB_PRESENT_COMPLETE_NOTIFY: {
		xcb_present_complete_notify_event_t *ce = (xcb_present_complete_notify_event_t*) ge;

		char *mode_str = "UNKNOWN";
		switch (ce->mode) {
		case XCB_PRESENT_COMPLETE_MODE_FLIP:
			mode_str = "FLIP";
			break;
		case XCB_PRESENT_COMPLETE_MODE_COPY:
			mode_str = "COPY";
			break;
		case XCB_PRESENT_COMPLETE_MODE_SKIP:
			mode_str = "SKIP";
			break;
		}

		printf("PRESENT COMPLETE NOTIFY %u, %s, msc %lu, ust %lu\n", ce->serial, mode_str, ce->msc, ce->ust);

		drawable->display->current_msc = ce->msc;

		present_next(drawable);

		break;
	}
	case XCB_PRESENT_EVENT_IDLE_NOTIFY: {
		xcb_present_idle_notify_event_t *ie = (xcb_present_idle_notify_event_t*) ge;

		printf("PRESENT IDLE NOTIFY %u\n", ie->serial);

		drawable->buffers[ie->serial]->busy = false;

		break;
	}

	case XCB_PRESENT_EVENT_CONFIGURE_NOTIFY: {
		xcb_present_configure_notify_event_t *ce = (xcb_present_configure_notify_event_t*) ge;
		printf("PRESENT CONFIGURE NOTIFY %ux%u\n", ce->width, ce->height);

		drawable->width = ce->width;
		drawable->height = ce->height;

		create_buffers(drawable);

		break;
	}

	case XCB_PRESENT_EVENT_REDIRECT_NOTIFY: {
		xcb_present_redirect_notify_event_t *re = (xcb_present_redirect_notify_event_t*) ge;
		printf("PRESENT REDIRECT NOTIFY %u\n", re->serial);

		break;
	}

	}

	free(ge);
}

static void poll_special_events(struct display *display)
{
	xcb_generic_event_t *ev;

	while ((ev = xcb_poll_for_special_event(display->connection, display->special_ev)))
		handle_event(display, (xcb_present_generic_event_t*)ev);
}

static void wait_special_event(struct display *display)
{
	xcb_generic_event_t *ev;

	ev = xcb_wait_for_special_event(display->connection, display->special_ev);
	handle_event(display, (xcb_present_generic_event_t*)ev);

	while ((ev = xcb_poll_for_special_event(display->connection, display->special_ev)))
		handle_event(display, (xcb_present_generic_event_t*)ev);
}

static void get_window_data(xcb_connection_t *c, xcb_window_t window, uint32_t *width, uint32_t *height)
{
	xcb_get_geometry_cookie_t cookie;
	xcb_get_geometry_reply_t *reply;

	cookie = xcb_get_geometry(c, window);
	reply = xcb_get_geometry_reply(c, cookie, NULL);
	FAIL_IF(!reply, "failed to get geometry");

	*width = reply->width;
	*height = reply->height;

	uint32_t depth = reply->depth;
	FAIL_IF(depth != 24 && depth != 32, "bad depth %d", depth);

	free(reply);
}

static struct display *init_display()
{
	Display* dpy = XOpenDisplay(NULL);
	FAIL_IF(!dpy, "Failed to connect to the X server");

	xcb_connection_t* c = XGetXCBConnection(dpy);
	FAIL_IF(!c, "Failed to get xcb connection");

	/* Acquire event queue ownership */
	//XSetEventQueueOwner(dpy, XCBOwnsEventQueue);

	/* Get the first screen */
	const xcb_setup_t* setup  = xcb_get_setup (c);
	xcb_screen_t* screen = xcb_setup_roots_iterator (setup).data;

	printf("Screen %ux%u\n", screen->width_in_pixels, screen->height_in_pixels);

	check_present_ext(c, screen);

	check_dri3_ext(c, screen);

	int drm_fd = dri3_open(c, screen);

	drmVersion* ver = drmGetVersion(drm_fd);
	printf("driver %s (%s)\n", ver->name, ver->desc);


	uint32_t width;
	uint32_t height;

	if (s_fullscreen) {
		width = screen->width_in_pixels;
		height = screen->height_in_pixels;
	} else {
		width = 300;
		height = 300;
	}

	xcb_window_t window = create_window(c, screen, width, height);

	struct display *display = calloc(1, sizeof(struct display));

	display->connection = c;
	display->screen = screen;
	display->window = window;
	display->drm_fd = drm_fd;

	s_buf_ops->create_device(display);

	return display;
}

static xcb_special_event_t *init_special_event_queue(struct display *display, uint32_t *special_ev_stamp)
{
	xcb_connection_t *c = display->connection;

	uint32_t id = xcb_generate_id(c);
	xcb_void_cookie_t cookie;

	cookie = xcb_present_select_input_checked(c, id, display->window,
						  XCB_PRESENT_EVENT_MASK_COMPLETE_NOTIFY |
						  XCB_PRESENT_EVENT_MASK_IDLE_NOTIFY |
						  XCB_PRESENT_EVENT_MASK_CONFIGURE_NOTIFY |
						  XCB_PRESENT_EVENT_MASK_REDIRECT_NOTIFY);
	xcb_generic_error_t *error =
			xcb_request_check(c, cookie);
	FAIL_IF(error, "req xge failed");

	xcb_special_event_t *special_ev = xcb_register_for_special_xge(c, &xcb_present_id, id, special_ev_stamp);
	FAIL_IF(!special_ev, "no special ev");

	return special_ev;
}

static struct drawable *create_drawable(struct display *display)
{
	struct drawable *drawable = calloc(1, sizeof(struct drawable));

	drawable->display = display;

	return drawable;
}

static void present_next(struct drawable *drawable)
{
	struct xcb_connection_t *c = drawable->display->connection;

	printf("present %u\n", drawable->current_idx);

	uint32_t idx = drawable->current_idx;
	drawable->current_idx = (drawable->current_idx + 1) % 3;

	struct buffer *buffer = drawable->buffers[idx];

	while (buffer->busy) {
		printf("BUF %u BUSY, wait\n", idx);
		wait_special_event(drawable->display);
	}

	buffer->busy = true;

	xcb_pixmap_t pixmap = buffer->pixmap;

	uint32_t options = XCB_PRESENT_OPTION_NONE;
	//if (swap_interval == 0)
	//options |= XCB_PRESENT_OPTION_ASYNC;
	//if (force_copy)
	//options |= XCB_PRESENT_OPTION_COPY;

	uint32_t serial = idx;
	uint32_t target_msc = 0;
	uint32_t divisor = 3;
	uint32_t remainder = idx;

	//divisor = remainder = 0;
	//target_msc = display->current_msc + 100;

	xcb_void_cookie_t cookie;
	cookie = xcb_present_pixmap_checked(c,
					    drawable->display->window,
					    pixmap,
					    serial,
					    0, 0, 0, 0, // valid, update, x_off, y_off
					    None, /* target_crtc */
					    None, /* wait fence */
					    None, /* idle fence */
					    options,
					    target_msc,
					    divisor,
					    remainder,
					    0, /* notifiers len */
					    NULL); /* notifiers */

	xcb_generic_error_t *error;
	if ((error = xcb_request_check(c, cookie))) {
		FAIL("present pixmap failed");
	}

	xcb_flush (c);
}

static void main_loop(struct display *display)
{
	xcb_connection_t *c = display->connection;
	struct drawable *drawable = display->drawable;

	xcb_generic_event_t *e;

	while ((e = xcb_wait_for_event(c))) {
		bool done = false;

		switch (e->response_type & ~0x80) {
		case XCB_EXPOSE: {
			//xcb_expose_event_t* ee = (xcb_expose_event_t *)e;
			printf("EXPOSE\n");
			done = true;
			break;
		}

		default:
			printf("Unknown X event %x\n", e->response_type);
			break;
		}

		free(e);

		if (done)
			break;
	}

	// present first frame
	present_next(drawable);

	while (true) {
		poll_special_events(display);

		while ((e = xcb_poll_for_event(c))) {
			printf("X event %u\n", e->response_type);

			switch (e->response_type & ~0x80) {
			case XCB_KEY_PRESS: {
				xcb_key_press_event_t *kpe = (xcb_key_press_event_t*)e;

				if (kpe->detail == 9)
					exit(0);

				break;
			}
			}

			free(e);
		}
	}
}

int main(int argc, char **argv)
{
	s_buf_ops = &gbm_buf_ops;

	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "-f") == 0)
			s_fullscreen = true;
		else if (strcmp(argv[i], "gbm") == 0)
			s_buf_ops = &gbm_buf_ops;
		else if (strcmp(argv[i], "x11") == 0)
			s_buf_ops = &x11_buf_ops;
#ifdef HAS_LIBDRM_ETNAVIV
		else if (strcmp(argv[i], "etna"))
			s_buf_ops = &etnaviv_buf_ops;
#endif
		else {
			printf("unknown param %s\n", argv[i]);
			exit(-1);
		}
	}

	struct display *display = init_display();

	/* setup special event queue */
	display->special_ev = init_special_event_queue(display, &display->special_ev_stamp);

	display->drawable = create_drawable(display);

	/* setup initial buffers */
	struct drawable *drawable = display->drawable;

	uint32_t width, height;
	get_window_data(display->connection, display->window, &width, &height);

	drawable->width = width;
	drawable->height = height;

	create_buffers(drawable);

	xcb_flush (display->connection);

	main_loop(display);
}
