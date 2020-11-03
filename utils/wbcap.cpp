#include <fstream>
#include <unistd.h>
#include <poll.h>
#include <chrono>

#include <fmt/format.h>
#include <fmt/chrono.h>

#include <kms++/kms++.h>
#include <kms++util/kms++util.h>

using namespace std;
using namespace kms;
using namespace std::chrono;

static Connector* find_wb_conn(Card& card)
{
	auto conns = card.get_connectors();
	auto it = find_if(conns.begin(), conns.end(), [](Connector* c) { return c->is_wb(); });
	if (it == conns.end())
		return nullptr;
	return *it;
}

[[maybe_unused]]
static uint16_t crc16(uint16_t crc, uint8_t data)
{
	const uint16_t CRC16_IBM = 0x8005;

	for (uint8_t i = 0; i < 8; i++) {
		if (((crc & 0x8000) >> 8) ^ (data & 0x80))
			crc = (crc << 1) ^ CRC16_IBM;
		else
			crc = (crc << 1);

		data <<= 1;
	}

	return crc;
}

[[maybe_unused]]
static string fb_crc(IFramebuffer* fb)
{
	uint8_t* p = fb->map(0);
	uint16_t r, g, b;

	r = g = b = 0;

	for (unsigned y = 0; y < fb->height(); ++y) {
		for (unsigned x = 0; x < fb->width(); ++x) {
			uint32_t* p32 = (uint32_t*)(p + fb->stride(0) * y + x * 4);
			RGB rgb(*p32);

			r = crc16(r, rgb.r);
			r = crc16(r, 0);

			g = crc16(g, rgb.g);
			g = crc16(g, 0);

			b = crc16(b, rgb.b);
			b = crc16(b, 0);
		}
	}

	return fmt::format("{:#06x} {:#06x} {:#06x}", r, g, b);
}

[[maybe_unused]]
static uint32_t fb_crc2(IFramebuffer* fb)
{
	uint32_t hash = 0;

	uint8_t* p = fb->map(0);

	for (unsigned y = 0; y < fb->height(); ++y) {
		for (unsigned x = 0; x < fb->width(); ++x) {
			uint32_t* p32 = (uint32_t*)(p + fb->stride(0) * y + x * 4);
			uint32_t pix = *p32;
			pix &= 0xffffff; // drop the alpha
			hash = (hash + (324723947 + pix)) ^ 93485734985;
		}
	}

	return hash;
}

