#include <cstdio>
#include <cstring>
#include <algorithm>
#include <regex>
#include <set>
#include <chrono>

#include <sys/select.h>

#include <kms++/kms++.h>
#include <kms++/modedb.h>
#include <kms++/mode_cvt.h>

#include <kms++util/kms++util.h>

using namespace std;
using namespace kms;

struct PlaneInfo
{
	Plane* plane;

	unsigned x;
	unsigned y;
	unsigned w;
	unsigned h;

	vector<MappedFramebuffer*> fbs;
};

struct OutputInfo
{
	Connector* connector;

	Crtc* crtc;
	Plane* primary_plane;
	Videomode mode;
	bool user_set_crtc;
	vector<MappedFramebuffer*> fbs;

	vector<PlaneInfo> planes;
};

static bool s_use_dmt;
static bool s_use_cea;
static unsigned s_num_buffers = 1;
static bool s_flip_mode;
static bool s_flip_sync;
static bool s_cvt;
static bool s_cvt_v2;
static bool s_cvt_vid_opt;

static set<Crtc*> s_used_crtcs;
static set<Plane*> s_used_planes;

__attribute__ ((unused))
static void print_regex_match(smatch sm)
{
	for (unsigned i = 0; i < sm.size(); ++i) {
		string str = sm[i].str();
		printf("%u: %s\n", i, str.c_str());
	}
}

static void get_default_connector(Card& card, OutputInfo& output)
{
	output.connector = card.get_first_connected_connector();
	output.mode = output.connector->get_default_mode();
}

static void parse_connector(Card& card, const string& str, OutputInfo& output)
{
	Connector* conn = resolve_connector(card, str);

	if (!conn)
		EXIT("No connector '%s'", str.c_str());

	if (!conn->connected())
		EXIT("Connector '%s' not connected", conn->fullname().c_str());

	output.connector = conn;
	output.mode = output.connector->get_default_mode();
}

static void get_default_crtc(Card& card, OutputInfo& output)
{
	Crtc* crtc = output.connector->get_current_crtc();

	if (crtc && s_used_crtcs.find(crtc) == s_used_crtcs.end()) {
		s_used_crtcs.insert(crtc);
		output.crtc = crtc;
		return;
	}

	for (const auto& possible : output.connector->get_possible_crtcs()) {
		if (s_used_crtcs.find(possible) == s_used_crtcs.end()) {
			s_used_crtcs.insert(possible);
			output.crtc = possible;
			return;
		}
	}

	EXIT("Could not find available crtc");
}

