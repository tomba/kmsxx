#pragma once

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "layout.h"

namespace pixpat
{

// Pattern-specific parameters parsed from pixpat_pattern_opts::params.
// The wire format is a comma-separated list of "key=value" items;
// whitespace around tokens is trimmed, keys and values are
// case-insensitive ASCII, and neither may contain ',' or '='.
// Malformed input leaves ok() returning false — the pattern dispatcher
// fails the call when that happens.
//
// Patterns query keys by name via get() / get_int() / get_hex_color().
// Unknown keys are ignored: each pattern handles forward compatibility,
// not the parser.
class Params
{
public:
	explicit Params(const char* csv);

	bool ok() const noexcept {
		return ok_;
	}

	std::optional<std::string_view> get(std::string_view key) const noexcept;
	std::optional<int> get_int(std::string_view key) const noexcept;
	std::optional<RGB16> get_hex_color(std::string_view key) const noexcept;

private:
	std::vector<std::pair<std::string, std::string> > kv_;
	bool ok_{ true };
};

namespace detail
{

inline char ascii_tolower(char c) noexcept
{
	return (c >= 'A' && c <= 'Z') ? char(c + ('a' - 'A')) : c;
}

inline std::string_view trim(std::string_view s) noexcept
{
	while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
		s.remove_prefix(1);
	while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
		s.remove_suffix(1);
	return s;
}

inline bool ieq(std::string_view a, std::string_view b) noexcept
{
	if (a.size() != b.size())
		return false;
	for (size_t i = 0; i < a.size(); ++i)
		if (ascii_tolower(a[i]) != ascii_tolower(b[i]))
			return false;
	return true;
}

} // namespace detail


inline Params::Params(const char* csv)
{
	if (!csv || *csv == '\0')
		return;

	std::string_view s(csv);
	while (!s.empty()) {
		const size_t comma = s.find(',');
		std::string_view item = (comma == std::string_view::npos)
		                                ? s : s.substr(0, comma);
		s = (comma == std::string_view::npos)
		            ? std::string_view{} : s.substr(comma + 1);

		item = detail::trim(item);
		if (item.empty()) {
			ok_ = false;
			return;
		}

		const size_t eq = item.find('=');
		if (eq == std::string_view::npos) {
			ok_ = false;
			return;
		}
		const std::string_view k = detail::trim(item.substr(0, eq));
		const std::string_view v = detail::trim(item.substr(eq + 1));
		if (k.empty()) {
			ok_ = false;
			return;
		}
		kv_.emplace_back(std::string(k), std::string(v));
	}
}

inline std::optional<std::string_view>
Params::get(std::string_view key) const noexcept
{
	for (const auto& [k, v] : kv_)
		if (detail::ieq(k, key))
			return std::string_view(v);
	return std::nullopt;
}

inline std::optional<int>
Params::get_int(std::string_view key) const noexcept
{
	auto v = get(key);
	if (!v || v->empty())
		return std::nullopt;
	int sign = 1;
	size_t i = 0;
	if ((*v)[0] == '-') { sign = -1; ++i; }
	else if ((*v)[0] == '+') { ++i; }
	if (i == v->size())
		return std::nullopt;
	int out = 0;
	for (; i < v->size(); ++i) {
		const char c = (*v)[i];
		if (c < '0' || c > '9')
			return std::nullopt;
		out = out * 10 + (c - '0');
	}
	return sign * out;
}

// Parses a hex color string. The optional `0x`/`0X` prefix is allowed.
// The number of hex digits after the prefix selects the layout:
//   6 digits  — 8-bit  RRGGBB              (alpha defaults to opaque)
//   8 digits  — 8-bit  AARRGGBB            (alpha-first)
//  12 digits  — 16-bit RRRRGGGGBBBB        (alpha defaults to opaque)
//  16 digits  — 16-bit AAAARRRRGGGGBBBB    (alpha-first)
// 8-bit components are byte-replicated to the normalized 16-bit form
// (0xFF → 0xFFFF); 16-bit components are stored directly. Any other
// length, malformed digits, or stray separators yield std::nullopt.
inline std::optional<RGB16>
Params::get_hex_color(std::string_view key) const noexcept
{
	auto v = get(key);
	if (!v)
		return std::nullopt;

	std::string_view s = *v;
	if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
		s.remove_prefix(2);

	const auto digit = [](char c) -> int {
				   if (c >= '0' && c <= '9') return c - '0';
				   if (c >= 'a' && c <= 'f') return c - 'a' + 10;
				   if (c >= 'A' && c <= 'F') return c - 'A' + 10;
				   return -1;
			   };
	const auto parse_n = [&](size_t off, size_t n) -> std::optional<unsigned> {
				     unsigned out = 0;
				     for (size_t i = 0; i < n; ++i) {
					     const int d = digit(s[off + i]);
					     if (d < 0)
						     return std::nullopt;
					     out = (out << 4) | unsigned(d);
				     }
				     return out;
			     };

	bool has_alpha;
	bool is_16bpc;
	switch (s.size()) {
	case 6:  has_alpha = false; is_16bpc = false; break;
	case 8:  has_alpha = true;  is_16bpc = false; break;
	case 12: has_alpha = false; is_16bpc = true;  break;
	case 16: has_alpha = true;  is_16bpc = true;  break;
	default: return std::nullopt;
	}

	const size_t per = is_16bpc ? 4 : 2;
	const unsigned full = is_16bpc ? 0xFFFFu : 0xFFu;
	unsigned a = full, r, g, b;
	size_t off = 0;
	if (has_alpha) {
		auto av = parse_n(off, per);
		if (!av) return std::nullopt;
		a = *av;
		off += per;
	}
	auto rv = parse_n(off, per);
	if (!rv) return std::nullopt;
	r = *rv;
	off += per;
	auto gv = parse_n(off, per);
	if (!gv) return std::nullopt;
	g = *gv;
	off += per;
	auto bv = parse_n(off, per);
	if (!bv) return std::nullopt;
	b = *bv;

	if (is_16bpc) {
		return RGB16{ uint16_t(r), uint16_t(g), uint16_t(b), uint16_t(a) };
	} else {
		const auto rep = [](unsigned x) noexcept {
					 return uint16_t((x << 8) | x);
				 };
		return RGB16{ rep(r), rep(g), rep(b), rep(a) };
	}
}

} // namespace pixpat
