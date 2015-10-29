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

enum omap_plane {
	OMAP_DSS_GFX	= 0,
	OMAP_DSS_VIDEO1	= 1,
	OMAP_DSS_VIDEO2	= 2,
	OMAP_DSS_VIDEO3	= 3,
	OMAP_DSS_WB	= 4,
};

int main(int argc, char **argv)
{
	Card card;

	if (card.master() == false)
		printf("Not DRM master, modeset may fail\n");

	//card.print_short();

	auto conn = card.get_first_connected_connector();
	auto crtc = conn->get_current_crtc();

	Plane* plane = 0;

	for (Plane* p : crtc->get_possible_planes()) {
		if (p->plane_type() == PlaneType::Overlay) {
			plane = p;
			break;
		}
	}

	auto srcfb = new DumbFramebuffer(card, 1920, 1200, PixelFormat::XRGB8888);
	draw_test_pattern(*srcfb);

	//ifstream is("src.data", ifstream::binary);
	//is.read((char*)srcfb->map(0), srcfb->size(0));

	auto dstfb = new DumbFramebuffer(card, 1920, 1200, PixelFormat::NV12);

	int r;

	r = crtc->set_plane(plane, *srcfb,
			    0, 0, srcfb->width(), srcfb->height(),
			    0, 0, srcfb->width(), srcfb->height());
	ASSERT(r == 0);

	usleep(250*1000);

	r = crtc->set_plane(plane, *dstfb,
			    0, 0, dstfb->width(), dstfb->height(),
			    0, 0, dstfb->width(), dstfb->height());
	ASSERT(r == 0);

	int srcfds[2], dstfds[2];

	r = drmPrimeHandleToFD(card.fd(), srcfb->handle(0), DRM_CLOEXEC, &srcfds[0]);
	ASSERT(r == 0);

	if (srcfb->num_planes() > 1) {
		r = drmPrimeHandleToFD(card.fd(), srcfb->handle(1), DRM_CLOEXEC, &srcfds[1]);
		ASSERT(r == 0);
	}

	r = drmPrimeHandleToFD(card.fd(), dstfb->handle(0), DRM_CLOEXEC, &dstfds[0]);
	ASSERT(r == 0);

	if (dstfb->num_planes() > 1) {
		r = drmPrimeHandleToFD(card.fd(), dstfb->handle(1), DRM_CLOEXEC, &dstfds[1]);
		ASSERT(r == 0);
	}


	int wbfd = open("/dev/omap_wb", O_RDWR);

	struct omap_wb_convert_info conv_cmd = { };

	conv_cmd.src.pipe = OMAP_DSS_VIDEO3;
	conv_cmd.src.fourcc = (uint32_t)srcfb->format();
	conv_cmd.src.num_planes = srcfb->num_planes();
	conv_cmd.src.plane[0].fd = srcfds[0];
	conv_cmd.src.plane[0].win.x = 0;
	conv_cmd.src.plane[0].win.y = 0;
	conv_cmd.src.plane[0].win.w = srcfb->width();
	conv_cmd.src.plane[0].win.h = srcfb->height();
	conv_cmd.src.plane[0].pitch = srcfb->stride(0);

	if (srcfb->num_planes() > 1) {
		conv_cmd.src.plane[1].fd = srcfds[1];
		conv_cmd.src.plane[1].win.x = 0;
		conv_cmd.src.plane[1].win.y = 0;
		conv_cmd.src.plane[1].win.w = srcfb->width();
		conv_cmd.src.plane[1].win.h = srcfb->height();
		conv_cmd.src.plane[1].pitch = srcfb->stride(1);
	}

	conv_cmd.dst.fourcc = (uint32_t)dstfb->format();
	conv_cmd.dst.num_planes = dstfb->num_planes();
	conv_cmd.dst.plane[0].fd = dstfds[0];
	conv_cmd.dst.plane[0].win.x = 0;
	conv_cmd.dst.plane[0].win.y = 0;
	conv_cmd.dst.plane[0].win.w = dstfb->width();
	conv_cmd.dst.plane[0].win.h = dstfb->height();
	conv_cmd.dst.plane[0].pitch = dstfb->stride(0);

	if (dstfb->num_planes() > 1) {
		conv_cmd.dst.plane[1].fd = dstfds[1];
		conv_cmd.dst.plane[1].win.x = 0;
		conv_cmd.dst.plane[1].win.y = 0;
		conv_cmd.dst.plane[1].win.w = dstfb->width();
		conv_cmd.dst.plane[1].win.h = dstfb->height();
		conv_cmd.dst.plane[1].pitch = dstfb->stride(1);
	}

	r = ioctl(wbfd, OMAP_WB_CONVERT, &conv_cmd);
	if (r < 0)
		printf("wb o/p: wb_convert failed: %d\n", errno);

	printf("press enter to exit\n");

	getchar();

	delete srcfb;
}
