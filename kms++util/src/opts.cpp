#include <algorithm>

#include <unistd.h>
#include <getopt.h>

#include <kms++util/opts.h>

using namespace std;

Option::Option(const string& str, function<void()> func)
	: m_void_func(func)
{
	parse(str);
}

Option::Option(const string& str, function<void(const string)> func)
	: m_func(func)
{
	parse(str);
}

void Option::parse(const string& str)
{
	auto iend = str.end();
	if (*(iend - 1) == '=') {
		iend--;
		m_has_arg = 1;
	} else if (*(iend - 1) == '?') {
		iend--;
		m_has_arg = 2;
	} else {
		m_has_arg = 0;
	}

	auto isplit = find(str.begin(), iend, '|');

	if (isplit != str.begin())
		m_short = str[0];
	else
		m_short = 0;

	if (isplit != iend)
		m_long = string(isplit + 1, iend);
}

OptionSet::OptionSet(initializer_list<Option> il)
	: m_opts(il)
{
}

void OptionSet::parse(int argc, char** argv)
{
	string shortopts = ":";
	vector<struct option> longopts;

	for (unsigned opt_idx = 0; opt_idx < m_opts.size(); ++opt_idx) {
		const Option& o = m_opts[opt_idx];

		if (o.m_short != 0) {
			shortopts.push_back(o.m_short);
			if (o.m_has_arg == 1)
				shortopts.push_back(':');
			else if (o.m_has_arg == 2)
				shortopts.append("::");
		}

		if (!o.m_long.empty()) {
			struct option copt;
			copt.name = o.m_long.c_str();
			copt.has_arg = o.m_has_arg;
			copt.flag = 0;
			copt.val = opt_idx + 1000;
			longopts.push_back(copt);
		}
	}

	longopts.push_back(option {});

	while (1) {
		int long_idx = 0;
		int c = getopt_long(argc, argv, shortopts.c_str(),
				    longopts.data(), &long_idx);
		if (c == -1)
			break;

		if (c == '?')
			throw std::invalid_argument(string("Unrecognized option ") + argv[optind - 1]);

		if (c == ':') {
			const Option& o = find_opt(optopt);
			if (optopt < 256)
				throw std::invalid_argument(string("Missing argument to -") + o.m_short);
			else
				throw std::invalid_argument(string("Missing argument to --") + o.m_long);
		}

		string sarg = { optarg ?: "" };

		const Option& opt = find_opt(c);

		if (opt.m_func)
			opt.m_func(sarg);
		else
			opt.m_void_func();
	}

	for (int i = optind; i < argc; ++i)
		m_params.push_back(argv[i]);
}

const Option& OptionSet::find_opt(int c)
{
	if (c < 256)
		return *find_if(m_opts.begin(), m_opts.end(), [c](const Option& o) { return o.m_short == c; });
	else
		return m_opts[c - 1000];
}
