#include <cstdio>
#include <fstream>
#include <unistd.h>

#include "kms++.h"

#include "test.h"

using namespace std;
using namespace kms;

static void read_frame(ifstream& is, DumbFramebuffer* fb, Crtc* crtc, Plane* plane)
{
	for (unsigned i = 0; i < fb->num_planes(); ++i)
		is.read((char*)fb->map(i), fb->size(i));

	unsigned w = min(crtc->width(), fb->width());
	unsigned h = min(crtc->height(), fb->height());

	int r = crtc->set_plane(plane, *fb,
				0, 0, w, h,
				0, 0, fb->width(), fb->height());

	ASSERT(r == 0);
}

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


	ifstream is(filename, ifstream::binary);

	is.seekg(0, std::ios::end);
	unsigned fsize = is.tellg();
	is.seekg(0);


	Card card;

	auto conn = card.get_first_connected_connector();
	auto crtc = conn->get_current_crtc();

	auto fb = new DumbFramebuffer(card, w, h, pixfmt);

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


	unsigned frame_size = 0;
	for (unsigned i = 0; i < fb->num_planes(); ++i)
		frame_size += fb->size(i);

	unsigned num_frames = fsize / frame_size;
	printf("file size %u, frame size %u, frames %u\n", fsize, frame_size, num_frames);

	for (unsigned i = 0; i < num_frames; ++i) {
		printf("frame %d\n", i);
		read_frame(is, fb, crtc, plane);
		usleep(1000*50);
	}

	printf("press enter to exit\n");

	is.close();

	getchar();

	delete fb;
}
