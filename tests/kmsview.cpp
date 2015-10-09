#include <cstdio>
#include <fstream>

#include "kms++.h"

#include "test.h"

using namespace std;
using namespace kms;

int main(int argc, char** argv)
{
	if (argc != 5) {
		printf("Usage: %s <file> <width> <height> <fourcc>\n", argv[0]);
		return -1;
	}

	string filename = argv[1];
	uint32_t w = stoi(argv[2]);
	uint32_t h = stoi(argv[3]);
	string modestr = argv[4];

	auto pixfmt = FourCCToPixelFormat(modestr);

	Card card;

	auto conn = card.get_first_connected_connector();
	auto crtc = conn->get_current_crtc();

	auto fb = new DumbFramebuffer(card, w, h, pixfmt);

	ifstream is(filename, ifstream::binary);
	is.read((char*)fb->map(0), fb->size(0));
	is.close();

	Plane* plane = 0;

	for (Plane* p : crtc->get_possible_planes()) {
		if (p->plane_type() != PlaneType::Overlay)
			continue;

		if (!p->supports_format(pixfmt))
			continue;

		plane = p;
		break;
	}

	FAIL_IF(!plane, "available plane not found");

	int r = crtc->set_plane(plane, *fb,
				0, 0, w, h,
				0, 0, w, h);

	ASSERT(r == 0);

	printf("press enter to exit\n");

	getchar();

	delete fb;
}