static void parse_crtc(Card& card, const string& crtc_str, OutputInfo& output)
{
	// @12:1920x1200i@60
	// @12:33000000,800/210/30/16/-,480/22/13/10/-,i

	const regex modename_re("(?:(@?)(\\d+):)?"	// @12:
				"(?:(\\d+)x(\\d+)(i)?)"	// 1920x1200i
				"(?:@([\\d\\.]+))?");	// @60

	const regex modeline_re("(?:(@?)(\\d+):)?"			// @12:
				"(\\d+),"				// 33000000,
				"(\\d+)/(\\d+)/(\\d+)/(\\d+)/([+-]),"	// 800/210/30/16/-,
				"(\\d+)/(\\d+)/(\\d+)/(\\d+)/([+-])"	// 480/22/13/10/-
				"(?:,([i]+))?"				// ,i
				);

	smatch sm;
	if (regex_match(crtc_str, sm, modename_re)) {
		if (sm[2].matched) {
			bool use_id = sm[1].length() == 1;
			unsigned num = stoul(sm[2].str());

			if (use_id) {
				Crtc* c = card.get_crtc(num);
				if (!c)
					EXIT("Bad crtc id '%u'", num);

				output.crtc = c;
			} else {
				auto crtcs = card.get_crtcs();

				if (num >= crtcs.size())
					EXIT("Bad crtc number '%u'", num);

				output.crtc = crtcs[num];
			}
		} else {
			output.crtc = output.connector->get_current_crtc();
		}

		unsigned w = stoul(sm[3]);
		unsigned h = stoul(sm[4]);
		bool ilace = sm[5].matched ? true : false;
		float refresh = sm[6].matched ? stof(sm[6]) : 0;

		if (s_cvt) {
			output.mode = videomode_from_cvt(w, h, refresh, ilace, s_cvt_v2, s_cvt_vid_opt);
		} else if (s_use_dmt) {
			try {
				output.mode = find_dmt(w, h, refresh, ilace);
			} catch (exception& e) {
				EXIT("Mode not found from DMT tables\n");
			}
		} else if (s_use_cea) {
			try {
				output.mode = find_cea(w, h, refresh, ilace);
			} catch (exception& e) {
				EXIT("Mode not found from CEA tables\n");
			}
		} else {
			try {
				output.mode = output.connector->get_mode(w, h, refresh, ilace);
			} catch (exception& e) {
				EXIT("Mode not found from the connector\n");
			}
		}
	} else if (regex_match(crtc_str, sm, modeline_re)) {
		if (sm[2].matched) {
			bool use_id = sm[1].length() == 1;
			unsigned num = stoul(sm[2].str());

			if (use_id) {
				Crtc* c = card.get_crtc(num);
				if (!c)
					EXIT("Bad crtc id '%u'", num);

				output.crtc = c;
			} else {
				auto crtcs = card.get_crtcs();

				if (num >= crtcs.size())
					EXIT("Bad crtc number '%u'", num);

				output.crtc = crtcs[num];
			}
		} else {
			output.crtc = output.connector->get_current_crtc();
		}

		unsigned clock = stoul(sm[3]);

		unsigned hact = stoul(sm[4]);
		unsigned hfp = stoul(sm[5]);
		unsigned hsw = stoul(sm[6]);
		unsigned hbp = stoul(sm[7]);
		bool h_pos_sync = sm[8] == "+" ? true : false;

		unsigned vact = stoul(sm[9]);
		unsigned vfp = stoul(sm[10]);
		unsigned vsw = stoul(sm[11]);
		unsigned vbp = stoul(sm[12]);
		bool v_pos_sync = sm[13] == "+" ? true : false;

		output.mode = videomode_from_timings(clock / 1000, hact, hfp, hsw, hbp, vact, vfp, vsw, vbp);
		output.mode.set_hsync(h_pos_sync ? SyncPolarity::Positive : SyncPolarity::Negative);
		output.mode.set_vsync(v_pos_sync ? SyncPolarity::Positive : SyncPolarity::Negative);

		if (sm[14].matched) {
			for (int i = 0; i < sm[14].length(); ++i) {
				char f = string(sm[14])[i];

				switch (f) {
				case 'i':
					output.mode.set_interlace(true);
					break;
				default:
					EXIT("Bad mode flag %c", f);
				}
			}
		}
	} else {
		EXIT("Failed to parse crtc option '%s'", crtc_str.c_str());
	}
}

static void parse_plane(Card& card, const string& plane_str, const OutputInfo& output, PlaneInfo& pinfo)
{
	// 3:400,400-400x400
	const regex plane_re("(?:(@?)(\\d+):)?"		// 3:
			     "(?:(\\d+),(\\d+)-)?"	// 400,400-
			     "(\\d+)x(\\d+)");		// 400x400

	smatch sm;
	if (!regex_match(plane_str, sm, plane_re))
		EXIT("Failed to parse plane option '%s'", plane_str.c_str());

	if (sm[2].matched) {
		bool use_id = sm[1].length() == 1;
		unsigned num = stoul(sm[2].str());

		if (use_id) {
			Plane* p = card.get_plane(num);
			if (!p)
				EXIT("Bad plane id '%u'", num);

			pinfo.plane = p;
		} else {
			auto planes = card.get_planes();

			if (num >= planes.size())
				EXIT("Bad plane number '%u'", num);

			pinfo.plane = planes[num];
		}
	} else {
		for (Plane* p : output.crtc->get_possible_planes()) {
			if (s_used_planes.find(p) != s_used_planes.end())
				continue;

			if (p->plane_type() != PlaneType::Overlay)
				continue;

			pinfo.plane = p;
		}

		if (!pinfo.plane)
			EXIT("Failed to find available plane");
	}

	s_used_planes.insert(pinfo.plane);

	pinfo.w = stoul(sm[5]);
	pinfo.h = stoul(sm[6]);

	if (sm[3].matched)
		pinfo.x = stoul(sm[3]);
	else
		pinfo.x = output.mode.hdisplay / 2 - pinfo.w / 2;

	if (sm[4].matched)
		pinfo.y = stoul(sm[4]);
	else
		pinfo.y = output.mode.vdisplay / 2 - pinfo.h / 2;
}

