#pragma once

#include <cstdio>
#include <format>
#include <string>

// This can be removed when moving to C++23. For now, this gives us
// fmt::format() and fmt::print().

namespace fmt {

using std::format;

template<typename... Args>
void print(std::format_string<Args...> fmtstr, Args&&... args)
{
	std::string s = std::format(fmtstr, std::forward<Args>(args)...);
	std::fwrite(s.data(), 1, s.size(), stdout);
}

template<typename... Args>
void print(std::FILE* f, std::format_string<Args...> fmtstr, Args&&... args)
{
	std::string s = std::format(fmtstr, std::forward<Args>(args)...);
	std::fwrite(s.data(), 1, s.size(), f);
}

} // namespace fmt
