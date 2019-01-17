#include <cstdio>
#include <cstring>
#include <algorithm>
#include <regex>
#include <set>
#include <chrono>
#include <cstdint>
#include <cinttypes>

#include <sys/select.h>

#include <kms++/kms++.h>
#include <kms++/modedb.h>
#include <kms++/mode_cvt.h>

#include <kms++util/kms++util.h>

using namespace std;
using namespace kms;

struct PropInfo {
	PropInfo(string n, uint64_t v) : prop(NULL), name(n), val(v) {}

	Property *prop;
	string name;
	uint64_t val;
};

struct PlaneInfo
{
	Plane* plane;

	unsigned x;
	unsigned y;
	unsigned w;
	unsigned h;

	unsigned view_x;
	unsigned view_y;
	unsigned view_w;
	unsigned view_h;

	vector<Framebuffer*> fbs;

	vector<PropInfo> props;
};

struct OutputInfo
{
	Connector* connector;

	Crtc* crtc;
	Videomode mode;
	vector<Framebuffer*> legacy_fbs;

	vector<PlaneInfo> planes;

	vector<PropInfo> conn_props;
	vector<PropInfo> crtc_props;
};

static bool s_use_dmt;
static bool s_use_cea;
static unsigned s_num_buffers = 1;
static bool s_flip_mode;
static bool s_flip_sync;
static bool s_cvt;
static bool s_cvt_v2;
static bool s_cvt_vid_opt;
static unsigned s_max_flips;

__attribute__ ((unused))
static void print_regex_match(smatch sm)
{
	for (unsigned i = 0; i < sm.size(); ++i) {
		string str = sm[i].str();
		printf("%u: %s\n", i, str.c_str());
	}
}

static void get_connector(ResourceManager& resman, OutputInfo& output, const string& str = "")
{
	Connector* conn = resman.reserve_connector(str);

	if (!conn)
		EXIT("No connector '%s'", str.c_str());

	if (!conn->connected())
		EXIT("Connector '%s' not connected", conn->fullname().c_str());

	output.connector = conn;
	output.mode = output.connector->get_default_mode();
}

static void get_default_crtc(ResourceManager& resman, OutputInfo& output)
{
	output.crtc = resman.reserve_crtc(output.connector);

	if (!output.crtc)
		EXIT("Could not find available crtc");
}


static PlaneInfo *add_default_planeinfo(OutputInfo* output)
{
	output->planes.push_back(PlaneInfo { });
	PlaneInfo *ret = &output->planes.back();
	ret->w = output->mode.hdisplay;
	ret->h = output->mode.vdisplay;
	return ret;
}

static void parse_crtc(ResourceManager& resman, Card& card, const string& crtc_str, OutputInfo& output)
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

	if (output.crtc)
		output.crtc = resman.reserve_crtc(output.crtc);
	else
		output.crtc = resman.reserve_crtc(output.connector);

	if (!output.crtc)
		EXIT("Could not find available crtc");
}

static void parse_plane(ResourceManager& resman, Card& card, const string& plane_str, const OutputInfo& output, PlaneInfo& pinfo)
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

		auto plane = resman.reserve_plane(pinfo.plane);
		if (!plane)
			EXIT("Plane id %u is not available", pinfo.plane->id());
	}

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

static void parse_prop(const string& prop_str, vector<PropInfo> &props)
{
	string name, val;

	size_t split = prop_str.find("=");

	if (split == string::npos)
		EXIT("Equal sign ('=') not found in %s", prop_str.c_str());

	name = prop_str.substr(0, split);
	val = prop_str.substr(split+1);

	props.push_back(PropInfo(name, stoull(val, 0, 0)));
}

static void get_props(Card& card, vector<PropInfo> &props, const DrmPropObject* propobj)
{
	for (auto& pi : props)
		pi.prop = propobj->get_prop(pi.name);
}

static vector<Framebuffer*> get_default_fb(Card& card, unsigned width, unsigned height)
{
	vector<Framebuffer*> v;

	for (unsigned i = 0; i < s_num_buffers; ++i)
		v.push_back(new DumbFramebuffer(card, width, height, PixelFormat::XRGB8888));

	return v;
}

