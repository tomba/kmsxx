#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <iostream>
#include <string>
#include <unistd.h>
#include <fmt/format.h>

#include <kms++/kms++.h>
#include <kms++util/kms++util.h>

using namespace std;
using namespace kms;

static struct {
	bool print_props;
	bool print_modes;
	bool print_list;
	bool x_modeline;
} s_opts;

static string format_mode(const Videomode& m, unsigned idx)
{
	string str;

	str = fmt::format("  {:2} ", idx);

	if (s_opts.x_modeline) {
		str += fmt::format("{:12} {:6} {:4} {:4} {:4} {:4} {:4} {:4} {:4} {:4} {:3} {:#x} {:#x}",
				   m.name,
				   m.clock,
				   m.hdisplay, m.hsync_start, m.hsync_end, m.htotal,
				   m.vdisplay, m.vsync_start, m.vsync_end, m.vtotal,
				   m.vrefresh,
				   m.flags,
				   m.type);
	} else {
		str += m.to_string_long_padded();
	}

	return str;
}

static string format_mode_short(const Videomode& m)
{
	return m.to_string_long();
}

static string format_connector(Connector& c)
{
	string str;

	str = fmt::format("Connector {} ({}) {}",
			  c.idx(), c.id(), c.fullname());

	switch (c.connector_status()) {
	case ConnectorStatus::Connected:
		str += " (connected)";
		break;
	case ConnectorStatus::Disconnected:
		str += " (disconnected)";
		break;
	default:
		str += " (unknown)";
		break;
	}

	return str;
}

static string format_encoder(Encoder& e)
{
	return fmt::format("Encoder {} ({}) {}",
			   e.idx(), e.id(), e.get_encoder_type());
}

static string format_crtc(Crtc& c)
{
	string str;

	str = fmt::format("Crtc {} ({})", c.idx(), c.id());

	if (c.mode_valid())
		str += " " + format_mode_short(c.mode());

	return str;
}

static string format_plane(Plane& p)
{
	string str;

	str = fmt::format("Plane {} ({})", p.idx(), p.id());

	if (p.fb_id())
		str += fmt::format(" fb-id: {}", p.fb_id());

	string crtcs = join<Crtc*>(p.get_possible_crtcs(), " ", [](Crtc* crtc) { return to_string(crtc->idx()); });

	str += fmt::format(" (crtcs: {})", crtcs);

	if (p.card().has_atomic()) {
		str += fmt::format(" {},{} {}x{} -> {},{} {}x{}",
				   (uint32_t)p.get_prop_value("SRC_X") >> 16,
				   (uint32_t)p.get_prop_value("SRC_Y") >> 16,
				   (uint32_t)p.get_prop_value("SRC_W") >> 16,
				   (uint32_t)p.get_prop_value("SRC_H") >> 16,
				   (int32_t)p.get_prop_value("CRTC_X"),
				   (int32_t)p.get_prop_value("CRTC_Y"),
				   (uint32_t)p.get_prop_value("CRTC_W"),
				   (uint32_t)p.get_prop_value("CRTC_H"));
	}

	string fmts = join<PixelFormat>(p.get_formats(), " ", [](PixelFormat fmt) { return PixelFormatToFourCC(fmt); });

	str += fmt::format(" ({})", fmts);

	return str;
}

static string format_fb(Framebuffer& fb)
{
	return fmt::format("FB {} {}x{} {}",
			   fb.id(), fb.width(), fb.height(),
			   PixelFormatToFourCC(fb.format()));
}

static string format_property(const Property* prop, uint64_t val)
{
	string ret = fmt::format("{} ({}) = ", prop->name(), prop->id());

	switch (prop->type()) {
	case PropertyType::Bitmask: {
		vector<string> v, vall;

		for (auto kvp : prop->get_enums()) {
			if (val & (1 << kvp.first))
				v.push_back(kvp.second);
			vall.push_back(fmt::format("{}={:#x}", kvp.second, 1 << kvp.first));
		}

		// XXX
		ret += fmt::format("{:#x} ({}) [{}]", val, join(v, "|"), join(vall, "|"));

		break;
	}

	case PropertyType::Blob: {
		uint32_t blob_id = (uint32_t)val;

		if (blob_id) {
			Blob blob(prop->card(), blob_id);
			auto data = blob.data();

			ret += fmt::format("blob-id {} len {}", blob_id, data.size());
		} else {
			ret += fmt::format("blob-id {}", blob_id);
		}

		break;
	}

	case PropertyType::Enum: {
		string cur;
		vector<string> vall;

		for (auto kvp : prop->get_enums()) {
			if (val == kvp.first)
				cur = kvp.second;
			vall.push_back(fmt::format("{}={}", kvp.second, kvp.first));
		}

		ret += fmt::format("{} ({}) [{}]", val, cur, join(vall, "|"));

		break;
	}

	case PropertyType::Object: {
		ret += fmt::format("object id {}", val);
		break;
	}

	case PropertyType::Range: {
		auto values = prop->get_values();

		ret += fmt::format("{} [{} - {}]",
				   val, values[0], values[1]);

		break;
	}

	case PropertyType::SignedRange: {
		auto values = prop->get_values();

		ret += fmt::format("{} [{} - {}]",
				   (int64_t)val, (int64_t)values[0], (int64_t)values[1]);

		break;
	}
	}

	if (prop->is_pending())
		ret += " (pending)";
	if (prop->is_immutable())
		ret += " (immutable)";

	return ret;
}

