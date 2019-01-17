#include <stdio.h>
#include <unistd.h>
#include <algorithm>

#include <kms++/kms++.h>
#include <kms++util/kms++util.h>

using namespace std;
using namespace kms;

static const char* usage_str =
		"Usage: kmsblank [OPTION]...\n\n"
		"Blank screen(s)\n\n"
		"Options:\n"
		"      --device=DEVICE       DEVICE is the path to DRM card to open\n"
		"  -c, --connector=CONN      CONN is <connector>\n"
		"  -t, --time=TIME           blank/unblank in TIME intervals\n"
		"\n"
		"<connector> can be given by index (<idx>) or id (@<id>).\n"
		"<connector> can also be given by name.\n"
		;

static void usage()
{
	puts(usage_str);
}

int main(int argc, char **argv)
{
	string dev_path;

	vector<string> conn_strs;
	uint32_t time = 0;

	OptionSet optionset = {
		Option("|device=", [&dev_path](string s)
		{
			dev_path = s;
		}),
		Option("c|connector=", [&conn_strs](string str)
		{
			conn_strs.push_back(str);
		}),
		Option("t|time=", [&time](string str)
		{
			time = stoul(str);
		}),
		Option("h|help", []()
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

	Card card(dev_path);
	ResourceManager resman(card);

	vector<Connector*> conns;

	if (conn_strs.size() > 0) {
		for (string s : conn_strs) {
			auto c = resman.reserve_connector(s);
			if (!c)
				EXIT("Failed to resolve connector '%s'", s.c_str());
			conns.push_back(c);
		}
	} else {
		conns = card.get_connectors();
	}

	bool blank = true;

	while (true) {
		for (Connector* conn : conns) {
			if (!conn->connected()) {
				printf("Connector %u not connected\n", conn->idx());
				continue;
			}

			printf("Connector %u: %sblank\n", conn->idx(), blank ? "" : "un");
			int r = conn->set_prop_value("DPMS", blank ? 3 : 0);
			if (r)
				EXIT("Failed to set DPMS: %d", r);
		}

		if (time == 0)
			break;

		usleep(1000 * time);

		blank = !blank;
	}

	printf("press enter to exit\n");

	getchar();
}
