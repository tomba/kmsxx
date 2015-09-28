#include <cstdio>
#include <algorithm>

#include "kms++.h"
#include "utils/testpat.h"

#include "test.h"

using namespace std;
using namespace kms;

int main()
{
	Card card;

	if (card.master() == false)
		printf("Not DRM master, modeset may fail\n");

	//card.print_short();

	auto pipes = card.get_connected_pipelines();

	vector<Framebuffer*> fbs;

	for (auto pipe : pipes)
	{
		auto conn = pipe.connector;
		auto crtc = pipe.crtc;

		// RG16 XR24 UYVY YUYV NV12

		auto mode = conn->get_default_mode();

		auto fb = new Framebuffer(card, mode.hdisplay, mode.vdisplay, "XR24");
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
			auto planefb = new Framebuffer(card, 400, 400, "YUYV");
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
