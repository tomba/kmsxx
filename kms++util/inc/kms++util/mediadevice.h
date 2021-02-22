#pragma once

#include <string>
#include <memory>
#include <kms++/kms++.h>

#include <linux/media.h>
#include <linux/v4l2-mediabus.h> // XXX
#include <linux/v4l2-subdev.h>

struct MediaDevicePriv;

class MediaEntity;
class MediaLink;
class MediaPad;

class VideoSubdev;
class VideoDevice;

class MediaDevice
{
public:
	MediaDevice(const std::string& dev);
	MediaDevice(int fd);
	~MediaDevice();

	MediaDevice(const MediaDevice& other) = delete;
	MediaDevice& operator=(const MediaDevice& other) = delete;

	int fd() const { return m_fd; }

	void print(const std::string& name = "") const;

	MediaEntity* find_entity(const std::string &name) const;
	std::vector<MediaEntity*> collect_entities(const std::string &name) const;
private:
	int m_fd;

	MediaDevicePriv* m_priv;
};

class MediaObject
{
public:
	MediaObject(MediaDevice* media_device)
		: m_media_device(media_device)
	{ }

	virtual ~MediaObject() { }

	virtual uint32_t id() const = 0;
	virtual std::string name() const = 0;

	MediaDevice* m_media_device;
};

class MediaEntity : public MediaObject
{
public:
	MediaEntity(MediaDevice* media_device)
		: MediaObject(media_device)
	{ }

	bool is_subdev() const;

	std::string dev_path;

	media_v2_entity info;
	media_entity_desc desc;

	std::vector<std::shared_ptr<MediaPad>> pads;
	std::shared_ptr<MediaLink> link;

	uint32_t id() const { return info.id; }
	std::string name() const { return info.name; };

	std::unique_ptr<VideoSubdev> subdev;
	std::unique_ptr<VideoDevice> dev;

	std::vector<MediaEntity*> get_linked_entities(uint32_t pad_idx);
	std::vector<MediaLink *> get_links(uint32_t pad_idx);
};

class MediaPad : public MediaObject
{
public:
	MediaPad(MediaDevice* media_device)
		: MediaObject(media_device)
	{ }

	struct media_v2_pad info;
	struct media_pad_desc desc;

	std::shared_ptr<MediaEntity> entity;
	std::vector<std::shared_ptr<MediaLink>> links;

	uint32_t id() const { return info.id; }
	std::string name() const { return std::string("Pad") + std::to_string(info.id); };
};

class MediaLink : public MediaObject
{
public:
	MediaLink(MediaDevice* media_device)
		: MediaObject(media_device)
	{ }

	struct media_v2_link info;
	struct media_link_desc desc;

	std::shared_ptr<MediaObject> source;
	std::shared_ptr<MediaObject> sink;

	uint32_t id() const { return info.id; }
	std::string name() const { return std::string("Link") + std::to_string(info.id); };
};

class MediaInterface : public MediaObject
{
public:
	MediaInterface(MediaDevice* media_device)
		: MediaObject(media_device)
	{ }

	struct media_v2_interface info;

	std::shared_ptr<MediaLink> link;

	uint32_t id() const { return info.id; }
	std::string name() const { return std::string("Iface") + std::to_string(info.id); };
};

struct MediaDevicePriv {
	~MediaDevicePriv()
	{
	}

	MediaDevice* media_device;

	media_device_info info;

	std::vector<std::shared_ptr<MediaObject>> objects;
};
