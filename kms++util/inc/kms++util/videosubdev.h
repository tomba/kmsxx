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

class VideoSubdev
{
public:
	VideoSubdev(const std::string& dev_path);

	~VideoSubdev();

	void set_format(uint32_t pad, uint32_t width, uint32_t height, BusFormat fmt);
	void get_format(uint32_t pad, uint32_t& width, uint32_t& height, BusFormat& fmt);

	std::vector<SubdevRoute> get_routing();
private:
	int m_fd;
	std::string m_name;
};
