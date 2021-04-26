#pragma once

#include <string>
#include <memory>
#include <kms++/kms++.h>
#include <kms++util/mediabus.h>

struct SubdevRoute
{
	uint32_t sink_pad;
	uint32_t sink_stream;
	uint32_t source_pad;
	uint32_t source_stream;
	bool active;
	bool immutable;
};

enum class ConfigurationType {
	Active,
	Try,
};

class VideoSubdev
{
public:
	VideoSubdev(const std::string& dev_path);

	~VideoSubdev();

	void set_format(uint32_t pad, uint32_t stream, uint32_t width, uint32_t height, BusFormat fmt, ConfigurationType type = ConfigurationType::Active);
	int get_format(uint32_t pad, uint32_t stream, uint32_t& width, uint32_t& height, BusFormat& fmt, ConfigurationType type = ConfigurationType::Active);

	std::vector<SubdevRoute> get_routes(ConfigurationType type = ConfigurationType::Active);
	int set_routes(const std::vector<SubdevRoute>& routes, ConfigurationType type = ConfigurationType::Active);
private:
	int m_fd;
	std::string m_name;
};
