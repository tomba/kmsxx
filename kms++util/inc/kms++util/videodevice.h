#pragma once

#include <string>
#include <memory>
#include <kms++/kms++.h>

class VideoStreamer;
class MetaStreamer;

enum class VideoMemoryType
{
	MMAP,
	DMABUF,
};

class VideoBuffer
{
public:
	VideoMemoryType m_mem_type;
	uint32_t m_index;
	uint32_t m_length;
	int m_fd;
	uint32_t m_offset;
	kms::PixelFormat m_format;
};

class VideoDevice
{
public:
	struct VideoFrameSize {
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
	MetaStreamer* get_meta_capture_streamer();

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

	bool m_has_capture = false;
	bool m_has_mplane_capture = false;

	bool m_has_output = false;
	bool m_has_mplane_output = false;

	bool m_has_m2m = false;
	bool m_has_mplane_m2m = false;

	bool m_has_meta_capture = false;

	std::unique_ptr<VideoStreamer> m_capture_streamer;
	std::unique_ptr<VideoStreamer> m_output_streamer;
	std::unique_ptr<MetaStreamer> m_meta_capture_streamer;
};

class VideoStreamer
{
public:
	enum class StreamerType {
		CaptureSingle,
		CaptureMulti,
		OutputSingle,
		OutputMulti,
		CaptureMeta,
		OutputMeta,
		};

	VideoStreamer(int fd, StreamerType type);
	virtual ~VideoStreamer() { }

	std::vector<std::string> get_ports();
	void set_port(uint32_t index);

	std::vector<kms::PixelFormat> get_formats();
	int get_format(kms::PixelFormat& fmt, uint32_t& width, uint32_t& height);
	void set_format(kms::PixelFormat fmt, uint32_t width, uint32_t height);
	void get_selection(uint32_t& left, uint32_t& top, uint32_t& width, uint32_t& height);
	void set_selection(uint32_t& left, uint32_t& top, uint32_t& width, uint32_t& height);
	void set_queue_size(uint32_t queue_size, VideoMemoryType mem_type);
	void queue(VideoBuffer& fb);
	VideoBuffer dequeue();
	void stream_on();
	void stream_off();

	int fd() const { return m_fd; }

protected:
	int m_fd;
	StreamerType m_type;
	VideoMemoryType m_mem_type;
	std::vector<bool> m_fbs;
};


class MetaStreamer : public VideoStreamer
{
public:
	MetaStreamer(int fd, VideoStreamer::StreamerType type);

	void set_format(kms::PixelFormat fmt, uint32_t size);
};
