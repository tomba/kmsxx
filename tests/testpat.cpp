#include <cstdio>
#include <algorithm>

#include "kms++.h"

#include "test.h"
#include "cmdoptions.h"

using namespace std;
using namespace kms;

static map<string, CmdOption> options = {
	{ "m", HAS_PARAM("Set display mode, for example 1920x1080") },
};

int main(int argc, char **argv)
{
	Card card;
	CmdOptions opts(argc, argv, options);

	if (card.master() == false)
		printf("Not DRM master, modeset may fail\n");

	auto pipes = card.get_connected_pipelines();

	vector<Framebuffer*> fbs;

	for (auto pipe : pipes)
	{
		auto conn = pipe.connector;
		auto crtc = pipe.crtc;

		// RG16 XR24 UYVY YUYV NV12

		auto mode = conn->get_default_mode();

		if (opts.is_set("m"))
			mode = conn->get_mode(opts.opt_param("m"));

		auto fb = new DumbFramebuffer(card, mode.hdisplay, mode.vdisplay, PixelFormat::XRGB8888);
		draw_test_pattern(*fb);
		fbs.push_back(fb);

		printf("conn %u, crtc %u, fb %u\n", conn->id(), crtc->id(), fb->id());

		int r = crtc->set_mode(conn, *fb, mode);
		ASSERT(r == 0);
	}

	for (auto pipe: pipes)
	{
		auto crtc = pipe.crtc;

		Plane* plane = 0;

		for (Plane* p : crtc->get_possible_planes()) {
			if (p->plane_type() == PlaneType::Overlay) {
				plane = p;
				break;
			}
		}

		if (plane) {
			auto planefb = new DumbFramebuffer(card, 400, 400, PixelFormat::YUYV);
			draw_test_pattern(*planefb);
			fbs.push_back(planefb);

			int r = crtc->set_plane(plane, *planefb,
					    0, 0, planefb->width(), planefb->height(),
					    0, 0, planefb->width(), planefb->height());

			ASSERT(r == 0);
		}
	}

	printf("press enter to exit\n");

	getchar();

	for(auto fb : fbs)
		delete fb;
}
