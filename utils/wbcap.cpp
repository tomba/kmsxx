#include <cstdio>
#include <poll.h>
#include <unistd.h>
#include <algorithm>

#include <kms++/kms++.h>
#include <kms++util/kms++util.h>
#include <kms++util/videodevice.h>

#define CAMERA_BUF_QUEUE_SIZE 5

using namespace std;
using namespace kms;

static vector<DumbFramebuffer*> s_fbs;
static vector<DumbFramebuffer*> s_free_fbs;
static vector<DumbFramebuffer*> s_wb_fbs;
static vector<DumbFramebuffer*> s_ready_fbs;

class WBStreamer
{
public:
	WBStreamer(VideoStreamer* streamer, Crtc* crtc, uint32_t width, uint32_t height, PixelFormat pixfmt)
		: m_capdev(*streamer)
	{
		m_capdev.set_port(crtc->idx());
		m_capdev.set_format(pixfmt, width, height);
		m_capdev.set_queue_size(s_fbs.size());

		for (auto fb : s_free_fbs) {
			m_capdev.queue(fb);
			s_wb_fbs.push_back(fb);
		}

		s_free_fbs.clear();
	}

	~WBStreamer()
	{
	}

	WBStreamer(const WBStreamer& other) = delete;
	WBStreamer& operator=(const WBStreamer& other) = delete;

	int fd() const { return m_capdev.fd(); }

	void start_streaming()
	{
		m_capdev.stream_on();
	}

	void stop_streaming()
	{
		m_capdev.stream_off();
	}

	void Dequeue()
	{
		auto fb = m_capdev.dequeue();

		auto iter = find(s_wb_fbs.begin(), s_wb_fbs.end(), fb);
		s_wb_fbs.erase(iter);

		s_ready_fbs.insert(s_ready_fbs.begin(), fb);
	}

	void Queue()
	{
		if (s_free_fbs.size() == 0)
			return;

		auto fb = s_free_fbs.back();
		s_free_fbs.pop_back();

		m_capdev.queue(fb);

		s_wb_fbs.insert(s_wb_fbs.begin(), fb);
	}

private:
	VideoStreamer& m_capdev;
};

class WBFlipState : private PageFlipHandlerBase
{
public:
	WBFlipState(Card& card, Crtc* crtc, Plane* plane)
		: m_card(card), m_crtc(crtc), m_plane(plane)
	{
	}

	void setup(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
	{
		auto fb = s_ready_fbs.back();
		s_ready_fbs.pop_back();

		AtomicReq req(m_card);

		req.add(m_plane, "CRTC_ID", m_crtc->id());
		req.add(m_plane, "FB_ID", fb->id());

		req.add(m_plane, "CRTC_X", x);
		req.add(m_plane, "CRTC_Y", y);
		req.add(m_plane, "CRTC_W", width);
		req.add(m_plane, "CRTC_H", height);

		req.add(m_plane, "SRC_X", 0);
		req.add(m_plane, "SRC_Y", 0);
		req.add(m_plane, "SRC_W", fb->width() << 16);
		req.add(m_plane, "SRC_H", fb->height() << 16);

		int r = req.commit_sync();
		FAIL_IF(r, "initial plane setup failed");

		m_current_fb = fb;
	}

	void queue_next()
	{
		if (m_queued_fb)
			return;

		if (s_ready_fbs.size() == 0)
			return;

		auto fb = s_ready_fbs.back();
		s_ready_fbs.pop_back();

		AtomicReq req(m_card);
		req.add(m_plane, "FB_ID", fb->id());

		int r = req.commit(this);
		if (r)
			EXIT("Flip commit failed: %d\n", r);

		m_queued_fb = fb;
	}

private:
	void handle_page_flip(uint32_t frame, double time)
	{
		if (m_queued_fb) {
			if (m_current_fb)
				s_free_fbs.insert(s_free_fbs.begin(), m_current_fb);

			m_current_fb = m_queued_fb;
			m_queued_fb = nullptr;
		}

		queue_next();
	}

	Card& m_card;
	Crtc* m_crtc;
	Plane* m_plane;

	DumbFramebuffer* m_current_fb = nullptr;
	DumbFramebuffer* m_queued_fb = nullptr;
};

class BarFlipState : private PageFlipHandlerBase
{
public:
	BarFlipState(Card& card, Crtc* crtc)
		: m_card(card), m_crtc(crtc)
	{
		m_plane = m_crtc->get_primary_plane();

		uint32_t w = m_crtc->mode().hdisplay;
		uint32_t h = m_crtc->mode().vdisplay;

		for (unsigned i = 0; i < s_num_buffers; ++i)
			m_fbs[i] = new DumbFramebuffer(card, w, h, PixelFormat::XRGB8888);
	}

	~BarFlipState()
	{
		for (unsigned i = 0; i < s_num_buffers; ++i)
			delete m_fbs[i];
	}

	void start_flipping()
	{
		m_frame_num = 0;
		queue_next();
	}

private:
	void handle_page_flip(uint32_t frame, double time)
	{
		m_frame_num++;
		queue_next();
	}

