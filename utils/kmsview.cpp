#include <cstdio>
#include <fstream>
#include <unistd.h>

#include <kms++/kms++.h>
#include <kms++util/kms++util.h>

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

static const char* usage_str =
		"Usage: kmsview [options] <file> <width> <height> <fourcc>\n\n"
		"Options:\n"
		"  -c, --connector <name>	Output connector\n"
		"  -t, --time <ms>		Milliseconds to sleep between frames\n"
		;

static void usage()
{
	puts(usage_str);
}

int main(int argc, char** argv)
{
	uint32_t time = 0;
	string dev_path;
	string conn_name;

	OptionSet optionset = {
		Option("c|connector=", [&conn_name](string s)
		{
			conn_name = s;
		}),
		Option("|device=", [&dev_path](string s)
		{
			dev_path = s;
		}),
		Option("t|time=", [&time](const string& str)
		{
			time = stoul(str);
		}),
		Option("h|help", []()
		{
			usage();
			exit(-1);
		}),
	};

	optionset.parse(argc, argv);

	vector<string> params = optionset.params();

	if (params.size() != 4) {
		usage();
		exit(-1);
	}

	string filename = params[0];
	uint32_t w = stoi(params[1]);
	uint32_t h = stoi(params[2]);
	string modestr = params[3];

	auto pixfmt = FourCCToPixelFormat(modestr);

	ifstream is(filename, ifstream::binary);

	is.seekg(0, std::ios::end);
	unsigned fsize = is.tellg();
	is.seekg(0);


	Card card(dev_path);
	ResourceManager res(card);

	auto conn = res.reserve_connector(conn_name);
	auto crtc = res.reserve_crtc(conn);
	auto plane = res.reserve_overlay_plane(crtc, pixfmt);
	FAIL_IF(!plane, "available plane not found");

	auto fb = new DumbFramebuffer(card, w, h, pixfmt);

	unsigned frame_size = 0;
	for (unsigned i = 0; i < fb->num_planes(); ++i)
		frame_size += fb->size(i);

	unsigned num_frames = fsize / frame_size;
	printf("file size %u, frame size %u, frames %u\n", fsize, frame_size, num_frames);

	for (unsigned i = 0; i < num_frames; ++i) {
		printf("frame %d", i); fflush(stdout);
		read_frame(is, fb, crtc, plane);
		if (!time) {
			getchar();
		} else {
			usleep(time * 1000);
			printf("\n");
		}
	}

	is.close();

	if (time) {
		printf("press enter to exit\n");
		getchar();
	}

	delete fb;
}