static void parse_fb(Card& card, const string& fb_str, OutputInfo* output, PlaneInfo* pinfo)
{
	unsigned w, h;
	PixelFormat format = PixelFormat::XRGB8888;

	if (pinfo) {
		w = pinfo->w;
		h = pinfo->h;
	} else {
		w = output->mode.hdisplay;
		h = output->mode.vdisplay;
	}

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

	vector<Framebuffer*> v;

	for (unsigned i = 0; i < s_num_buffers; ++i)
		v.push_back(new DumbFramebuffer(card, w, h, format));

	if (pinfo)
		pinfo->fbs = v;
	else
		output->legacy_fbs = v;
}

static void parse_view(const string& view_str, PlaneInfo& pinfo)
{
	const regex view_re("(\\d+),(\\d+)-(\\d+)x(\\d+)");		// 400,400-400x400

	smatch sm;
	if (!regex_match(view_str, sm, view_re))
		EXIT("Failed to parse view option '%s'", view_str.c_str());

	pinfo.view_x = stoul(sm[1]);
	pinfo.view_y = stoul(sm[2]);
	pinfo.view_w = stoul(sm[3]);
	pinfo.view_h = stoul(sm[4]);
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
		"  -v, --view=VIEW           VIEW is <x>,<y>-<w>x<h>\n"
		"  -P, --property=PROP=VAL   Set PROP to VAL in the previous DRM object\n"
		"      --dmt                 Search for the given mode from DMT tables\n"
		"      --cea                 Search for the given mode from CEA tables\n"
		"      --cvt=CVT             Create videomode with CVT. CVT is 'v1', 'v2' or 'v2o'\n"
		"      --flip[=max]          Do page flipping for each output with an optional maximum flips count\n"
		"      --sync                Synchronize page flipping\n"
		"\n"
		"<connector>, <crtc> and <plane> can be given by index (<idx>) or id (@<id>).\n"
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
		"\n"
		"Environmental variables:\n"
		"    KMSXX_DISABLE_UNIVERSAL_PLANES    Don't enable universal planes even if available\n"
		"    KMSXX_DISABLE_ATOMIC              Don't enable atomic modesetting even if available\n"
		;

static void usage()
{
	puts(usage_str);
}

enum class ArgType
{
	Connector,
	Crtc,
	Plane,
	Framebuffer,
	View,
	Property,
};

struct Arg
{
	ArgType type;
	string arg;
};

static string s_device_path;

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
			args.push_back(Arg { ArgType::Connector, s });
		}),
		Option("r|crtc=", [&](string s)
		{
			args.push_back(Arg { ArgType::Crtc, s });
		}),
		Option("p|plane=", [&](string s)
		{
			args.push_back(Arg { ArgType::Plane, s });
		}),
		Option("f|fb=", [&](string s)
		{
			args.push_back(Arg { ArgType::Framebuffer, s });
		}),
		Option("v|view=", [&](string s)
		{
			args.push_back(Arg { ArgType::View, s });
		}),
		Option("P|property=", [&](string s)
		{
			args.push_back(Arg { ArgType::Property, s });
		}),
		Option("|dmt", []()
		{
			s_use_dmt = true;
		}),
		Option("|cea", []()
		{
			s_use_cea = true;
		}),
		Option("|flip?", [&](string s)
		{
			s_flip_mode = true;
			s_num_buffers = 2;
			if (!s.empty())
				s_max_flips = stoi(s);
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

static vector<OutputInfo> setups_to_outputs(Card& card, ResourceManager& resman, const vector<Arg>& output_args)
{
	vector<OutputInfo> outputs;

	OutputInfo* current_output = 0;
	PlaneInfo* current_plane = 0;

	for (auto& arg : output_args) {
		switch (arg.type) {
		case ArgType::Connector:
		{
			outputs.push_back(OutputInfo { });
			current_output = &outputs.back();

			get_connector(resman, *current_output, arg.arg);
			current_plane = 0;

			break;
		}

		case ArgType::Crtc:
		{
			if (!current_output) {
				outputs.push_back(OutputInfo { });
				current_output = &outputs.back();
			}

			if (!current_output->connector)
				get_connector(resman, *current_output);

			parse_crtc(resman, card, arg.arg, *current_output);

			current_plane = 0;

			break;
		}

		case ArgType::Plane:
		{
			if (!current_output) {
				outputs.push_back(OutputInfo { });
				current_output = &outputs.back();
			}

			if (!current_output->connector)
				get_connector(resman, *current_output);

			if (!current_output->crtc)
				get_default_crtc(resman, *current_output);

			current_plane = add_default_planeinfo(current_output);

			parse_plane(resman, card, arg.arg, *current_output, *current_plane);

			break;
		}

		case ArgType::Framebuffer:
		{
			if (!current_output) {
				outputs.push_back(OutputInfo { });
				current_output = &outputs.back();
			}

			if (!current_output->connector)
				get_connector(resman, *current_output);

			if (!current_output->crtc)
				get_default_crtc(resman, *current_output);

			if (!current_plane && card.has_atomic())
				current_plane = add_default_planeinfo(current_output);

			parse_fb(card, arg.arg, current_output, current_plane);

			break;
		}

		case ArgType::View:
		{
			if (!current_plane || current_plane->fbs.empty())
				EXIT("'view' parameter requires a plane and a fb");

			parse_view(arg.arg, *current_plane);
			break;
		}

		case ArgType::Property:
		{
			if (!current_output)
				EXIT("No object to which set the property");

			if (current_plane)
				parse_prop(arg.arg, current_plane->props);
			else if (current_output->crtc)
				parse_prop(arg.arg, current_output->crtc_props);
			else if (current_output->connector)
				parse_prop(arg.arg, current_output->conn_props);
			else
				EXIT("no object");

			break;
		}
		}
	}

	if (outputs.empty()) {
		// no outputs defined, show a pattern on all connected screens
		for (Connector* conn : card.get_connectors()) {
			if (conn->connector_status() != ConnectorStatus::Connected)
				continue;

			OutputInfo output = { };
			output.connector = resman.reserve_connector(conn);
			EXIT_IF(!output.connector, "Failed to reserve connector %s", conn->fullname().c_str());
			output.crtc = resman.reserve_crtc(conn);
			EXIT_IF(!output.crtc, "Failed to reserve crtc for %s", conn->fullname().c_str());
			output.mode = output.connector->get_default_mode();

			outputs.push_back(output);
		}
	}

	for (OutputInfo& o : outputs) {
		get_props(card, o.conn_props, o.connector);

		if (!o.crtc)
			get_default_crtc(resman, o);

		get_props(card, o.crtc_props, o.crtc);

		if (card.has_atomic()) {
			if (o.planes.empty())
				add_default_planeinfo(&o);
		} else {
			if (o.legacy_fbs.empty())
				o.legacy_fbs = get_default_fb(card, o.mode.hdisplay, o.mode.vdisplay);
		}

		for (PlaneInfo &p : o.planes) {
			if (p.fbs.empty())
				p.fbs = get_default_fb(card, p.w, p.h);
		}

		for (PlaneInfo& p : o.planes) {
			if (!p.plane) {
				if (card.has_atomic())
					p.plane = resman.reserve_generic_plane(o.crtc, p.fbs[0]->format());
				else
					p.plane = resman.reserve_overlay_plane(o.crtc, p.fbs[0]->format());

				if (!p.plane)
					EXIT("Failed to find available plane");
			}
			get_props(card, p.props, p.plane);
		}
	}

	return outputs;
}

static char sync_to_char(SyncPolarity pol)
{
	switch (pol) {
	case SyncPolarity::Positive:
		return '+';
	case SyncPolarity::Negative:
		return '-';
	default:
		return '?';
	}
}

static std::string videomode_to_string(const Videomode& m)
{
	string h = sformat("%u/%u/%u/%u/%c", m.hdisplay, m.hfp(), m.hsw(), m.hbp(), sync_to_char(m.hsync()));
	string v = sformat("%u/%u/%u/%u/%c", m.vdisplay, m.vfp(), m.vsw(), m.vbp(), sync_to_char(m.vsync()));

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

		printf("Connector %u/@%u: %s", o.connector->idx(), o.connector->id(),
		       o.connector->fullname().c_str());

		for (const PropInfo &prop: o.conn_props)
			printf(" %s=%" PRIu64, prop.prop->name().c_str(),
			       prop.val);

		printf("\n  Crtc %u/@%u", o.crtc->idx(), o.crtc->id());

		for (const PropInfo &prop: o.crtc_props)
			printf(" %s=%" PRIu64, prop.prop->name().c_str(),
			       prop.val);

		printf(": %s\n", videomode_to_string(o.mode).c_str());

		if (!o.legacy_fbs.empty()) {
			auto fb = o.legacy_fbs[0];
			printf("    Fb %u %ux%u-%s\n", fb->id(), fb->width(), fb->height(), PixelFormatToFourCC(fb->format()).c_str());
		}

		for (unsigned j = 0; j < o.planes.size(); ++j) {
			const PlaneInfo& p = o.planes[j];
			auto fb = p.fbs[0];
			printf("  Plane %u/@%u: %u,%u-%ux%u", p.plane->idx(), p.plane->id(),
			       p.x, p.y, p.w, p.h);
			for (const PropInfo &prop: p.props)
				printf(" %s=%" PRIu64, prop.prop->name().c_str(),
				       prop.val);
			printf("\n");

			printf("    Fb %u %ux%u-%s\n", fb->id(), fb->width(), fb->height(),
			       PixelFormatToFourCC(fb->format()).c_str());
		}
	}
}

static void draw_test_patterns(const vector<OutputInfo>& outputs)
{
	for (const OutputInfo& o : outputs) {
		for (auto fb : o.legacy_fbs)
			draw_test_pattern(*fb);

		for (const PlaneInfo& p : o.planes)
			for (auto fb : p.fbs)
				draw_test_pattern(*fb);
	}
}

static void set_crtcs_n_planes_legacy(Card& card, const vector<OutputInfo>& outputs)
{
	// Disable unused crtcs
	for (Crtc* crtc : card.get_crtcs()) {
		if (find_if(outputs.begin(), outputs.end(), [crtc](const OutputInfo& o) { return o.crtc == crtc; }) != outputs.end())
			continue;

		crtc->disable_mode();
	}

	for (const OutputInfo& o : outputs) {
		int r;
		auto conn = o.connector;
		auto crtc = o.crtc;

		for (const PropInfo& prop : o.conn_props) {
			r = conn->set_prop_value(prop.prop, prop.val);
			EXIT_IF(r, "failed to set connector property %s\n", prop.name.c_str());
		}

		for (const PropInfo& prop : o.crtc_props) {
			r = crtc->set_prop_value(prop.prop, prop.val);
			EXIT_IF(r, "failed to set crtc property %s\n", prop.name.c_str());
		}

		if (!o.legacy_fbs.empty()) {
			auto fb = o.legacy_fbs[0];
			r = crtc->set_mode(conn, *fb, o.mode);
			if (r)
				printf("crtc->set_mode() failed for crtc %u: %s\n",
				       crtc->id(), strerror(-r));
		}

		for (const PlaneInfo& p : o.planes) {
			for (const PropInfo& prop : p.props) {
				r = p.plane->set_prop_value(prop.prop, prop.val);
				EXIT_IF(r, "failed to set plane property %s\n", prop.name.c_str());
			}

			auto fb = p.fbs[0];
			r = crtc->set_plane(p.plane, *fb,
						p.x, p.y, p.w, p.h,
						0, 0, fb->width(), fb->height());
			if (r)
				printf("crtc->set_plane() failed for plane %u: %s\n",
				       p.plane->id(), strerror(-r));
		}
	}
}

static void set_crtcs_n_planes_atomic(Card& card, const vector<OutputInfo>& outputs)
{
	int r;

	// XXX DRM framework doesn't allow moving an active plane from one crtc to another.
	// See drm_atomic.c::plane_switching_crtc().
	// For the time being, disable all crtcs and planes here.

	AtomicReq disable_req(card);

	// Disable unused crtcs
	for (Crtc* crtc : card.get_crtcs()) {
		//if (find_if(outputs.begin(), outputs.end(), [crtc](const OutputInfo& o) { return o.crtc == crtc; }) != outputs.end())
		//	continue;

		disable_req.add(crtc, {
				{ "ACTIVE", 0 },
			});
	}

	// Disable unused planes
	for (Plane* plane : card.get_planes())
		disable_req.add(plane, {
				{ "FB_ID", 0 },
				{ "CRTC_ID", 0 },
			});

	r = disable_req.commit_sync(true);
	if (r)
		EXIT("Atomic commit failed when disabling: %d\n", r);


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

		for (const PropInfo &prop: o.conn_props)
			req.add(conn, prop.prop, prop.val);

		req.add(crtc, {
				{ "ACTIVE", 1 },
				{ "MODE_ID", mode_blob->id() },
			});

		for (const PropInfo &prop: o.crtc_props)
			req.add(crtc, prop.prop, prop.val);

		for (const PlaneInfo& p : o.planes) {
			auto fb = p.fbs[0];

			req.add(p.plane, {
					{ "FB_ID", fb->id() },
					{ "CRTC_ID", crtc->id() },
					{ "SRC_X", (p.view_x ?: 0) << 16 },
					{ "SRC_Y", (p.view_y ?: 0) << 16 },
					{ "SRC_W", (p.view_w ?: fb->width()) << 16 },
					{ "SRC_H", (p.view_h ?: fb->height()) << 16 },
					{ "CRTC_X", p.x },
					{ "CRTC_Y", p.y },
					{ "CRTC_W", p.w },
					{ "CRTC_H", p.h },
				});

			for (const PropInfo &prop: p.props)
				req.add(p.plane, prop.prop, prop.val);
		}
	}

	r = req.test(true);
	if (r)
		EXIT("Atomic test failed: %d\n", r);

	r = req.commit_sync(true);
	if (r)
		EXIT("Atomic commit failed: %d\n", r);
}

