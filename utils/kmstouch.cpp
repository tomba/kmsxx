#include <cstdio>
#include <unistd.h>
#include <algorithm>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstring>

#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>

#include <kms++/kms++.h>
#include <kms++util/kms++util.h>

using namespace std;
using namespace kms;

static const char* usage_str =
	"Usage: kmstouch [OPTION]...\n\n"
	"Simple touchscreen tester\n\n"
	"Options:\n"
	"      --input=DEVICE        DEVICE is the path to input device to open\n"
	"      --device=DEVICE       DEVICE is the path to DRM card to open\n"
	"  -c, --connector=CONN      CONN is <connector>\n"
	"\n";

static void usage()
{
	puts(usage_str);
}

static bool s_print_ev = false;

static vector<pair<int32_t, int32_t>> s_coords;

// axis -> min,max
static map<int, pair<int32_t, int32_t>> s_abs_map;
// axis -> value
static map<int, int32_t> s_abs_vals;

static void print_abs_bits(struct libevdev* dev, int axis)
{
	const struct input_absinfo* abs;

	if (!libevdev_has_event_code(dev, EV_ABS, axis))
		return;

	abs = libevdev_get_abs_info(dev, axis);

	printf("	Value	%6d\n", abs->value);
	printf("	Min	%6d\n", abs->minimum);
	printf("	Max	%6d\n", abs->maximum);
	if (abs->fuzz)
		printf("	Fuzz	%6d\n", abs->fuzz);
	if (abs->flat)
		printf("	Flat	%6d\n", abs->flat);
	if (abs->resolution)
		printf("	Resolution	%6d\n", abs->resolution);
}

static void print_code_bits(struct libevdev* dev, unsigned int type, unsigned int max)
{
	for (uint32_t i = 0; i <= max; i++) {
		if (!libevdev_has_event_code(dev, type, i))
			continue;

		printf("    Event code %i (%s)\n", i, libevdev_event_code_get_name(type, i));
		if (type == EV_ABS)
			print_abs_bits(dev, i);
	}
}

static void print_bits(struct libevdev* dev)
{
	printf("Supported events:\n");

	for (uint32_t i = 0; i <= EV_MAX; i++) {
		if (!libevdev_has_event_type(dev, i))
			continue;

		printf("  Event type %d (%s)\n", i, libevdev_event_type_get_name(i));

		switch (i) {
		case EV_KEY:
			print_code_bits(dev, EV_KEY, KEY_MAX);
			break;
		case EV_REL:
			print_code_bits(dev, EV_REL, REL_MAX);
			break;
		case EV_ABS:
			print_code_bits(dev, EV_ABS, ABS_MAX);
			break;
		case EV_LED:
			print_code_bits(dev, EV_LED, LED_MAX);
			break;
		}
	}
}

static void collect_current(struct libevdev* dev)
{
	for (uint32_t i = 0; i <= ABS_MAX; i++) {
		if (!libevdev_has_event_code(dev, EV_ABS, i))
			continue;

		const struct input_absinfo* abs;

		abs = libevdev_get_abs_info(dev, i);

		s_abs_vals[i] = abs->value;
		s_abs_map[i] = make_pair(abs->minimum, abs->maximum);
	}
}

static void print_props(struct libevdev* dev)
{
	printf("Properties:\n");

	for (uint32_t i = 0; i <= INPUT_PROP_MAX; i++) {
		if (!libevdev_has_property(dev, i))
			continue;

		printf("  Property type %d (%s)\n", i, libevdev_property_get_name(i));
	}
}

