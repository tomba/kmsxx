#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <algorithm>
#include <chrono>

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
	"  -i, --interval=MS[,MS]    toggle between blank and unblank every MS\n"
	"                            milliseconds. With two values, the first is\n"
	"                            the time in the initial state and the second\n"
	"                            the time in the other state.\n"
	"  -n, --count=N             with -i, stop after N cycles (default: run\n"
	"                            until killed)\n"
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

static void sleep_ms(uint32_t ms)
{
	struct timespec ts = {
		.tv_sec = ms / 1000,
		.tv_nsec = (long)(ms % 1000) * 1000000,
	};

	nanosleep(&ts, nullptr);
}

static void set_dpms(const vector<Connector*>& conns, bool blank)
{
	ConnectorDPMS dpms = blank ? ConnectorDPMS::Off : ConnectorDPMS::On;

	for (Connector* conn : conns) {
		auto start = chrono::steady_clock::now();
		int r = conn->set_prop_value("DPMS", (uint64_t)dpms);
		auto end = chrono::steady_clock::now();
		if (r)
			EXIT("Failed to set DPMS: %d", r);

		chrono::duration<double, milli> ms = end - start;
		printf("Connector %s: %sblank (%.1f ms)\n", conn->fullname().c_str(),
		       blank ? "" : "un", ms.count());
	}
}

int main(int argc, char** argv)
{
	string dev_path;

	vector<string> conn_strs;
	uint32_t interval[2] = { 0, 0 };
	uint32_t count = 0;
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
		Option("i|interval=", [&interval](string str) {
			size_t comma = str.find(',');
			interval[0] = stoul(str.substr(0, comma));
			if (comma == string::npos)
				interval[1] = interval[0];
			else
				interval[1] = stoul(str.substr(comma + 1));
			if (interval[0] == 0 || interval[1] == 0)
				EXIT("Interval must be positive");
		}),
		Option("n|count=", [&count](string str) {
			count = stoul(str);
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

	if (wait && interval[0])
		EXIT("-w and -i cannot be used together");

	if (count && !interval[0])
		EXIT("-n requires -i");

	Card card(dev_path);

	if (!card.is_master())
		EXIT("Could not get DRM master permission. Card already in use?");

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

	if (interval[0]) {
		for (uint32_t i = 0; count == 0 || i < count; i++) {
			set_dpms(conns, blank);
			sleep_ms(interval[0]);

			set_dpms(conns, !blank);
			if (count == 0 || i + 1 < count)
				sleep_ms(interval[1]);
		}
	} else {
		set_dpms(conns, blank);
	}

	if (wait) {
		printf("press enter to exit\n");
		getchar();
	}
}
