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

static void setup_buf(struct omap_wb_buffer *buf, DumbFramebuffer* fb)
{
	buf->fourcc = (uint32_t)fb->format();
	buf->x = 0;
	buf->y = 0;
	buf->width = fb->width();
	buf->height = fb->height();

	buf->num_planes = fb->num_planes();

	for (unsigned i = 0; i < fb->num_planes(); ++i) {
		buf->plane[i].fd = fb->prime_fd(i);
		buf->plane[i].pitch = fb->stride(i);
	}
}

int main(int argc, char **argv)
{
	Card card;

	if (card.master() == false)
		printf("Not DRM master, modeset may fail\n");

	//card.print_short();

	auto conn = card.get_first_connected_connector();
	auto crtc = conn->get_current_crtc();

	unsigned num_srcs = 2;

	DumbFramebuffer* srcfbs[num_srcs] = {0};


	srcfbs[0] = new DumbFramebuffer(card, 1920/2, 1200, PixelFormat::XRGB8888);
	draw_test_pattern(*srcfbs[0]);

	srcfbs[1] = new DumbFramebuffer(card, 1920/2, 1200/2, PixelFormat::XRGB8888);
	draw_test_pattern(*srcfbs[1]);

	//ifstream is("src.data", ifstream::binary);
	//is.read((char*)srcfb->map(0), srcfb->size(0));

	int r;

	int wbfd = open("/dev/omap_wb", O_RDWR);

	struct omap_wb_convert_info conv_cmd = { };

	conv_cmd.wb_mode = OMAP_WB_MEM2MEM_MGR;

	conv_cmd.num_srcs = num_srcs;

	conv_cmd.src[0].pipe = OMAP_WB_VIDEO3;
	conv_cmd.src[1].pipe = OMAP_WB_VIDEO2;

	for (unsigned i = 0; i < num_srcs; ++i)
		setup_buf(&conv_cmd.src[i], srcfbs[i]);



	conv_cmd.wb_channel = OMAP_WB_CHANNEL_LCD3;
	conv_cmd.channel_width = 1920;
	conv_cmd.channel_height = 1200;



	auto dstfb = new DumbFramebuffer(card, 1920, 1200, PixelFormat::NV12);

	setup_buf(&conv_cmd.dst, dstfb);

	r = ioctl(wbfd, OMAP_WB_CONVERT, &conv_cmd);
	if (r < 0)
		printf("wb o/p: wb_convert failed: %d\n", errno);


	usleep(250000);




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

	for (unsigned i = 0; i < num_srcs; ++i)
		if (srcfbs[i])
			delete srcfbs[i];
}