static vector<MappedFramebuffer*> get_default_fb(Card& card, unsigned width, unsigned height)
{
	vector<MappedFramebuffer*> v;

	for (unsigned i = 0; i < s_num_buffers; ++i)
		v.push_back(new DumbFramebuffer(card, width, height, PixelFormat::XRGB8888));

	return v;
}

static vector<MappedFramebuffer*> parse_fb(Card& card, const string& fb_str, unsigned def_w, unsigned def_h)
{
	unsigned w = def_w;
	unsigned h = def_h;
	PixelFormat format = PixelFormat::XRGB8888;

	if (!fb_str.empty()) {
		// XXX the regexp is not quite correct
		// 400x400-NV12
		const regex fb_re("(?:(\\d+)x(\\d+))?"		// 400x400
				  "(?:-)?"			// -
				  "(\\w\\w\\w\\w)?");		// NV12

		smatch sm;
		if (!regex_match(fb_str, sm, fb_re))
			EXIT("Failed to parse fb option '%s'", fb_str.c_str());

		if (sm[1].matched)
			w = stoul(sm[1]);
		if (sm[2].matched)
			h = stoul(sm[2]);
		if (sm[3].matched)
			format = FourCCToPixelFormat(sm[3]);
	}

	vector<MappedFramebuffer*> v;

	for (unsigned i = 0; i < s_num_buffers; ++i)
		v.push_back(new DumbFramebuffer(card, w, h, format));

	return v;
}

static const char* usage_str =
		"Usage: kmstest [OPTION]...\n\n"
		"Show a test pattern on a display or plane\n\n"
		"Options:\n"
		"      --device=DEVICE       DEVICE is the path to DRM card to open\n"
		"  -c, --connector=CONN      CONN is <connector>\n"
		"  -r, --crtc=CRTC           CRTC is [<crtc>:]<w>x<h>[@<Hz>]\n"
		"                            or\n"
		"                            [<crtc>:]<pclk>,<hact>/<hfp>/<hsw>/<hbp>/<hsp>,<vact>/<vfp>/<vsw>/<vbp>/<vsp>[,i]\n"
		"  -p, --plane=PLANE         PLANE is [<plane>:][<x>,<y>-]<w>x<h>\n"
		"  -f, --fb=FB               FB is [<w>x<h>][-][<4cc>]\n"
		"      --dmt                 Search for the given mode from DMT tables\n"
		"      --cea                 Search for the given mode from CEA tables\n"
		"      --cvt=CVT             Create videomode with CVT. CVT is 'v1', 'v2' or 'v2o'\n"
		"      --flip                Do page flipping for each output\n"
		"      --sync                Synchronize page flipping\n"
		"\n"
		"<connector>, <crtc> and <plane> can be given by index (<idx>) or id (<id>).\n"
		"<connector> can also be given by name.\n"
		"\n"
		"Options can be given multiple times to set up multiple displays or planes.\n"
		"Options may apply to previous options, e.g. a plane will be set on a crtc set in\n"
		"an earlier option.\n"
		"If you omit parameters, kmstest tries to guess what you mean\n"
		"\n"
		"Examples:\n"
		"\n"
		"Set eDP-1 mode to 1920x1080@60, show XR24 framebuffer on the crtc, and a 400x400 XB24 plane:\n"
		"    kmstest -c eDP-1 -r 1920x1080@60 -f XR24 -p 400x400 -f XB24\n\n"
		"XR24 framebuffer on first connected connector in the default mode:\n"
		"    kmstest -f XR24\n\n"
		"XR24 framebuffer on a 400x400 plane on the first connected connector in the default mode:\n"
		"    kmstest -p 400x400 -f XR24\n\n"
		"Test pattern on the second connector with default mode:\n"
		"    kmstest -c 1\n"
		;

