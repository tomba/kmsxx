#include <v4l2++/helpers.h>

void __my_throw(const char* file, int line, const char *funcname, const char *cond, fmt::string_view format, fmt::format_args args)
{
	std::string str = fmt::vformat(format, args);

	fmt::print(stderr, "{}:{}: {}:\n{}", file, line, funcname, str);
	if (cond)
		fmt::print(stderr, " ({})\n", cond);
	else
		fmt::print("\n");

	fflush(stderr);

	__throw_exception_again std::runtime_error(str);
}
