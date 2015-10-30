#include <cstdio>
#include <algorithm>

#include "kms++.h"

#include "test.h"

#include "omapwb.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <xf86drm.h>

#include <fstream>
#include <istream>

using namespace std;
using namespace kms;

static void setup_ovl(struct omap_wb_buffer *buf, DumbFramebuffer* fb,
		      uint32_t x, uint32_t y,
		      uint32_t out_w, uint32_t out_h)
{
	buf->fourcc = (uint32_t)fb->format();
	buf->x_pos = x;
	buf->y_pos = y;
	buf->width = fb->width();
	buf->height = fb->height();
	buf->out_width = out_w;
	buf->out_height = out_h;

	buf->num_planes = fb->num_planes();

	for (unsigned i = 0; i < fb->num_planes(); ++i) {
		buf->plane[i].fd = fb->prime_fd(i);
		buf->plane[i].pitch = fb->stride(i);
	}
}

static void setup_wb_ovl(struct omap_wb_buffer *buf, DumbFramebuffer* fb)
{
	buf->fourcc = (uint32_t)fb->format();
	buf->x_pos = 0;
	buf->y_pos = 0;
	buf->width = fb->width();
	buf->height = fb->height();
	buf->out_width = 0;
	buf->out_height = 0;

	buf->num_planes = fb->num_planes();

	for (unsigned i = 0; i < fb->num_planes(); ++i) {
		buf->plane[i].fd = fb->prime_fd(i);
		buf->plane[i].pitch = fb->stride(i);
	}
}

void read_raw_image(DumbFramebuffer *targetfb)
{
	ifstream is("src.data", ifstream::binary);
	is.read((char*)targetfb->map(0), targetfb->size(0));
}

int main(int argc, char **argv)
{
	Card card;

	auto conn = card.get_first_connected_connector();
	auto crtc = conn->get_current_crtc();

	int wbfd = open("/dev/omap_wb", O_RDWR);

	struct omap_wb_convert_info conv_cmd = { };

	conv_cmd.wb_mode = OMAP_WB_MEM2MEM_MGR;

	unsigned num_srcs = 0;

	{
		struct omap_wb_buffer& src = conv_cmd.src[num_srcs];

		auto fb = new DumbFramebuffer(card, 1920/2, 1200/2, PixelFormat::XRGB8888);
		draw_test_pattern(*fb);

		src.pipe = OMAP_WB_VIDEO3;

		setup_ovl(&src, fb,
			  100, 50,
			  fb->width(), fb->height());

		num_srcs++;
	}

#if 1
	{
		struct omap_wb_buffer& src = conv_cmd.src[num_srcs];

		auto fb = new DumbFramebuffer(card, 1920/2, 1200/2, PixelFormat::XRGB8888);
		draw_test_pattern(*fb);

		src.pipe = OMAP_WB_VIDEO2;

		setup_ovl(&src, fb,
			  200, 150,
			  fb->width(), fb->height());

		num_srcs++;
	}
#endif

	conv_cmd.num_srcs = num_srcs;


	{
		// Setup ovl manager
		// these are ignored for OVL mode
		conv_cmd.wb_channel = OMAP_WB_CHANNEL_LCD3;
		conv_cmd.channel_width = 1920;
		conv_cmd.channel_height = 1200;
	}



	auto dstfb = new DumbFramebuffer(card, 1920, 1200, PixelFormat::NV12);

	setup_wb_ovl(&conv_cmd.dst, dstfb);

	int r;

	r = ioctl(wbfd, OMAP_WB_CONVERT, &conv_cmd);
	if (r < 0)
		printf("wb o/p: wb_convert failed: %d\n", errno);



	usleep(250000);


	/* show the result */

	Plane* plane = 0;

	for (Plane* p : crtc->get_possible_planes()) {
		if (p->plane_type() == PlaneType::Overlay) {
			plane = p;
			break;
		}
	}

	r = crtc->set_plane(plane, *dstfb,
			    0, 0, dstfb->width(), dstfb->height(),
			    0, 0, dstfb->width(), dstfb->height());
	ASSERT(r == 0);



	printf("press enter to exit\n");

	getchar();
}