int main()
{
	Card card;

	card.disable_all();

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

	auto conn = res.reserve_connector(lcd_conn_name);
	FAIL_IF(!conn, "available connector not found");
	auto crtc = res.reserve_crtc(conn);
	FAIL_IF(!crtc, "available crtc not found");
	auto plane = res.reserve_overlay_plane(crtc, pixfmt);
	FAIL_IF(!plane, "available plane not found");

	auto mode = conn->get_default_mode();
	auto mode_blob = mode.to_blob(card);

	uint32_t lcd_fb_w = mode.hdisplay;
	uint32_t lcd_fb_h = mode.vdisplay;

	auto lcd_fb1 = new DumbFramebuffer(card, lcd_fb_w, lcd_fb_h, pixfmt);
	auto lcd_fb2 = new DumbFramebuffer(card, lcd_fb_w, lcd_fb_h, pixfmt);
	draw_test_pattern(*lcd_fb1);
	draw_test_pattern(*lcd_fb2);
	auto fbs = { lcd_fb1, lcd_fb2 };

	{
		AtomicReq req(card);
		req.add_display(conn, crtc, mode_blob.get(), plane, lcd_fb1);
		req.add(wb_conn, "CRTC_ID", crtc->id());
		req.commit_sync(true);
	}

	vector<DumbFramebuffer*> wb_fbs;
	for (int i = 0; i < 3; ++i) {
		auto wb_fb = new DumbFramebuffer(card, crtc->width(), crtc->height(), pixfmt);
		draw_test_pattern(*wb_fb);
		draw_text(*wb_fb, 0, 0, to_string(i), RGB(255, 255, 255));
		wb_fbs.push_back(wb_fb);
	}

#ifndef HDMI
	auto hdmi_conn = res.reserve_connector("HDMI");
	FAIL_IF(!hdmi_conn, "available connector not found");
	auto hdmi_crtc = res.reserve_crtc(hdmi_conn);
	FAIL_IF(!hdmi_crtc, "available crtc not found");
	auto hdmi_plane1 = res.reserve_generic_plane(hdmi_crtc, pixfmt);
	FAIL_IF(!hdmi_plane1, "available plane not found");
	auto hdmi_plane2 = res.reserve_generic_plane(hdmi_crtc, pixfmt);
	FAIL_IF(!hdmi_plane2, "available plane not found");
	auto hdmi_plane3 = res.reserve_generic_plane(hdmi_crtc, pixfmt);
	FAIL_IF(!hdmi_plane3, "available plane not found");

	auto hdmi_planes = vector<Plane*>{ hdmi_plane1, hdmi_plane2, hdmi_plane3 };

	auto hdmi_mode = hdmi_conn->get_default_mode();
	auto hdmi_mode_blob = hdmi_mode.to_blob(card);

	{
		AtomicReq req(card);

		//req.add_display(hdmi_conn, hdmi_crtc, hdmi_mode_blob.get(), hdmi_plane1, wb_fbs[0]);

		req.add(hdmi_conn, {
					   { "CRTC_ID", hdmi_crtc->id() },
				   });

		req.add(hdmi_crtc, {
					   { "ACTIVE", 1 },
					   { "MODE_ID", hdmi_mode_blob->id() },
				   });

		for (int i = 0; i < 3; ++i) {
			auto fb = wb_fbs[i];
			req.add(hdmi_planes[i], {
							{ "FB_ID", fb->id() },
							{ "CRTC_ID", hdmi_crtc->id() },
							{ "SRC_X", 0 << 16 },
							{ "SRC_Y", 0 << 16 },
							{ "SRC_W", fb->width() << 16 },
							{ "SRC_H", fb->height() << 16 },
							{ "CRTC_X", (i % 2) == 0 ? 0 : 850 },
							{ "CRTC_Y", (i / 2) == 0 ? 0 : 500 },
							{ "CRTC_W", fb->width() },
							{ "CRTC_H", fb->height() },
						});
		}

		int r = req.commit_sync(true);
		assert(!r);
	}
#endif

	getchar();

	class FlipHandler : public PageFlipHandlerBase
	{
	public:
		FlipHandler(Card& card, Plane* plane, vector<DumbFramebuffer*> bufs,
			    Crtc* src_crtc, Connector* wb_conn, vector<DumbFramebuffer*> wb_bufs)
			: m_card(card), m_plane(plane), m_bufs(bufs),
			  m_src_crtc(src_crtc), m_wb_conn(wb_conn), m_wb_bufs(wb_bufs)
		{
		}

		void handle_page_flip2(uint32_t frame, double time, uint32_t crtc_id)  override
		{
			Crtc* crtc = m_card.get_crtc(crtc_id);
			printf("flip frame %u, crtc %u\n", frame, crtc->idx());
			return;
			do_commit();
		}

		void handle_vblank(uint32_t frame) override
		{
			//printf("VBLANK\n");
			//int r = m_card.vblank_setup(this);
			//FAIL_IF(r, "wait vblank failed");
		}

		void handle_crtc()
		{
			printf("CRTC handler\n");

			close(m_crtc_out_fence);
			m_crtc_out_fence = -1;

			m_active_src = m_committed_src;
			m_committed_src = -1;

			fmt::print("ACTIVE SRC {}, time {}\n", m_active_src, duration_cast<milliseconds>(system_clock::now() - m_src_tp[m_active_src]));
			printf("\n");
			do_commit();
		}

		void handle_wb()
		{
			fmt::print("WB handler\n");

			close(m_wb_out_fence);
			m_wb_out_fence = -1;

			m_active_wb = m_committed_wb;
			m_committed_wb = -1;

			fmt::print("DONE WB {}, time {}\n", m_active_wb, duration_cast<milliseconds>(system_clock::now() - m_wb_tp[m_active_wb]));

			uint32_t wb_crc = fb_crc2(m_current_wb_fb);
			fmt::print("CRC {:#x}   {:#x}   {}\n", m_fb_crc, wb_crc, m_fb_crc == wb_crc ? "OK" : "FAIL");

			if (1) {
				const string filename = fmt::format("wb-dst-{}.raw", m_counter - 1);
				unique_ptr<ofstream> os = unique_ptr<ofstream>(new ofstream(filename, ofstream::binary));

				for (unsigned i = 0; i < m_current_wb_fb->num_planes(); ++i)
					os->write((char*)m_current_wb_fb->map(i), m_current_wb_fb->size(i));
			}

			if (true && m_counter == 3) {
				printf("COUNTER EXCEEDED, PRESS ENTER\n");
				getchar();
				exit(0);
			}

			//do_commit();
		}

		void do_commit()
		{
			uint32_t idx = m_current;
			m_current = (m_current + 1) % m_bufs.size();
			auto fb = m_bufs[idx];
			m_committed_src = idx;

			uint32_t xpos = m_old_xpos[1] + 4;
			if (xpos > m_bufs[0]->width() - 10)
				xpos = 0;
			draw_color_bar(*fb, m_old_xpos[0], xpos, 10);
			m_old_xpos[0] = m_old_xpos[1];
			m_old_xpos[1] = xpos;

			draw_text(*fb, 0, 0, to_string(m_counter), RGB(255, 255, 255));
			m_counter++;

#if 0
			if (m_current_wb_fb) {
				fmt::print("WB  {}: {}   {}\n", m_counter, "" /*fb_crc(m_current_wb_fb)*/, fb_crc2(m_current_wb_fb));
				fmt::print("WB  {}: {}   {}\n", m_counter, "" /*fb_crc(m_current_wb_fb)*/, fb_crc2(m_current_wb_fb));
				fmt::print("WB  {}: {}   {}\n", m_counter, "" /*fb_crc(m_current_wb_fb)*/, fb_crc2(m_current_wb_fb));

				m_wb_bufs.push_back(m_current_wb_fb);

				if (0) {
					const string filename = fmt::format("wbcap-{}-1.raw", m_counter - 1);
					unique_ptr<ofstream> os = unique_ptr<ofstream>(new ofstream(filename, ofstream::binary));

					for (unsigned i = 0; i < m_current_wb_fb->num_planes(); ++i)
						os->write((char*)m_current_wb_fb->map(i), m_current_wb_fb->size(i));
				}

				if (0) {
					const string filename = fmt::format("wbcap-{}-2.raw", m_counter - 1);
					unique_ptr<ofstream> os = unique_ptr<ofstream>(new ofstream(filename, ofstream::binary));

					for (unsigned i = 0; i < m_current_wb_fb->num_planes(); ++i)
						os->write((char*)m_current_wb_fb->map(i), m_current_wb_fb->size(i));
				}

				fmt::print("WB  {}: {}   {}\n", m_counter, fb_crc(m_current_wb_fb), fb_crc2(m_current_wb_fb));
			}
#endif
			//fmt::print("LCD {}: {}\n", m_counter, fb_crc(fb));

			//m_current_wb_fb = m_wb_bufs.back();
			//m_wb_bufs.pop_back();
			uint32_t wb_idx = (m_counter - 1) % m_wb_bufs.size();
			//wb_idx = 0;
			m_current_wb_fb  = m_wb_bufs[wb_idx];
			m_committed_wb = wb_idx;

			fmt::print("COMMIT SRC {} WB {}\n", m_committed_src, m_committed_wb); fflush(stdout);

			m_fb_crc = fb_crc2(fb);

			if (0) {
				const string filename = fmt::format("wb-src-{}.raw", m_counter - 1);
				unique_ptr<ofstream> os = unique_ptr<ofstream>(new ofstream(filename, ofstream::binary));

				for (unsigned i = 0; i < fb->num_planes(); ++i)
					os->write((char*)fb->map(i), fb->size(i));
			}


			//std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now())

			m_src_tp[m_committed_src] = system_clock::now();
			m_wb_tp[m_committed_wb] = system_clock::now();


			{
				AtomicReq req(m_card);
				req.add(m_plane, "FB_ID", fb->id());
				req.add(m_src_crtc, "OUT_FENCE_PTR", (uint64_t)&m_crtc_out_fence);
				req.add(m_wb_conn, "WRITEBACK_FB_ID", m_current_wb_fb->id());
				req.add(m_wb_conn, "WRITEBACK_OUT_FENCE_PTR", (uint64_t)&m_wb_out_fence);
				req.add(m_wb_conn, "CRTC_ID", m_src_crtc->id());
				int r = req.commit(this, true);
				FAIL_IF(r, "plane commit failed: %d", r);

				FAIL_IF(m_crtc_out_fence == -1, "crtc out fence failed");
				FAIL_IF(m_wb_out_fence == -1, "wb out fence failed");
			}


			//printf("COMMIT DONE\n"); fflush(stdout);

			if (0)
			{
			int old_xpos = 0;
			uint32_t xpos = 0;
			while (true) {
				draw_color_bar(*fb, old_xpos, xpos, 10);
				old_xpos = xpos;
				xpos += 4;
				if (xpos > fb->width() - 10)
					xpos = 0;
			}
			}
		}

		Card& m_card;
		Plane* m_plane;
		vector<DumbFramebuffer*> m_bufs;
		uint32_t m_current = 0;
		uint32_t m_counter = 0;
		uint32_t m_old_xpos[2] = { 0 };

		Crtc* m_src_crtc;
		Connector* m_wb_conn;
		DumbFramebuffer* m_current_wb_fb = nullptr;
		vector<DumbFramebuffer*> m_wb_bufs;

		int32_t m_crtc_out_fence = -1;
		int32_t m_wb_out_fence = -1;

		uint32_t m_fb_crc;

		int32_t m_committed_src = -1;
		int32_t m_committed_wb = -1;
		int32_t m_active_src = -1;
		int32_t m_active_wb = -1;

		time_point<system_clock> m_src_tp[5];
		time_point<system_clock> m_wb_tp[5];
	};

	usleep(100000);
	printf("\nstarting\n");

	FlipHandler flip(card, plane, fbs, crtc, wb_conn, wb_fbs);
	flip.do_commit();

	//int r = card.vblank_setup(&flip);
	//FAIL_IF(r, "wait vblank failed");

	while (true) {
		vector<struct pollfd> fds = {
			{ card.fd(), POLLIN },
		};

		if (flip.m_crtc_out_fence != -1)
			fds.push_back({ flip.m_crtc_out_fence, POLLIN });

		if (flip.m_wb_out_fence != -1)
			fds.push_back({ flip.m_wb_out_fence, POLLIN });

		int r = poll(fds.data(), fds.size(), -1);
		FAIL_IF(r < 0, "poll() failed with %d: %s", errno, strerror(errno));
		FAIL_IF(r == 0, "poll timeout");

		for (const auto& pfd : fds) {
			if (pfd.revents & POLLIN) {
				if (pfd.fd == card.fd())
					card.call_page_flip_handlers();
				else if (pfd.fd == flip.m_crtc_out_fence)
					flip.handle_crtc();
				else if (pfd.fd == flip.m_wb_out_fence)
					flip.handle_wb();
			}
		}
	}

	return 0;
}
