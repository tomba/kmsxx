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
	if (m_fd < 0)
		throw runtime_error(fmt::format("Failed to open subdev {}", dev_path));
}

VideoSubdev::~VideoSubdev()
{
	::close(m_fd);
}

enum v4l2_subdev_format_whence configTypeToWhence(ConfigurationType type)
{
	return type == ConfigurationType::Active ? V4L2_SUBDEV_FORMAT_ACTIVE : V4L2_SUBDEV_FORMAT_TRY;
}

void VideoSubdev::set_format(uint32_t pad, uint32_t stream, uint32_t width, uint32_t height, BusFormat busfmt, ConfigurationType type)
{
	struct v4l2_subdev_format fmt {};
	int r;

	uint32_t cod = BusFormatToCode(busfmt);

	fmt.pad = pad;
	fmt.stream = stream;
	fmt.which = configTypeToWhence(type);

	r = ioctl(m_fd, VIDIOC_SUBDEV_G_FMT, &fmt);
	FAIL_IF(r, "VIDIOC_SUBDEV_G_FMT failed: %d: %s", r, strerror(errno));

	//fmt::print("GET FMT {}x{}, {:#x}\n", fmt.format.width, fmt.format.height, fmt.format.code);

	fmt.format.width = width;
	fmt.format.height = height;
	fmt.format.code = cod;
	fmt.format.field = V4L2_FIELD_NONE;

	r = ioctl(m_fd, VIDIOC_SUBDEV_S_FMT, &fmt);
	FAIL_IF(r, "VIDIOC_SUBDEV_S_FMT failed: %d: %s", r, strerror(errno));

	//fmt::print("SET FMT {}x{}, {:#x}\n", fmt.format.width, fmt.format.height, fmt.format.code);

	fmt.pad = pad;
	fmt.stream = stream;
	fmt.which = configTypeToWhence(type);

	r = ioctl(m_fd, VIDIOC_SUBDEV_G_FMT, &fmt);
	FAIL_IF(r, "VIDIOC_SUBDEV_G_FMT failed: %d: %s", r, strerror(errno));

	if (width != fmt.format.width || height != fmt.format.height || cod != fmt.format.code) {
		fmt::print("{}: size & fmt setup failed: {}/{}/{}  != {}/{}/{:#x}\n", m_name,
			   width, height, BusFormatToString(busfmt), fmt.format.width, fmt.format.height, fmt.format.code);
	}
}

int VideoSubdev::get_format(uint32_t pad, uint32_t stream, uint32_t& width, uint32_t& height, BusFormat &busfmt, ConfigurationType type)
{
	struct v4l2_subdev_format fmt {};
	int r;

	fmt.pad = pad;
	fmt.stream = stream;
	fmt.which = configTypeToWhence(type);

	r = ioctl(m_fd, VIDIOC_SUBDEV_G_FMT, &fmt);
	if (r)
		return r;

	width = fmt.format.width;
	height = fmt.format.height;
	busfmt = CodeToBusFormat(fmt.format.code);

	return 0;
}

vector<SubdevRoute> VideoSubdev::get_routes(ConfigurationType type)
{
	struct v4l2_subdev_route vroutes[20] { };
	struct v4l2_subdev_routing vrouting { };

	vrouting.num_routes = ARRAY_SIZE(vroutes);
	vrouting.routes = (uint64_t)vroutes;
	vrouting.which = configTypeToWhence(type);

	//fmt::print("GET ROUTING for {}\n", m_name);
	int r;

	r = ioctl(m_fd, VIDIOC_SUBDEV_G_ROUTING, &vrouting);
	if (r) {
		//fmt::print("GET ROUTING failed for {}: {} {}\n", m_name, r, strerror(errno));
		return {};
	}

	vector<SubdevRoute> v;

// XXX v4l header is missing BIT() definition
#define BIT(n) (1U << n)

	for (uint32_t i = 0; i < vrouting.num_routes; ++i) {
		const auto& r = vroutes[i];

		v.push_back(SubdevRoute { r.sink_pad, r.sink_stream, r.source_pad, r.source_stream,
					 !!(r.flags & V4L2_SUBDEV_ROUTE_FL_ACTIVE), !!(r.flags & V4L2_SUBDEV_ROUTE_FL_IMMUTABLE), !!(r.flags & V4L2_SUBDEV_ROUTE_FL_SOURCE) });
	}

	return v;
}

int VideoSubdev::set_routes(const std::vector<SubdevRoute>& routes, ConfigurationType type)
{
	vector<v4l2_subdev_route> vroutes;

	for (const auto& r : routes) {
		v4l2_subdev_route vroute{ r.sink_pad, r.sink_stream, r.source_pad, r.source_stream,
					  (r.active ? V4L2_SUBDEV_ROUTE_FL_ACTIVE : 0) |
						  (r.immutable ? V4L2_SUBDEV_ROUTE_FL_IMMUTABLE : 0) |
						  (r.source ? V4L2_SUBDEV_ROUTE_FL_SOURCE : 0) };
		vroutes.push_back(vroute);
	}

	struct v4l2_subdev_routing vrouting { };

	vrouting.num_routes = vroutes.size();
	vrouting.routes = (uint64_t)vroutes.data();
	vrouting.which = configTypeToWhence(type);

	int r;

	r = ioctl(m_fd, VIDIOC_SUBDEV_S_ROUTING, &vrouting);
	if (r) {
		fmt::print("SET ROUTING failed for {}: {} {}\n", m_name, r, strerror(errno));
		return {};
	}

	return 0;
}