static void usage()
{
	puts(usage_str);
}

enum class ObjectType
{
	Connector,
	Crtc,
	Plane,
	Framebuffer,
};

struct Arg
{
	ObjectType type;
	string arg;
};

static string s_device_path = "/dev/dri/card0";

static vector<Arg> parse_cmdline(int argc, char **argv)
{
	vector<Arg> args;

	OptionSet optionset = {
		Option("|device=",
		[&](string s)
		{
			s_device_path = s;
		}),
		Option("c|connector=",
		[&](string s)
		{
			args.push_back(Arg { ObjectType::Connector, s });
		}),
		Option("r|crtc=", [&](string s)
		{
			args.push_back(Arg { ObjectType::Crtc, s });
		}),
		Option("p|plane=", [&](string s)
		{
			args.push_back(Arg { ObjectType::Plane, s });
		}),
		Option("f|fb=", [&](string s)
		{
			args.push_back(Arg { ObjectType::Framebuffer, s });
		}),
		Option("|dmt", []()
		{
			s_use_dmt = true;
		}),
		Option("|cea", []()
		{
			s_use_cea = true;
		}),
		Option("|flip", []()
		{
			s_flip_mode = true;
			s_num_buffers = 2;
		}),
		Option("|sync", []()
		{
			s_flip_sync = true;
		}),
		Option("|cvt=", [&](string s)
		{
			if (s == "v1")
				s_cvt = true;
			else if (s == "v2")
				s_cvt = s_cvt_v2 = true;
			else if (s == "v2o")
				s_cvt = s_cvt_v2 = s_cvt_vid_opt = true;
			else {
				usage();
				exit(-1);
			}
		}),
		Option("h|help", [&]()
		{
			usage();
			exit(-1);
		}),
	};

	optionset.parse(argc, argv);

	if (optionset.params().size() > 0) {
		usage();
		exit(-1);
	}

	return args;
}

static vector<OutputInfo> setups_to_outputs(Card& card, const vector<Arg>& output_args)
{
	vector<OutputInfo> outputs;

	if (output_args.size() == 0) {
		// no output args, show a pattern on all screens
		for (auto& pipe : card.get_connected_pipelines()) {
			OutputInfo output = { };
			output.connector = pipe.connector;
			output.crtc = pipe.crtc;
			output.mode = output.connector->get_default_mode();

			output.fbs = get_default_fb(card, output.mode.hdisplay, output.mode.vdisplay);

			outputs.push_back(output);
		}

		return outputs;
	}

	OutputInfo* current_output = 0;
	PlaneInfo* current_plane = 0;

	for (auto& arg : output_args) {
		switch (arg.type) {
		case ObjectType::Connector:
		{
			outputs.push_back(OutputInfo { });
			current_output = &outputs.back();

			parse_connector(card, arg.arg, *current_output);
			current_plane = 0;

			break;
		}

		case ObjectType::Crtc:
		{
			if (!current_output) {
				outputs.push_back(OutputInfo { });
				current_output = &outputs.back();
			}

			if (!current_output->connector)
				get_default_connector(card, *current_output);

			parse_crtc(card, arg.arg, *current_output);

			current_output->user_set_crtc = true;

			current_plane = 0;

			break;
		}

		case ObjectType::Plane:
		{
			if (!current_output) {
				outputs.push_back(OutputInfo { });
				current_output = &outputs.back();
			}

			if (!current_output->connector)
				get_default_connector(card, *current_output);

			if (!current_output->crtc)
				get_default_crtc(card, *current_output);

			current_output->planes.push_back(PlaneInfo { });
			current_plane = &current_output->planes.back();

			parse_plane(card, arg.arg, *current_output, *current_plane);

			break;
		}

		case ObjectType::Framebuffer:
		{
			if (!current_output) {
				outputs.push_back(OutputInfo { });
				current_output = &outputs.back();
			}

			if (!current_output->connector)
				get_default_connector(card, *current_output);

			if (!current_output->crtc)
				get_default_crtc(card, *current_output);

			int def_w, def_h;

			if (current_plane) {
				def_w = current_plane->w;
				def_h = current_plane->h;
			} else {
				def_w = current_output->mode.hdisplay;
				def_h = current_output->mode.vdisplay;
			}

			auto fbs = parse_fb(card, arg.arg, def_w, def_h);

			if (current_plane)
				current_plane->fbs = fbs;
			else
				current_output->fbs = fbs;

			break;
		}
		}
	}

	// create default framebuffers if needed
	for (OutputInfo& o : outputs) {
		if (!o.crtc) {
			get_default_crtc(card, *current_output);
			o.user_set_crtc = true;
		}

		if (o.fbs.empty() && o.user_set_crtc)
			o.fbs = get_default_fb(card, o.mode.hdisplay, o.mode.vdisplay);

		for (PlaneInfo &p : o.planes) {
			if (p.fbs.empty())
				p.fbs = get_default_fb(card, p.w, p.h);
		}
	}

	return outputs;
}

