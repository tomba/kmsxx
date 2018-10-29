#pragma once

#include <string>
#include <kms++/kms++.h>

class VideoStreamer;

class VideoDevice
{
public:
	struct VideoFrameSize
	{
		uint32_t min_w, max_w, step_w;
		uint32_t min_h, max_h, step_h;
	};

	VideoDevice(const std::string& dev);
	VideoDevice(int fd);
	~VideoDevice();

	VideoDevice(const VideoDevice& other) = delete;
	VideoDevice& operator=(const VideoDevice& other) = delete;

	VideoStreamer* get_capture_streamer();
	VideoStreamer* get_output_streamer();

	std::vector<std::tuple<uint32_t, uint32_t>> get_discrete_frame_sizes(kms::PixelFormat fmt);
	VideoFrameSize get_frame_sizes(kms::PixelFormat fmt);

	int fd() const { return m_fd; }
	bool has_capture() const { return m_has_capture; }
	bool has_output() const { return m_has_output; }
	bool has_m2m() const { return m_has_m2m; }

	static std::vector<std::string> get_capture_devices();
	static std::vector<std::string> get_m2m_devices();

private:
	int m_fd;

	bool m_has_capture;
	bool m_has_mplane_capture;

	bool m_has_output;
	bool m_has_mplane_output;

	bool m_has_m2m;
	bool m_has_mplane_m2m;

	std::vector<kms::DumbFramebuffer*> m_capture_fbs;
	std::vector<kms::DumbFramebuffer*> m_output_fbs;

	VideoStreamer* m_capture_streamer;
	VideoStreamer* m_output_streamer;
};

class VideoStreamer
{
public:
	enum class StreamerType {
		CaptureSingle,
		CaptureMulti,
		OutputSingle,
		OutputMulti,
	};

	VideoStreamer(int fd, StreamerType type);

	std::vector<std::string> get_ports();
	void set_port(uint32_t index);

	std::vector<kms::PixelFormat> get_formats();
	void set_format(kms::PixelFormat fmt, uint32_t width, uint32_t height);
	void get_selection(uint32_t& left, uint32_t& top, uint32_t& width, uint32_t& height);
	void set_selection(uint32_t& left, uint32_t& top, uint32_t& width, uint32_t& height);
	void set_queue_size(uint32_t queue_size);
	void queue(kms::DumbFramebuffer* fb);
	kms::DumbFramebuffer* dequeue();
	void stream_on();
	void stream_off();

	int fd() const { return m_fd; }

private:
	int m_fd;
	StreamerType m_type;
	std::vector<kms::DumbFramebuffer*> m_fbs;
};
