#include <fmt/format.h>

#include <kms++/kms++.h>
#include <kms++/modedb.h>
#include <kms++/mode_cvt.h>

#include <kms++util/kms++util.h>

using namespace std;
using namespace kms;

static Connector* find_wb_conn(Card& card)
{
	auto conns = card.get_connectors();
	auto it = find_if(conns.begin(), conns.end(), [](Connector* c) { return c->is_wb(); });
	if (it == conns.end())
		return nullptr;
	return *it;
}

int main()
{
	Card card;

	ResourceManager res(card);

	// Get WB conn

	Connector* wb_conn = find_wb_conn(card);
	FAIL_IF(!wb_conn, "No WB connector found");

	{
		auto wb_modes_blob = wb_conn->get_prop_value_as_blob("WRITEBACK_PIXEL_FORMATS");
		auto wb_modes_vec = wb_modes_blob->data();
		auto v = vector<uint32_t>((uint32_t*)wb_modes_vec.data(), (uint32_t*)wb_modes_vec.data() + wb_modes_vec.size() / 4);
		fmt::print("WB pix formats:\n");
		for (auto& m : v)
			fmt::print("  {}\n", PixelFormatToFourCC((PixelFormat)m));
	}

	res.reserve_connector(wb_conn);

	PixelFormat pixfmt = PixelFormat::XRGB8888;

	// Setup LCD

	string lcd_conn_name = "DPI";

	auto lcd_conn = res.reserve_connector(lcd_conn_name);
	FAIL_IF(!lcd_conn, "available connector not found");
	auto lcd_crtc = res.reserve_crtc(lcd_conn);
	FAIL_IF(!lcd_crtc, "available crtc not found");
	auto lcd_plane = res.reserve_overlay_plane(lcd_crtc, pixfmt);
	FAIL_IF(!lcd_plane, "available plane not found");

	auto lcd_mode = lcd_conn->get_default_mode();
	auto lcd_mode_blob = lcd_mode.to_blob(card);

	uint32_t lcd_fb_w = 600;
	uint32_t lcd_fb_h = 400;

	auto lcd_fb1 = new DumbFramebuffer(card, lcd_fb_w, lcd_fb_h, pixfmt);
	auto lcd_fb2 = new DumbFramebuffer(card, lcd_fb_w, lcd_fb_h, pixfmt);
	draw_test_pattern(*lcd_fb1);
	draw_test_pattern(*lcd_fb2);
	auto lcd_fbs = {lcd_fb1, lcd_fb2};

	{
		AtomicReq req(card);
		req.add_display(lcd_conn, lcd_crtc, lcd_mode_blob.get(), lcd_plane, lcd_fb1);
		req.commit_sync(true);
	}

	// Setup HDMI

	string hdmi_conn_name = "HDMI";

	auto hdmi_conn = res.reserve_connector(hdmi_conn_name);
	FAIL_IF(!hdmi_conn, "available connector not found");
	auto hdmi_crtc = res.reserve_crtc(hdmi_conn);
	FAIL_IF(!hdmi_crtc, "available crtc not found");
	auto hdmi_plane = res.reserve_overlay_plane(hdmi_crtc, pixfmt);
	FAIL_IF(!hdmi_plane, "available plane not found");

	auto hdmi_mode = hdmi_conn->get_default_mode();
	auto hdmi_mode_blob = hdmi_mode.to_blob(card);

	uint32_t hdmi_fb_w = lcd_mode.hdisplay;
	uint32_t hdmi_fb_h = lcd_mode.vdisplay;

	vector<DumbFramebuffer*> wb_fbs;
	for (int i = 0; i < 3; ++i) {
		auto hdmi_fb = new DumbFramebuffer(card, hdmi_fb_w, hdmi_fb_h, pixfmt);
		draw_test_pattern(*hdmi_fb);
		wb_fbs.push_back(hdmi_fb);
	}

	{
		AtomicReq req(card);
		req.add_display(hdmi_conn, hdmi_crtc, hdmi_mode_blob.get(), hdmi_plane, wb_fbs[0]);
		req.commit_sync(true);
	}

	//getc(stdin);

#if 0
	{
		int r;
		AtomicReq req(card);
		req.add(wb_conn, "WRITEBACK_FB_ID", hdmi_fb->id());
		req.add(wb_conn, "CRTC_ID", lcd_crtc->id());
		r = req.test(true);
		FAIL_IF(r, "WB commit test failed");
		r = req.commit_sync(true);
		FAIL_IF(r, "WB commit failed");
	}
#endif

	fd_set fds;

	int fd = card.fd();

	class FlipHandler : public PageFlipHandlerBase
	{
	public:
		FlipHandler(Card& card, Plane* plane, vector<DumbFramebuffer*> bufs,
			    Crtc* src_crtc, Connector* wb_conn, vector<DumbFramebuffer*> wb_bufs)
			: m_card(card), m_plane(plane), m_bufs(bufs),
			  m_src_crtc(src_crtc), m_wb_conn(wb_conn), m_wb_bufs(wb_bufs)
		{

		}

		void handle_page_flip(uint32_t frame, double time)
		{
			printf("flip\n");
			do_commit();
		}

		void do_commit()
		{
			uint32_t idx = m_current;
			m_current = (m_current + 1) % m_bufs.size();
			auto fb = m_bufs[idx];

			uint32_t xpos = m_old_xpos[1] + 4;
			if (xpos > m_bufs[0]->width() - 10)
				xpos = 0;
			draw_color_bar(*fb, m_old_xpos[0], xpos, 10);
			m_old_xpos[0] = m_old_xpos[1];
			m_old_xpos[1] = xpos;

			draw_text(*fb, 0, 0, to_string(m_counter), RGB(255, 255, 255));
			m_counter++;


			uint32_t wb_idx = m_wb_current;
			m_wb_current = (m_wb_current + 1) % m_wb_bufs.size();
			auto wb_fb = m_wb_bufs[wb_idx];

			AtomicReq req(m_card);
			req.add(m_plane, "FB_ID", fb->id());
			req.add(m_wb_conn, "WRITEBACK_FB_ID", wb_fb->id());
			req.add(m_wb_conn, "CRTC_ID", m_src_crtc->id());
			int r = req.commit(this, true);
			FAIL_IF(r, "plane commit failed: %d", r);
		}

		Card& m_card;
		Plane* m_plane;
		vector<DumbFramebuffer*> m_bufs;
		uint32_t m_current = 0;
		uint32_t m_counter = 0;
		uint32_t m_old_xpos[2] = {0};

		Crtc* m_src_crtc;
		Connector* m_wb_conn;
		vector<DumbFramebuffer*> m_wb_bufs;
		uint32_t m_wb_current = 0;
	};

	FlipHandler flip(card, lcd_plane, lcd_fbs, lcd_crtc, wb_conn, wb_fbs);
	flip.do_commit();

	while (true) {
		FD_ZERO(&fds);
		FD_SET(fd, &fds);

		int r;

		r = select(fd + 1, &fds, NULL, NULL, NULL);
		if (r < 0) {
			fmt::print(stderr, "select() failed with {}: {}\n", errno, strerror(errno));
			abort();
		} else if (FD_ISSET(fd, &fds)) {
			card.call_page_flip_handlers();
		}
	}

	getc(stdin);

#if 0



	draw_rect(*lcd_fb, 0, 0, lcd_fb_w, lcd_fb_h, RGB(255, 0, 0));

	getc(stdin);

	{
		int r;
		AtomicReq req(card);
		req.add(wb_conn, "WRITEBACK_FB_ID", hdmi_fb->id());
		req.add(wb_conn, "CRTC_ID", lcd_crtc->id());
		r = req.test(true);
		FAIL_IF(r, "WB commit test failed");
		r = req.commit_sync(true);
		FAIL_IF(r, "WB commit failed");
	}

	getc(stdin);
#endif
	return 0;
}
