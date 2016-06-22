#include <cstdio>
#include <poll.h>
#include <unistd.h>
#include <algorithm>
#include <fstream>
#include <map>
#include <system_error>

#include <kms++/kms++.h>
#include <kms++util/kms++util.h>
#include <kms++util/videodevice.h>

const uint32_t NUM_SRC_BUFS=2;
const uint32_t NUM_DST_BUFS=2;

using namespace std;
using namespace kms;

static const char* usage_str =
		"Usage: wbm2m [OPTIONS]\n\n"
		"Options:\n"
		"  -f, --format=4CC          Output format"
		"  -h, --help                Print this help\n"
		;

const int bar_speed = 4;
const int bar_width = 10;

static unsigned get_bar_pos(DumbFramebuffer* fb, unsigned frame_num)
{
	return (frame_num * bar_speed) % (fb->width() - bar_width + 1);
}

static void read_frame(DumbFramebuffer* fb, unsigned frame_num)
{
	static map<DumbFramebuffer*, int> s_bar_pos_map;

	int old_pos = -1;
	if (s_bar_pos_map.find(fb) != s_bar_pos_map.end())
		old_pos = s_bar_pos_map[fb];

	int pos = get_bar_pos(fb, frame_num);
	draw_color_bar(*fb, old_pos, pos, bar_width);
	draw_text(*fb, fb->width() / 2, 0, to_string(frame_num), RGB(255, 255, 255));
	s_bar_pos_map[fb] = pos;
}

int main(int argc, char** argv)
{
	// XXX get from args
	const uint32_t src_width = 800;
	const uint32_t src_height = 480;
	const auto src_fmt = PixelFormat::XRGB8888;
	const uint32_t num_src_frames = 10;

	const uint32_t dst_width = 800;
	const uint32_t dst_height = 480;
	auto dst_fmt = PixelFormat::XRGB8888;

	const string filename = "wb-out.raw";

	OptionSet optionset = {
		Option("f|format=", [&](string s)
		{
			dst_fmt = FourCCToPixelFormat(s);
		}),
		Option("h|help", [&]()
		{
			puts(usage_str);
			exit(-1);
		}),
	};

	optionset.parse(argc, argv);

	if (optionset.params().size() > 0) {
		puts(usage_str);
		exit(-1);
	}

	VideoDevice vid("/dev/video10");

	Card card;

	uint32_t src_frame_num = 0;
	uint32_t dst_frame_num = 0;

	VideoStreamer* out = vid.get_output_streamer();
	VideoStreamer* in = vid.get_capture_streamer();

	out->set_format(src_fmt, src_width, src_height);
	in->set_format(dst_fmt, dst_width, dst_height);

	out->set_queue_size(NUM_SRC_BUFS);
	in->set_queue_size(NUM_DST_BUFS);


	for (unsigned i = 0; i < min(NUM_SRC_BUFS, num_src_frames); ++i) {
		auto fb = new DumbFramebuffer(card, src_width, src_height, src_fmt);

		read_frame(fb, src_frame_num++);

		out->queue(fb);
	}

	for (unsigned i = 0; i < min(NUM_DST_BUFS, num_src_frames); ++i) {
		auto fb = new DumbFramebuffer(card, dst_width, dst_height, dst_fmt);
		in->queue(fb);
	}

	vector<pollfd> fds(3);

	fds[0].fd = 0;
	fds[0].events =  POLLIN;
	fds[1].fd = vid.fd();
	fds[1].events =  POLLIN;
	fds[2].fd = card.fd();
	fds[2].events =  POLLIN;

	ofstream os(filename, ofstream::binary);

	out->stream_on();
	in->stream_on();

	while (true) {
		int r = poll(fds.data(), fds.size(), -1);
		ASSERT(r > 0);

		if (fds[0].revents != 0)
			break;

		if (fds[1].revents) {
			fds[1].revents = 0;


			try {
				DumbFramebuffer *dst_fb = in->dequeue();
				printf("Writing frame %u\n", dst_frame_num);
				for (unsigned i = 0; i < dst_fb->num_planes(); ++i)
					os.write((char*)dst_fb->map(i), dst_fb->size(i));
				in->queue(dst_fb);

				dst_frame_num++;

				if (dst_frame_num >= num_src_frames)
					break;

			} catch (system_error& se) {
				if (se.code() != errc::resource_unavailable_try_again)
					FAIL("dequeue failed: %s", se.what());

				break;
			}

			DumbFramebuffer *src_fb = out->dequeue();

			if (src_frame_num < num_src_frames) {
				read_frame(src_fb, src_frame_num++);
				out->queue(src_fb);
			}
		}

		if (fds[2].revents) {
			fds[2].revents = 0;
		}
	}

	printf("exiting...\n");
}
