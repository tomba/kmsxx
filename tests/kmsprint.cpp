#include <cstdio>
#include <algorithm>
#include <iostream>

#include "kms++.h"
#include "cmdoptions.h"

using namespace std;
using namespace kms;

namespace kmsprint {

string width(int w, string str)
{
	str.resize(w, ' ');
	return str;
}

void print_mode(const Videomode &m, int ind)
{
	printf("%s%s %6d %4d %4d %4d %4d %d %4d %4d %4d %4d %d  %2d 0x%04x %2d\n",
	       width(ind, "").c_str(),
	       m.name[0] == '\0' ? "" : width(11, m.name).c_str(),
	       m.clock,
	       m.hdisplay,
	       m.hsync_start,
	       m.hsync_end,
	       m.htotal,
	       m.hskew,
	       m.vdisplay,
	       m.vsync_start,
	       m.vsync_end,
	       m.vtotal,
	       m.vscan,
	       m.vrefresh,
	       m.flags,
	       m.type);
}

void print_property(uint64_t val, const Property& p, int ind)
{
	printf("%s%s (id %d) = %s\n", width(ind, "").c_str(),
	       p.name().c_str(), p.id(), p.to_str(val).c_str());
}

void print_properties(DrmObject& o, int ind)
{
	auto pmap = o.get_prop_map();
	printf("%sProperties, %u in total:\n", width(ind, "").c_str(),
	       (unsigned) pmap.size());
	for (auto pp : pmap) {
		const Property& p = *o.card().get_prop(pp.first);
		print_property(pp.second, p, ind + 2);
	}
}

void print_plane(Plane& p, int ind, const CmdOptions& opts)
{
	printf("%sPlane Id %d %d,%d -> %dx%d formats:", width(ind, "").c_str(),
	       p.id(), p.crtc_x(), p.crtc_y(), p.x(), p.y());
	for (auto f : p.get_formats())
		printf(" %s", PixelFormatToFourCC(f).c_str());
	printf("\n");

	if (opts.is_set("p"))
		print_properties(p, ind+2);
}

void print_crtc(Crtc& cc, int ind, const CmdOptions& opts)
{
	printf("%sCRTC Id %d BufferId %d %dx%d at %dx%d gamma_size %d\n",
	       width(ind, "").c_str(), cc.id(), cc.buffer_id(), cc.width(),
	       cc.height(), cc.x(), cc.y(), cc.gamma_size());

	printf("%s   Mode ", width(ind, "").c_str());
	print_mode(cc.mode(), 0);

	if (opts.is_set("p"))
		print_properties(cc, ind+2);

	if (opts.is_set("r"))
		for (auto p : cc.get_possible_planes())
			print_plane(*p, ind + 2, opts);
}

void print_encoder(Encoder& e, int ind, const CmdOptions& opts)
{
	printf("%sEncoder Id %d type %s\n", width(ind, "").c_str(),
	       e.id(), e.get_encoder_type().c_str());

	if (opts.is_set("p"))
		print_properties(e, ind+2);

	if (opts.is_set("r"))
		for (auto cc : e.get_possible_crtcs())
			print_crtc(*cc, ind + 2, opts);
}

void print_connector(Connector& c, int ind, const CmdOptions& opts)
{
	printf("%sConnector %s Id %d %sconnected", width(ind, "").c_str(),
	       c.fullname().c_str(), c.id(), c.connected() ? "" : "dis");
	if (c.subpixel() != 0)
		printf(" Subpixel: %s", c.subpixel_str().c_str());
	printf("\n");

	if (opts.is_set("p"))
		print_properties(c, ind+2);

	if (opts.is_set("r"))
		for (auto enc : c.get_encoders())
			print_encoder(*enc, ind + 2, opts);

	if (opts.is_set("m")) {
		auto modes = c.get_modes();
		printf("%sModes, %u in total:\n", width(ind + 2, "").c_str(),
		       (unsigned) modes.size());
		for (auto mode : modes)
			print_mode(mode, ind + 3);
	}
}

}

static map<string, CmdOption> options = {
	{ "-id", HAS_PARAM("Object id to print") },
	{ "p", NO_PARAM("Print properties") },
	{ "m", NO_PARAM("Print modes") },
	{ "r", NO_PARAM("Recursively print all related objects") },
};

using namespace kmsprint;

int main(int argc, char **argv)
{
	Card card;
	CmdOptions opts(argc, argv, options);

	if (opts.error().length()) {
		cerr << opts.error() << opts.usage();
		return -1;
	}

	/* No options implyles recursion */
	if (!opts.is_set("-id")) {
		opts.get_option("r").oset();
		for (auto conn : card.get_connectors())
			print_connector(*conn, 0, opts);
		return 0;
	}

	if (opts.is_set("-id")) {
		auto ob = card.get_object(atoi(opts.opt_param("-id").c_str()));
		if (!ob) {
			cerr << opts.cmd() << ": Object id " <<
				opts.opt_param("-id") << " not found." << endl;
			return -1;
		}

		if (auto co = dynamic_cast<Connector*>(ob))
			print_connector(*co, 0, opts);
		else if (auto en = dynamic_cast<Encoder*>(ob))
			print_encoder(*en, 0, opts);
		else if (auto cr = dynamic_cast<Crtc*>(ob))
			print_crtc(*cr, 0, opts);
		else if (auto pl = dynamic_cast<Plane*>(ob))
			print_plane(*pl, 0, opts);
		else {
			cerr << opts.cmd() << ": Unkown DRM Object type" <<
				endl;
			return -1;
		}

		return 0;
	}
}
