#include <kms++util/strhelpers.h>

#include <algorithm>
#include <stdarg.h>

using namespace std;

string to_lower(const string& str)
{
	string data = str;
	transform(data.begin(), data.end(), data.begin(), ::tolower);
	return data;
}

string sformat(const char *fmt, ...)
{
	static char s_format_buf[1024];

	va_list args;
	va_start(args, fmt);

	vsnprintf(s_format_buf, sizeof(s_format_buf), fmt, args);

	va_end(args);

	return string(s_format_buf);
}