static std::string videomode_to_string(const Videomode& m)
{
	string h = sformat("%u/%u/%u/%u", m.hdisplay, m.hfp(), m.hsw(), m.hbp());
	string v = sformat("%u/%u/%u/%u", m.vdisplay, m.vfp(), m.vsw(), m.vbp());

	return sformat("%s %.3f %s %s %u (%.2f) %#x %#x",
		       m.name.c_str(),
		       m.clock / 1000.0,
		       h.c_str(), v.c_str(),
		       m.vrefresh, m.calculated_vrefresh(),
		       m.flags,
		       m.type);
}

static void print_outputs(const vector<OutputInfo>& outputs)
{
	for (unsigned i = 0; i < outputs.size(); ++i) {
		const OutputInfo& o = outputs[i];

		printf("Connector %u/@%u: %s\n", o.connector->idx(), o.connector->id(),
		       o.connector->fullname().c_str());
		printf("  Crtc %u/@%u", o.crtc->idx(), o.crtc->id());
		if (o.primary_plane)
			printf(" (plane %u/@%u)", o.primary_plane->idx(), o.primary_plane->id());
		printf(": %s\n", videomode_to_string(o.mode).c_str());
		if (!o.fbs.empty()) {
			auto fb = o.fbs[0];
			printf("    Fb %u %ux%u-%s\n", fb->id(), fb->width(), fb->height(),
			       PixelFormatToFourCC(fb->format()).c_str());
		}

		for (unsigned j = 0; j < o.planes.size(); ++j) {
			const PlaneInfo& p = o.planes[j];
			auto fb = p.fbs[0];
			printf("  Plane %u/@%u: %u,%u-%ux%u\n", p.plane->idx(), p.plane->id(),
			       p.x, p.y, p.w, p.h);
			printf("    Fb %u %ux%u-%s\n", fb->id(), fb->width(), fb->height(),
			       PixelFormatToFourCC(fb->format()).c_str());
		}
	}
}

static void draw_test_patterns(const vector<OutputInfo>& outputs)
{
	for (const OutputInfo& o : outputs) {
		for (auto fb : o.fbs)
			draw_test_pattern(*fb);

		for (const PlaneInfo& p : o.planes)
			for (auto fb : p.fbs)
				draw_test_pattern(*fb);
	}
}

static void set_crtcs_n_planes_legacy(Card& card, const vector<OutputInfo>& outputs)
{
	for (const OutputInfo& o : outputs) {
		auto conn = o.connector;
		auto crtc = o.crtc;

		if (!o.fbs.empty()) {
			auto fb = o.fbs[0];
			int r = crtc->set_mode(conn, *fb, o.mode);
			if (r)
				printf("crtc->set_mode() failed for crtc %u: %s\n",
				       crtc->id(), strerror(-r));
		}

		for (const PlaneInfo& p : o.planes) {
			auto fb = p.fbs[0];
			int r = crtc->set_plane(p.plane, *fb,
						p.x, p.y, p.w, p.h,
						0, 0, fb->width(), fb->height());
			if (r)
				printf("crtc->set_plane() failed for plane %u: %s\n",
				       p.plane->id(), strerror(-r));
		}
	}
}

