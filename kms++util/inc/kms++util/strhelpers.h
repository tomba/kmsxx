#include <sstream>
#include <string>
#include <vector>
#include <functional>

std::string to_lower(const std::string& str);

template <typename T>
std::string join(const T& values, const std::string& delim)
{
	std::ostringstream ss;
	for (const auto& v : values) {
		if (&v != &values[0])
			ss << delim;
		ss << v;
	}
	return ss.str();
}

template <typename T>
std::string join(const std::vector<T>& values, const std::string& delim, std::function<std::string(T)> func)
{
	std::ostringstream ss;
	for (const auto& v : values) {
		if (&v != &values[0])
			ss << delim;
		ss << func(v);
	}
	return ss.str();
}

std::string sformat(const char *fmt, ...)
	__attribute__ ((format (printf, 1, 2)));