static void handle_event(struct input_event& ev, DumbFramebuffer* fb)
{
	static vector<pair<uint16_t, int32_t>> s_event_vec;

	if (s_print_ev)
		printf("%-6s %20s %6d\n",
		       libevdev_event_type_get_name(ev.type),
		       libevdev_event_code_get_name(ev.type, ev.code),
		       ev.value);

	switch (ev.type) {
	case EV_ABS:
		s_event_vec.emplace_back(ev.code, ev.value);
		break;

	case EV_KEY:
		s_event_vec.emplace_back(ev.code, ev.value);
		break;

	case EV_SYN:
		switch (ev.code) {
		case SYN_REPORT: {
			int32_t min_x = s_abs_map[ABS_X].first;
			int32_t max_x = s_abs_map[ABS_X].second;

			int32_t min_y = s_abs_map[ABS_Y].first;
			int32_t max_y = s_abs_map[ABS_Y].second;

			for (const auto& p : s_event_vec) {
				switch (p.first) {
				case ABS_X:
				case ABS_Y:
					s_abs_vals[p.first] = p.second;
					break;
				default:
					break;
				}
			}

			int32_t abs_x = s_abs_vals[ABS_X];
			int32_t abs_y = s_abs_vals[ABS_Y];

			int32_t x = (abs_x - min_x) * (fb->width() - 1) / (max_x - min_x);
			int32_t y = (abs_y - min_y) * (fb->height() - 1) / (max_y - min_y);

			printf("%d, %d -> %d, %d\n", abs_x, abs_y, x, y);

			draw_rgb_pixel(*fb, x, y, RGB(255, 255, 255));

			s_event_vec.clear();

			if (s_print_ev)
				printf("----\n");
			break;
		}

		default:
			EXIT("Unhandled syn event code %u\n", ev.code);
			break;
		}

		break;

	default:
		EXIT("Unhandled event type %u\n", ev.type);
		break;
	}
}

int main(int argc, char** argv)
{
	string drm_dev_path = "/dev/dri/card0";
	string input_dev_path = "/dev/input/event0";
	string conn_name;

	OptionSet optionset = {
		Option("i|input=", [&input_dev_path](string s) {
			input_dev_path = s;
		}),
		Option("|device=", [&drm_dev_path](string s) {
			drm_dev_path = s;
		}),
		Option("c|connector=", [&conn_name](string s) {
			conn_name = s;
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

	struct libevdev* dev = nullptr;

	int fd = open(input_dev_path.c_str(), O_RDONLY | O_NONBLOCK);
	FAIL_IF(fd < 0, "Failed to open input device %s: %s\n", input_dev_path.c_str(), strerror(errno));
	int rc = libevdev_new_from_fd(fd, &dev);
	FAIL_IF(rc < 0, "Failed to init libevdev (%s)\n", strerror(-rc));

	printf("Input device name: \"%s\"\n", libevdev_get_name(dev));
	printf("Input device ID: bus %#x vendor %#x product %#x\n",
	       libevdev_get_id_bustype(dev),
	       libevdev_get_id_vendor(dev),
	       libevdev_get_id_product(dev));

	if (!libevdev_has_event_type(dev, EV_ABS) ||
	    !libevdev_has_event_code(dev, EV_KEY, BTN_TOUCH)) {
		printf("This device does not look like a mouse\n");
		exit(1);
	}

	print_bits(dev);
	print_props(dev);

	collect_current(dev);

	Card card(drm_dev_path);
	ResourceManager resman(card);

	auto pixfmt = PixelFormat::XRGB8888;

	Connector* conn = resman.reserve_connector(conn_name);
	Crtc* crtc = resman.reserve_crtc(conn);
	Plane* plane = resman.reserve_overlay_plane(crtc, pixfmt);

	Videomode mode = conn->get_default_mode();

	uint32_t w = mode.hdisplay;
	uint32_t h = mode.vdisplay;

	auto fb = new DumbFramebuffer(card, w, h, pixfmt);

	AtomicReq req(card);

	req.add(plane, "CRTC_ID", crtc->id());
	req.add(plane, "FB_ID", fb->id());

	req.add(plane, "CRTC_X", 0);
	req.add(plane, "CRTC_Y", 0);
	req.add(plane, "CRTC_W", w);
	req.add(plane, "CRTC_H", h);

	req.add(plane, "SRC_X", 0);
	req.add(plane, "SRC_Y", 0);
	req.add(plane, "SRC_W", w << 16);
	req.add(plane, "SRC_H", h << 16);

	int r = req.commit_sync();
	FAIL_IF(r, "initial plane setup failed");

	do {
		struct input_event ev {
		};
		rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
		if (rc == 0)
			handle_event(ev, fb);

	} while (rc == 1 || rc == 0 || rc == -EAGAIN);

	delete fb;
}