static void set_crtcs_n_planes(Card& card, const vector<OutputInfo>& outputs)
{
	// Keep blobs here so that we keep ref to them until we have committed the req
	vector<unique_ptr<Blob>> blobs;

	AtomicReq req(card);

	for (const OutputInfo& o : outputs) {
		auto conn = o.connector;
		auto crtc = o.crtc;

		blobs.emplace_back(o.mode.to_blob(card));
		Blob* mode_blob = blobs.back().get();

		req.add(conn, {
				{ "CRTC_ID", crtc->id() },
			});

		req.add(crtc, {
				{ "ACTIVE", 1 },
				{ "MODE_ID", mode_blob->id() },
			});

		if (!o.fbs.empty()) {
			auto fb = o.fbs[0];

			req.add(o.primary_plane, {
					{ "FB_ID", fb->id() },
					{ "CRTC_ID", crtc->id() },
					{ "SRC_X", 0 << 16 },
					{ "SRC_Y", 0 << 16 },
					{ "SRC_W", fb->width() << 16 },
					{ "SRC_H", fb->height() << 16 },
					{ "CRTC_X", 0 },
					{ "CRTC_Y", 0 },
					{ "CRTC_W", fb->width() },
					{ "CRTC_H", fb->height() },
				});
		}

		for (const PlaneInfo& p : o.planes) {
			auto fb = p.fbs[0];

			req.add(p.plane, {
					{ "FB_ID", fb->id() },
					{ "CRTC_ID", crtc->id() },
					{ "SRC_X", 0 << 16 },
					{ "SRC_Y", 0 << 16 },
					{ "SRC_W", fb->width() << 16 },
					{ "SRC_H", fb->height() << 16 },
					{ "CRTC_X", p.x },
					{ "CRTC_Y", p.y },
					{ "CRTC_W", p.w },
					{ "CRTC_H", p.h },
				});
		}
	}

	int r;

	r = req.test(true);
	if (r)
		EXIT("Atomic test failed: %d\n", r);

	r = req.commit_sync(true);
	if (r)
		EXIT("Atomic commit failed: %d\n", r);
}

class FlipState : private PageFlipHandlerBase
{
public:
	FlipState(Card& card, const string& name, vector<const OutputInfo*> outputs)
		: m_card(card), m_name(name), m_outputs(outputs)
	{
	}

	void start_flipping()
	{
		m_prev_frame = m_prev_print = std::chrono::steady_clock::now();
		m_slowest_frame = std::chrono::duration<float>::min();
		m_frame_num = 0;
		queue_next();
	}

private:
	void handle_page_flip(uint32_t frame, double time)
	{
		m_frame_num++;

		auto now = std::chrono::steady_clock::now();

		std::chrono::duration<float> diff = now - m_prev_frame;
		if (diff > m_slowest_frame)
			m_slowest_frame = diff;

		if (m_frame_num  % 100 == 0) {
			std::chrono::duration<float> fsec = now - m_prev_print;
			printf("Connector %s: fps %f, slowest %.2f ms\n",
			       m_name.c_str(),
			       100.0 / fsec.count(),
			       m_slowest_frame.count() * 1000);
			m_prev_print = now;
			m_slowest_frame = std::chrono::duration<float>::min();
		}

		m_prev_frame = now;

		queue_next();
	}

	static unsigned get_bar_pos(MappedFramebuffer* fb, unsigned frame_num)
	{
		return (frame_num * bar_speed) % (fb->width() - bar_width + 1);
	}

	static void draw_bar(MappedFramebuffer* fb, unsigned frame_num)
	{
		int old_xpos = frame_num < s_num_buffers ? -1 : get_bar_pos(fb, frame_num - s_num_buffers);
		int new_xpos = get_bar_pos(fb, frame_num);

		draw_color_bar(*fb, old_xpos, new_xpos, bar_width);
		draw_text(*fb, fb->width() / 2, 0, to_string(frame_num), RGB(255, 255, 255));
	}

	static void do_flip_output(AtomicReq& req, unsigned frame_num, const OutputInfo& o)
	{
		unsigned cur = frame_num % s_num_buffers;

		if (!o.fbs.empty()) {
			auto fb = o.fbs[cur];

			draw_bar(fb, frame_num);

			req.add(o.primary_plane, {
					{ "FB_ID", fb->id() },
				});
		}

		for (const PlaneInfo& p : o.planes) {
			auto fb = p.fbs[cur];

			draw_bar(fb, frame_num);

			req.add(p.plane, {
					{ "FB_ID", fb->id() },
				});
		}
	}