	static unsigned get_bar_pos(DumbFramebuffer* fb, unsigned frame_num)
	{
		return (frame_num * bar_speed) % (fb->width() - bar_width + 1);
	}

	void draw_bar(DumbFramebuffer* fb, unsigned frame_num)
	{
		int old_xpos = frame_num < s_num_buffers ? -1 : get_bar_pos(fb, frame_num - s_num_buffers);
		int new_xpos = get_bar_pos(fb, frame_num);

		draw_color_bar(*fb, old_xpos, new_xpos, bar_width);
		draw_text(*fb, fb->width() / 2, 0, to_string(frame_num), RGB(255, 255, 255));
	}

	void queue_next()
	{
		AtomicReq req(m_card);

		unsigned cur = m_frame_num % s_num_buffers;

		auto fb = m_fbs[cur];

		draw_bar(fb, m_frame_num);

		req.add(m_plane, {
				{ "FB_ID", fb->id() },
			});

		int r = req.commit(this);
		if (r)
			EXIT("Flip commit failed: %d\n", r);
	}

	static const unsigned s_num_buffers = 3;

	DumbFramebuffer* m_fbs[s_num_buffers];

	Card& m_card;
	Crtc* m_crtc;
	Plane* m_plane;

	unsigned m_frame_num;

	static const unsigned bar_width = 20;
	static const unsigned bar_speed = 8;
};

static const char* usage_str =
		"Usage: wbcap [OPTIONS]\n\n"
		"Options:\n"
		"  -s, --src=CONN            Source connector\n"
		"  -d, --dst=CONN            Destination connector\n"
		"  -f, --format=4CC          Format"
		"  -h, --help                Print this help\n"
		;

int main(int argc, char** argv)
{
	string src_conn_name = "unknown";
	string dst_conn_name = "hdmi";
	PixelFormat pixfmt = PixelFormat::XRGB8888;

	OptionSet optionset = {
		Option("s|src=", [&](string s)
		{
			src_conn_name = s;
		}),
		Option("d|dst=", [&](string s)
		{
			dst_conn_name = s;
		}),
		Option("f|format=", [&](string s)
		{
			pixfmt = FourCCToPixelFormat(s);
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

	VideoDevice vid("/dev/video11");

	Card card;
	ResourceManager resman(card);

	auto src_conn = resman.reserve_connector(src_conn_name);
	auto src_crtc = resman.reserve_crtc(src_conn);

	uint32_t src_width = src_crtc->mode().hdisplay;
	uint32_t src_height = src_crtc->mode().vdisplay;

	printf("src %s, crtc %ux%u\n", src_conn->fullname().c_str(), src_width, src_height);

	auto dst_conn = resman.reserve_connector(dst_conn_name);
	auto dst_crtc = resman.reserve_crtc(dst_conn);
	auto dst_plane = resman.reserve_overlay_plane(dst_crtc, pixfmt);
	FAIL_IF(!dst_plane, "Plane not found");

	uint32_t dst_width = min((uint32_t)dst_crtc->mode().hdisplay, src_width);
	uint32_t dst_height = min((uint32_t)dst_crtc->mode().vdisplay, src_height);

	printf("dst %s, crtc %ux%u, plane %ux%u\n", dst_conn->fullname().c_str(),
	       dst_crtc->mode().hdisplay, dst_crtc->mode().vdisplay,
	       dst_width, dst_height);

	for (int i = 0; i < CAMERA_BUF_QUEUE_SIZE; ++i) {
		auto fb = new DumbFramebuffer(card, src_width, src_height, pixfmt);
		s_fbs.push_back(fb);
		s_free_fbs.push_back(fb);
	}

	// get one fb for initial setup
	s_ready_fbs.push_back(s_free_fbs.back());
	s_free_fbs.pop_back();

	// This draws a moving bar to SRC display
	BarFlipState barflipper(card, src_crtc);
	barflipper.start_flipping();

	// This shows the captures SRC frames on DST display
	WBFlipState wbflipper(card, dst_crtc, dst_plane);
	wbflipper.setup(0, 0, dst_width, dst_height);

	WBStreamer wb(vid.get_capture_streamer(), src_crtc, src_width, src_height, pixfmt);
	wb.start_streaming();

	vector<pollfd> fds(3);

	fds[0].fd = 0;
	fds[0].events =  POLLIN;
	fds[1].fd = wb.fd();
	fds[1].events =  POLLIN;
	fds[2].fd = card.fd();
	fds[2].events =  POLLIN;

	while (true) {
		int r = poll(fds.data(), fds.size(), -1);
		ASSERT(r > 0);

		if (fds[0].revents != 0)
			break;

		if (fds[1].revents) {
			fds[1].revents = 0;

			wb.Dequeue();
			wbflipper.queue_next();
		}

		if (fds[2].revents) {
			fds[2].revents = 0;

			card.call_page_flip_handlers();
			wb.Queue();
		}
	}

	printf("exiting...\n");
}