static void set_crtcs_n_planes(Card& card, const vector<OutputInfo>& outputs)
{
	if (card.has_atomic())
		set_crtcs_n_planes_atomic(card, outputs);
	else
		set_crtcs_n_planes_legacy(card, outputs);
}

static bool max_flips_reached;

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
		/*
		 * We get flip event for each crtc in this flipstate. We can commit the next frames
		 * only after we've gotten the flip event for all crtcs
		 */
		if (++m_flip_count < m_outputs.size())
			return;

		m_frame_num++;
		if (s_max_flips && m_frame_num >= s_max_flips)
			max_flips_reached = true;

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

	static unsigned get_bar_pos(Framebuffer* fb, unsigned frame_num)
	{
		return (frame_num * bar_speed) % (fb->width() - bar_width + 1);
	}

	static void draw_bar(Framebuffer* fb, unsigned frame_num)
	{
		int old_xpos = frame_num < s_num_buffers ? -1 : get_bar_pos(fb, frame_num - s_num_buffers);
		int new_xpos = get_bar_pos(fb, frame_num);

		draw_color_bar(*fb, old_xpos, new_xpos, bar_width);
		draw_text(*fb, fb->width() / 2, 0, to_string(frame_num), RGB(255, 255, 255));
	}

	static void do_flip_output(AtomicReq& req, unsigned frame_num, const OutputInfo& o)
	{
		unsigned cur = frame_num % s_num_buffers;

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

		if (!o.legacy_fbs.empty()) {
			auto fb = o.legacy_fbs[cur];

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
		m_flip_count = 0;

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
	unsigned m_flip_count;

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

	while (!max_flips_reached) {
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

	if (!card.is_master())
		EXIT("Could not get DRM master permission. Card already in use?");

	if (!card.has_atomic() && s_flip_sync)
		EXIT("Synchronized flipping requires atomic modesetting");

	ResourceManager resman(card);

	vector<OutputInfo> outputs = setups_to_outputs(card, resman, output_args);

	if (!s_flip_mode)
		draw_test_patterns(outputs);

	print_outputs(outputs);

	set_crtcs_n_planes(card, outputs);

	printf("press enter to exit\n");

	if (s_flip_mode)
		main_flip(card, outputs);
	else
		getchar();
}