	void do_flip_output_legacy(unsigned frame_num, const OutputInfo& o)
	{
		unsigned cur = frame_num % s_num_buffers;

		if (!o.fbs.empty()) {
			auto fb = o.fbs[cur];

			draw_bar(fb, frame_num);

			int r = o.crtc->page_flip(*fb, this);
			ASSERT(r == 0);
		}

		for (const PlaneInfo& p : o.planes) {
			auto fb = p.fbs[cur];

			draw_bar(fb, frame_num);

			int r = o.crtc->set_plane(p.plane, *fb,
						  p.x, p.y, p.w, p.h,
						  0, 0, fb->width(), fb->height());
			ASSERT(r == 0);
		}
	}

	void queue_next()
	{
		if (m_card.has_atomic()) {
			AtomicReq req(m_card);

			for (auto o : m_outputs)
				do_flip_output(req, m_frame_num, *o);

			int r = req.commit(this);
			if (r)
				EXIT("Flip commit failed: %d\n", r);
		} else {
			ASSERT(m_outputs.size() == 1);
			do_flip_output_legacy(m_frame_num, *m_outputs[0]);
		}
	}

	Card& m_card;
	string m_name;
	vector<const OutputInfo*> m_outputs;
	unsigned m_frame_num;

	chrono::steady_clock::time_point m_prev_print;
	chrono::steady_clock::time_point m_prev_frame;
	chrono::duration<float> m_slowest_frame;

	static const unsigned bar_width = 20;
	static const unsigned bar_speed = 8;
};

static void main_flip(Card& card, const vector<OutputInfo>& outputs)
{
	fd_set fds;

	FD_ZERO(&fds);

	int fd = card.fd();

	vector<unique_ptr<FlipState>> flipstates;

	if (!s_flip_sync) {
		for (const OutputInfo& o : outputs) {
			auto fs = unique_ptr<FlipState>(new FlipState(card, to_string(o.connector->idx()), { &o }));
			flipstates.push_back(move(fs));
		}
	} else {
		vector<const OutputInfo*> ois;

		string name;
		for (const OutputInfo& o : outputs) {
			name += to_string(o.connector->idx()) + ",";
			ois.push_back(&o);
		}

		auto fs = unique_ptr<FlipState>(new FlipState(card, name, ois));
		flipstates.push_back(move(fs));
	}

	for (unique_ptr<FlipState>& fs : flipstates)
		fs->start_flipping();

	while (true) {
		int r;

		FD_SET(0, &fds);
		FD_SET(fd, &fds);

		r = select(fd + 1, &fds, NULL, NULL, NULL);
		if (r < 0) {
			fprintf(stderr, "select() failed with %d: %m\n", errno);
			break;
		} else if (FD_ISSET(0, &fds)) {
			fprintf(stderr, "Exit due to user-input\n");
			break;
		} else if (FD_ISSET(fd, &fds)) {
			card.call_page_flip_handlers();
		}
	}
}

int main(int argc, char **argv)
{
	vector<Arg> output_args = parse_cmdline(argc, argv);

	Card card(s_device_path);

	if (!card.has_atomic() && s_flip_sync)
		EXIT("Synchronized flipping requires atomic modesetting");

	vector<OutputInfo> outputs = setups_to_outputs(card, output_args);

	ResourceManager resman(card);

	if (card.has_atomic()) {
		for (OutputInfo& o : outputs) {
			o.primary_plane = resman.reserve_primary_plane(o.crtc);

			if (!o.fbs.empty() && !o.primary_plane)
				EXIT("Could not get primary plane for crtc '%u'", o.crtc->id());
		}
	}

	if (!s_flip_mode)
		draw_test_patterns(outputs);

	print_outputs(outputs);

	if (card.has_atomic())
		set_crtcs_n_planes(card, outputs);
	else
		set_crtcs_n_planes_legacy(card, outputs);

	printf("press enter to exit\n");

	if (s_flip_mode)
		main_flip(card, outputs);
	else
		getchar();
}
