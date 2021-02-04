#include <fmt/format.h>

#include <string>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <system_error>
#include <sys/sysmacros.h>
#include <linux/media.h>
#include <linux/v4l2-subdev.h>

#include <kms++/kms++.h>
#include <kms++util/kms++util.h>
#include <kms++util/videosubdev.h>

using namespace std;
using namespace kms;

VideoSubdev::VideoSubdev(const string &dev_path)
{
	m_name = dev_path;
	m_fd = ::open(dev_path.c_str(), O_RDWR);

	//get_routing();
}

VideoSubdev::~VideoSubdev()
{
	::close(m_fd);
}

void VideoSubdev::set_format(uint32_t pad, uint32_t width, uint32_t height, BusFormat busfmt)
{
	struct v4l2_subdev_format fmt {};
	int r;

	uint32_t cod = BusFormatToCode(busfmt);

	fmt.pad = pad;
	fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;

	r = ioctl(m_fd, VIDIOC_SUBDEV_G_FMT, &fmt);
	FAIL_IF(r, "VIDIOC_SUBDEV_G_FMT failed: %d: %s", r, strerror(errno));

	//fmt::print("GET FMT {}x{}, {:#x}\n", fmt.format.width, fmt.format.height, fmt.format.code);

	fmt.format.width = width;
	fmt.format.height = height;
	fmt.format.code = cod;

	r = ioctl(m_fd, VIDIOC_SUBDEV_S_FMT, &fmt);
	FAIL_IF(r, "VIDIOC_SUBDEV_S_FMT failed: %d: %s", r, strerror(errno));

	//fmt::print("SET FMT {}x{}, {:#x}\n", fmt.format.width, fmt.format.height, fmt.format.code);

	fmt.pad = pad;
	fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;

	r = ioctl(m_fd, VIDIOC_SUBDEV_G_FMT, &fmt);
	FAIL_IF(r, "VIDIOC_SUBDEV_G_FMT failed: %d: %s", r, strerror(errno));

	if (width != fmt.format.width || height != fmt.format.height || cod != fmt.format.code) {
		fmt::print("size & fmt setup failed: {}/{}/{}  != {}/{}/{:#x}\n",
			   width, height, BusFormatToString(busfmt), fmt.format.width, fmt.format.height, fmt.format.code);
	}
}

void VideoSubdev::get_format(uint32_t pad, uint32_t& width, uint32_t& height, BusFormat &busfmt)
{
	struct v4l2_subdev_format fmt {};
	int r;

	fmt.pad = pad;
	fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;

	r = ioctl(m_fd, VIDIOC_SUBDEV_G_FMT, &fmt);
	FAIL_IF(r, "VIDIOC_SUBDEV_G_FMT failed: %d: %s", r, strerror(errno));

	width = fmt.format.width;
	height = fmt.format.height;
	busfmt = CodeToBusFormat(fmt.format.code);
}

vector<SubdevRoute> VideoSubdev::get_routing()
{
	struct v4l2_subdev_routing routing { };
	struct v4l2_subdev_route routes[20];

	routing.num_routes = ARRAY_SIZE(routes);
	routing.routes = (uint64_t)routes;

	fmt::print("GET ROUTING for {}\n", m_name);
	int r;

	r = ioctl(m_fd, VIDIOC_SUBDEV_G_ROUTING, &routing);
	if (r) {
		fmt::print("GET ROUTING failed for {}: {} {}\n", m_name, r, strerror(errno));
		return {};
	}

	vector<SubdevRoute> v;

// XXX v4l header is missing BIT() definition
#define BIT(n) (1U << n)

	for (uint32_t i = 0; i < routing.num_routes; ++i) {
		const auto& r = routes[i];

		v.push_back(SubdevRoute { r.sink_pad, r.sink_stream, r.source_pad, r.source_stream,
					 !!(r.flags & V4L2_SUBDEV_ROUTE_FL_ACTIVE), !!(r.flags & V4L2_SUBDEV_ROUTE_FL_IMMUTABLE) });
	}

	return v;
}
