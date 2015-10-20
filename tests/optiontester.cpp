#include <cstdio>
#include <algorithm>
#include <iostream>

#include "cmdoptions.h"

using namespace std;

static map<string, CmdOption> options = {
	{ "test", NO_PARAM("test") },
	{ "test2", HAS_PARAM("test2") },
};

int main(int argc, char **argv)
{
	CmdOptions opts(argc, argv, options);

	if (opts.error().length()) {
		cerr << opts.error() << opts.usage();
		return -1;
	}

	for (auto p : options)
		printf("Option %s set %d param %s\n", 
		       p.first.c_str(), opts.is_set(p.first),
		       opts.opt_param(p.first).c_str());;

	return 0;
}
