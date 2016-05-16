#include <cstdio>
#include <algorithm>
#include <regex>
#include <set>

#include "kms++.h"

#include "test.h"
#include "opts.h"

using namespace std;
using namespace kms;

struct PlaneInfo
{
	Plane* plane;

	unsigned x;
	unsigned y;
	unsigned w;
	unsigned h;

	DumbFramebuffer* fb;
};

struct OutputInfo
{
	Connector* connector;

	Crtc* crtc;
	Videomode mode;
	bool user_set_crtc;
	DumbFramebuffer* fb;

	vector<PlaneInfo> planes;
};

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
	Connector* conn = nullptr;

	auto connectors = card.get_connectors();

	if (str[0] == '@') {
		char* endptr;
		unsigned idx = strtoul(str.c_str() + 1, &endptr, 10);
		if (*endptr == 0) {
			if (idx >= connectors.size())
				EXIT("Bad connector number '%u'", idx);

			conn = connectors[idx];
		}
	} else {
		char* endptr;
		unsigned id = strtoul(str.c_str(), &endptr, 10);
		if (*endptr == 0) {
			Connector* c = card.get_connector(id);
			if (!c)
				EXIT("Bad connector id '%u'", id);

			conn = c;
		}
	}

	if (!conn) {
		auto iter = find_if(connectors.begin(), connectors.end(), [&str](Connector *c) { return c->fullname() == str; });
		if (iter != connectors.end())
			conn = *iter;
	}

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
	// @12:1920x1200@60
	const regex mode_re("(?:(@?)(\\d+):)?(?:(\\d+)x(\\d+)(i)?)(?:@(\\d+))?");

	smatch sm;
	if (!regex_match(crtc_str, sm, mode_re))
		EXIT("Failed to parse crtc option '%s'", crtc_str.c_str());

	if (sm[2].matched) {
		bool use_idx = sm[1].length() == 1;
		unsigned num = stoul(sm[2].str());

		if (use_idx) {
			auto crtcs = card.get_crtcs();

			if (num >= crtcs.size())
				EXIT("Bad crtc number '%u'", num);

			output.crtc = crtcs[num];
		} else {
			Crtc* c = card.get_crtc(num);
			if (!c)
				EXIT("Bad crtc id '%u'", num);

			output.crtc = c;
		}
	} else {
		output.crtc = output.connector->get_current_crtc();
	}

	unsigned w = stoul(sm[3]);
	unsigned h = stoul(sm[4]);
	bool ilace = sm[5].matched ? true : false;
	unsigned refresh = sm[6].matched ? stoul(sm[6]) : 0;

	output.mode = output.connector->get_mode(w, h, refresh, ilace);
}

