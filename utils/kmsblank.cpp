#include <stdio.h>
#include <unistd.h>
#include <algorithm>

#include <kms++/kms++.h>
#include <kms++util/kms++util.h>

using namespace std;
using namespace kms;

static const char* usage_str =
	"Usage: kmsblank [OPTION]...\n\n"
	"Blank or unblank screen(s)\n\n"
	"Options:\n"
	"      --device=DEVICE       DEVICE is the path to DRM card to open\n"
	"  -C, --card=NUM            open /dev/dri/card<NUM>\n"
	"  -c, --connector=CONN      CONN is <connector>\n"
	"  -u, --unblank             unblank instead of blank\n"
	"  -w, --wait                wait for enter before exiting\n"
	"  -t, --time=TIME           blank/unblank in TIME intervals\n"
	"\n"
	"<connector> can be given by index (<idx>) or id (@<id>).\n"
	"<connector> can also be given by name.\n"
	"\n"
	"The DPMS state persists after exit, unless the kernel's fbdev\n"
	"emulation restores its own mode when the device is closed.\n";

static void usage()
{
	puts(usage_str);
}

static void set_dpms(const vector<Connector*>& conns, bool blank)
{
	ConnectorDPMS dpms = blank ? ConnectorDPMS::Off : ConnectorDPMS::On;

	for (Connector* conn : conns) {
		printf("Connector %s: %sblank\n", conn->fullname().c_str(),
		       blank ? "" : "un");
		int r = conn->set_prop_value("DPMS", (uint64_t)dpms);
		if (r)
			EXIT("Failed to set DPMS: %d", r);
	}
}

int main(int argc, char** argv)
{
	string dev_path;

	vector<string> conn_strs;
	uint32_t time = 0;
	bool unblank = false;
	bool wait = false;

	OptionSet optionset = {
		Option("|device=", [&dev_path](string s) {
			dev_path = s;
		}),
		Option("C|card=", [&dev_path](string s) {
			dev_path = "/dev/dri/card" + s;
		}),
		Option("c|connector=", [&conn_strs](string str) {
			conn_strs.push_back(str);
		}),
		Option("u|unblank", [&unblank]() {
			unblank = true;
		}),
		Option("w|wait", [&wait]() {
			wait = true;
		}),
		Option("t|time=", [&time](string str) {
			time = stoul(str);
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

	// DPMS on a connector without a CRTC has no visible effect
	erase_if(conns, [](Connector* conn) {
		if (!conn->connected()) {
			printf("Connector %s not connected\n", conn->fullname().c_str());
			return true;
		}

		if (!conn->get_current_crtc()) {
			printf("Connector %s not enabled\n", conn->fullname().c_str());
			return true;
		}

		return false;
	});

	if (conns.empty())
		EXIT("No enabled connectors");

	bool blank = !unblank;

	while (true) {
		set_dpms(conns, blank);

		if (time == 0)
			break;

		usleep(1000 * time);

		blank = !blank;
	}

	if (wait) {
		printf("press enter to exit\n");
		getchar();
	}
}