static vector<string> format_props(DrmPropObject* o)
{
	vector<string> lines;

	auto pmap = o->get_prop_map();
	for (auto pp : pmap) {
		const Property* p = o->card().get_prop(pp.first);
		lines.push_back(format_property(p, pp.second));
	}

	return lines;
}

static string format_ob(DrmObject* ob)
{
	if (auto o = dynamic_cast<Connector*>(ob))
		return format_connector(*o);
	else if (auto o = dynamic_cast<Encoder*>(ob))
		return format_encoder(*o);
	else if (auto o = dynamic_cast<Crtc*>(ob))
		return format_crtc(*o);
	else if (auto o = dynamic_cast<Plane*>(ob))
		return format_plane(*o);
	else if (auto o = dynamic_cast<Framebuffer*>(ob))
		return format_fb(*o);
	else
		EXIT("Unkown DRM Object type\n");
}

template<class T>
vector<T> filter(const vector<T>& sequence, function<bool(T)> predicate)
{
	vector<T> result;

	for (auto it = sequence.begin(); it != sequence.end(); ++it)
		if (predicate(*it))
			result.push_back(*it);

	return result;
}

struct Entry {
	string title;
	vector<string> lines;
	vector<Entry> children;
};

static Entry& add_entry(vector<Entry>& entries)
{
	entries.emplace_back();
	return entries.back();
}
/*
static bool on_tty()
{
	return isatty(STDOUT_FILENO) > 0;
}
*/
enum class TreeGlyphMode {
	None,
	ASCII,
	UTF8,
};

static TreeGlyphMode s_glyph_mode = TreeGlyphMode::None;

enum class TreeGlyph {
	Vertical,
	Branch,
	Right,
	Space,
};

static const map<TreeGlyph, string> glyphs_utf8 = {
	{ TreeGlyph::Vertical, "│ " },
	{ TreeGlyph::Branch, "├─" },
	{ TreeGlyph::Right, "└─" },
	{ TreeGlyph::Space, "  " },

};

static const map<TreeGlyph, string> glyphs_ascii = {
	{ TreeGlyph::Vertical, "| " },
	{ TreeGlyph::Branch, "|-" },
	{ TreeGlyph::Right, "`-" },
	{ TreeGlyph::Space, "  " },

};

const string& get_glyph(TreeGlyph glyph)
{
	static const string s_empty = "  ";

	if (s_glyph_mode == TreeGlyphMode::None)
		return s_empty;

	const map<TreeGlyph, string>& glyphs = s_glyph_mode == TreeGlyphMode::UTF8 ? glyphs_utf8 : glyphs_ascii;

	return glyphs.at(glyph);
}

static void print_entry(const Entry& e, const string& prefix, bool is_child, bool is_last)
{
	string prefix1;
	string prefix2;

	if (is_child) {
		prefix1 = prefix + (is_last ? get_glyph(TreeGlyph::Right) : get_glyph(TreeGlyph::Branch));
		prefix2 = prefix + (is_last ? get_glyph(TreeGlyph::Space) : get_glyph(TreeGlyph::Vertical));
	}

	fmt::print("{}{}\n", prefix1, e.title);

	bool has_children = e.children.size() > 0;

	string data_prefix = prefix2 + (has_children ? get_glyph(TreeGlyph::Vertical) : get_glyph(TreeGlyph::Space));

	for (const string& str : e.lines) {
		string p = data_prefix + get_glyph(TreeGlyph::Space);
		fmt::print("{}{}\n", p, str);
	}

	for (const Entry& child : e.children) {
		bool is_last = &child == &e.children.back();

		print_entry(child, prefix2, true, is_last);
	}
}