static void parse_plane(Card& card, const string& plane_str, const OutputInfo& output, PlaneInfo& pinfo)
{
	// 3:400,400-400x400
	const regex plane_re("(?:(@?)(\\d+):)?(?:(\\d+),(\\d+)-)?(\\d+)x(\\d+)");

	smatch sm;
	if (!regex_match(plane_str, sm, plane_re))
		EXIT("Failed to parse plane option '%s'", plane_str.c_str());

	if (sm[2].matched) {
		bool use_idx = sm[1].length() == 1;
		unsigned num = stoul(sm[2].str());

		if (use_idx) {
			auto planes = card.get_planes();

			if (num >= planes.size())
				EXIT("Bad plane number '%u'", num);

			pinfo.plane = planes[num];
		} else {
			Plane* p = card.get_plane(num);
			if (!p)
				EXIT("Bad plane id '%u'", num);

			pinfo.plane = p;
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

static DumbFramebuffer* get_default_fb(Card& card, unsigned width, unsigned height)
{
	auto fb = new DumbFramebuffer(card, width, height, PixelFormat::XRGB8888);
	draw_test_pattern(*fb);
	return fb;
}

static DumbFramebuffer* parse_fb(Card& card, const string& fb_str, unsigned def_w, unsigned def_h)
{
	unsigned w = def_w;
	unsigned h = def_h;
	PixelFormat format = PixelFormat::XRGB8888;

	if (!fb_str.empty()) {
		// XXX the regexp is not quite correct
		// 400x400-NV12
		const regex fb_re("(?:(\\d+)x(\\d+))?(?:-)?(\\w\\w\\w\\w)?");

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

	auto fb = new DumbFramebuffer(card, w, h, format);
	draw_test_pattern(*fb);
	return fb;
}

static const char* usage_str =
		"Usage: testpat [OPTION]...\n\n"
		"Show a test pattern on a display or plane\n\n"
		"Options:\n"
		"      --device=DEVICE       DEVICE is the path to DRM card to open\n"
		"  -c, --connector=CONN      CONN is <connector>\n"
		"  -r, --crtc=CRTC           CRTC is [<crtc>:]<w>x<h>[@<Hz>]\n"
		"  -p, --plane=PLANE         PLANE is [<plane>:][<x>,<y>-]<w>x<h>\n"
		"  -f, --fb=FB               FB is [<w>x<h>][-][<4cc>]\n"
		"\n"
		"<connector>, <crtc> and <plane> can be given by id (<id>) or index (@<idx>).\n"
		"<connector> can also be given by name.\n"
		"\n"
		"Options can be given multiple times to set up multiple displays or planes.\n"
		"Options may apply to previous options, e.g. a plane will be set on a crtc set in\n"
		"an earlier option.\n"
		"If you omit parameters, testpat tries to guess what you mean\n"
		"\n"
		"Examples:\n"
		"\n"
		"Set eDP-1 mode to 1920x1080@60, show XR24 framebuffer on the crtc, and a 400x400 XB24 plane:\n"
		"    testpat -c eDP-1 -r 1920x1080@60 -f XR24 -p 400x400 -f XB24\n\n"
		"XR24 framebuffer on first connected connector in the default mode:\n"
		"    testpat -f XR24\n\n"
		"XR24 framebuffer on a 400x400 plane on the first connected connector in the default mode:\n"
		"    testpat -p 400x400 -f XR24\n\n"
		"Test pattern on the second connector with default mode:\n"
		"    testpat -c @1\n"
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

			output.fb = get_default_fb(card, output.mode.hdisplay, output.mode.vdisplay);

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

			auto fb = parse_fb(card, arg.arg, def_w, def_h);

			if (current_plane)
				current_plane->fb = fb;
			else
				current_output->fb = fb;

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

		if (!o.fb && o.user_set_crtc)
			o.fb = get_default_fb(card, o.mode.hdisplay, o.mode.vdisplay);

		for (PlaneInfo &p : o.planes) {
			if (!p.fb)
				p.fb = get_default_fb(card, p.w, p.h);
		}
	}

	return outputs;
}

static std::string videomode_to_string(const Videomode& mode)
{
	unsigned hfp = mode.hsync_start - mode.hdisplay;
	unsigned hsw = mode.hsync_end - mode.hsync_start;
	unsigned hbp = mode.htotal - mode.hsync_end;

	unsigned vfp = mode.vsync_start - mode.vdisplay;
	unsigned vsw = mode.vsync_end - mode.vsync_start;
	unsigned vbp = mode.vtotal - mode.vsync_end;

	float hz = (mode.clock * 1000.0) / (mode.htotal * mode.vtotal);
	if (mode.flags & (1<<4)) // XXX interlace
		hz *= 2;

	char buf[256];

	sprintf(buf, "%.2f MHz %u/%u/%u/%u %u/%u/%u/%u %uHz (%.2fHz)",
		mode.clock / 1000.0,
		mode.hdisplay, hfp, hsw, hbp,
		mode.vdisplay, vfp, vsw, vbp,
		mode.vrefresh, hz);

	return std::string(buf);
}

static void print_outputs(const vector<OutputInfo>& outputs)
{
	for (unsigned i = 0; i < outputs.size(); ++i) {
		const OutputInfo& o = outputs[i];

		printf("Connector %u/@%u: %s\n", o.connector->id(), o.connector->idx(),
		       o.connector->fullname().c_str());
		printf("  Crtc %u/@%u: %ux%u-%u (%s)\n", o.crtc->id(), o.crtc->idx(),
		       o.mode.hdisplay, o.mode.vdisplay, o.mode.vrefresh,
		       videomode_to_string(o.mode).c_str());
		if (o.fb)
			printf("    Fb %ux%u-%s\n", o.fb->width(), o.fb->height(),
			       PixelFormatToFourCC(o.fb->format()).c_str());

		for (unsigned j = 0; j < o.planes.size(); ++j) {
			const PlaneInfo& p = o.planes[j];
			printf("  Plane %u/@%u: %u,%u-%ux%u\n", p.plane->id(), p.plane->idx(),
			       p.x, p.y, p.w, p.h);
			printf("    Fb %ux%u-%s\n", p.fb->width(), p.fb->height(),
			       PixelFormatToFourCC(p.fb->format()).c_str());
		}
	}
}

static void set_crtcs_n_planes(Card& card, const vector<OutputInfo>& outputs)
{
	for (const OutputInfo& o : outputs) {
		auto conn = o.connector;
		auto crtc = o.crtc;

		if (o.fb) {
			int r = crtc->set_mode(conn, *o.fb, o.mode);
			if (r)
				printf("crtc->set_mode() failed for crtc %u: %s\n",
				       crtc->id(), strerror(-r));
		}

		for (const PlaneInfo& p : o.planes) {
			int r = crtc->set_plane(p.plane, *p.fb,
						p.x, p.y, p.w, p.h,
						0, 0, p.fb->width(), p.fb->height());
			if (r)
				printf("crtc->set_plane() failed for plane %u: %s\n",
				       p.plane->id(), strerror(-r));
		}
	}
}

int main(int argc, char **argv)
{
	vector<Arg> output_args = parse_cmdline(argc, argv);

	Card card(s_device_path);

	vector<OutputInfo> outputs = setups_to_outputs(card, output_args);

	print_outputs(outputs);

	set_crtcs_n_planes(card, outputs);

	printf("press enter to exit\n");

	getchar();
}
