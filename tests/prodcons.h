#pragma once

#define SOCKNAME "/tmp/mysock"

extern "C"
{
ssize_t sock_fd_write(int sock, void *buf, ssize_t buflen, int fd);
ssize_t sock_fd_read(int sock, void *buf, ssize_t bufsize, int *fd);
}

kms::Framebuffer* receive_fb(kms::Card& card, int cfd);
void send_fb(kms::Card& card, int cfd, kms::DumbFramebuffer& fb);

enum class MsgType : uint8_t
{
	SetCrtc,
};

struct MsgFramebuffer
{
	uint32_t width;
	uint32_t height;
	kms::PixelFormat format;

	unsigned num_planes;
};

struct MsgPlane
{
	uint32_t pitch;
	uint32_t offset;
};


struct MsgSetCrtc
{
	uint32_t connector;
	uint32_t crtc;

	kms::Videomode mode;
};