static void print_entries(const vector<Entry>& entries, const string& prefix)
{
	for (const Entry& e : entries) {
		print_entry(e, "", false, false);
	}
}

template<class T>
static void append(vector<DrmObject*>& dst, const vector<T*>& src)
{
	dst.insert(dst.end(), src.begin(), src.end());
}

static void print_as_list(Card& card)
{
	vector<DrmPropObject*> obs;
	vector<Framebuffer*> fbs;

	for (Connector* conn : card.get_connectors()) {
		obs.push_back(conn);
	}

	for (Encoder* enc : card.get_encoders()) {
		obs.push_back(enc);
	}

	for (Crtc* crtc : card.get_crtcs()) {
		obs.push_back(crtc);
		if (crtc->buffer_id() && !card.has_universal_planes()) {
			Framebuffer* fb = new Framebuffer(card, crtc->buffer_id());
			fbs.push_back(fb);
		}
	}

	for (Plane* plane : card.get_planes()) {
		obs.push_back(plane);
		if (plane->fb_id()) {
			Framebuffer* fb = new Framebuffer(card, plane->fb_id());
			fbs.push_back(fb);
		}
	}

	for (DrmPropObject* ob : obs) {
		fmt::print("{}\n", format_ob(ob));

		if (s_opts.print_props) {
			for (string str : format_props(ob))
				fmt::print("    {}\n", str);
		}
	}

	for (Framebuffer* fb : fbs) {
		fmt::print("{}\n", format_ob(fb));
	}
}

static void print_as_tree(Card& card)
{
	vector<Entry> entries;

	for (Connector* conn : card.get_connectors()) {
		Entry& e1 = add_entry(entries);
		e1.title = format_ob(conn);
		if (s_opts.print_props)
			e1.lines = format_props(conn);

		for (Encoder* enc : conn->get_encoders()) {
			Entry& e2 = add_entry(e1.children);
			e2.title = format_ob(enc);
			if (s_opts.print_props)
				e2.lines = format_props(enc);

			if (Crtc* crtc = enc->get_crtc()) {
				Entry& e3 = add_entry(e2.children);
				e3.title = format_ob(crtc);
				if (s_opts.print_props)
					e3.lines = format_props(crtc);

				if (crtc->buffer_id() && !card.has_universal_planes()) {
					Framebuffer fb(card, crtc->buffer_id());
					Entry& e5 = add_entry(e3.children);

					e5.title = format_ob(&fb);
				}

				for (Plane* plane : card.get_planes()) {
					if (plane->crtc_id() != crtc->id())
						continue;

					Entry& e4 = add_entry(e3.children);
					e4.title = format_ob(plane);
					if (s_opts.print_props)
						e4.lines = format_props(plane);

					uint32_t fb_id = plane->fb_id();
					if (fb_id) {
						Framebuffer fb(card, fb_id);

						Entry& e5 = add_entry(e4.children);

						e5.title = format_ob(&fb);
					}
				}
			}
		}
	}

	print_entries(entries, "");
}

static void print_modes(Card& card)
{
	for (Connector* conn : card.get_connectors()) {
		if (!conn->connected())
			continue;

		fmt::print("{}\n", format_ob(conn));

		auto modes = conn->get_modes();
		for (unsigned i = 0; i < modes.size(); ++i)
			fmt::print("{}\n", format_mode(modes[i], i));
	}
}

static const char* usage_str =
	"Usage: kmsprint [OPTIONS]\n\n"
	"Options:\n"
	"      --device=DEVICE     DEVICE is the path to DRM card to open\n"
	"  -l, --list              Print list instead of tree\n"
	"  -m, --modes             Print modes\n"
	"      --xmode             Print modes using X modeline\n"
	"  -p, --props             Print properties\n";

static void usage()
{
	puts(usage_str);
}

int main(int argc, char** argv)
{
	string dev_path;

	OptionSet optionset = {
		Option("|device=", [&dev_path](string s) {
			dev_path = s;
		}),
		Option("l|list", []() {
			s_opts.print_list = true;
		}),
		Option("m|modes", []() {
			s_opts.print_modes = true;
		}),
		Option("p|props", []() {
			s_opts.print_props = true;
		}),
		Option("|xmode", []() {
			s_opts.x_modeline = true;
		}),
		Option("h|help", []() {
			usage();
			exit(-1);
		}),
	};

	optionset.parse(argc, argv);

	if (optionset.params().size() > 0) {
		usage();
		exit(-1);
	}

	Card card(dev_path);

	if (s_opts.print_modes) {
		print_modes(card);
		return 0;
	}

	if (s_opts.print_list)
		print_as_list(card);
	else
		print_as_tree(card);
}
