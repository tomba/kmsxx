#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "kms++.h"
#include "test.h"
#include "prodcons.h"

using namespace kms;

Framebuffer* receive_fb(Card& card, int cfd)
{
	size_t len;

	MsgFramebuffer fbmsg;
	len = sock_fd_read(cfd, &fbmsg, sizeof(fbmsg), 0);
	ASSERT(len == sizeof(fbmsg));

	printf("recv fb\n");

	int fds[4];
	uint32_t pitches[4];
	uint32_t offsets[4];

	for (unsigned i = 0; i < fbmsg.num_planes; ++i) {
		MsgPlane planemsg;
		int fd;
		len = sock_fd_read(cfd, &planemsg, sizeof(planemsg), &fd);
		ASSERT(len == sizeof(planemsg));

		printf("recv plane, fd %d\n", fd);

		fds[i] = fd;
		pitches[i] = planemsg.pitch;
		offsets[i] = planemsg.offset;
	}

	return new ExtFramebuffer(card, fbmsg.width, fbmsg.height, fbmsg.format,
				     fds, pitches, offsets);
}

void send_fb(Card& card, int cfd, DumbFramebuffer& fb)
{
	MsgFramebuffer fbmsg;
	fbmsg.width = fb.width();
	fbmsg.height = fb.height();
	fbmsg.format = fb.format();
	fbmsg.num_planes = fb.num_planes();

	size_t len;

	len = sock_fd_write(cfd, &fbmsg, sizeof(fbmsg), -1);
	ASSERT(len == sizeof(fbmsg));

	printf("sent fb\n");

	for (unsigned i = 0; i < fbmsg.num_planes; ++i) {
		MsgPlane planemsg;
		planemsg.pitch = fb.stride(i);
		planemsg.offset = fb.offset(i);

		int fd = fb.prime_fd(i);

		len = sock_fd_write(cfd, &planemsg, sizeof(planemsg), fd);
		ASSERT(len == sizeof(planemsg));

		printf("sent plane handle %x, prime %d\n", fb.handle(i), fd);

		int r = close(fd);
		ASSERT(r == 0);
	}
}

/* http://keithp.com/blogs/fd-passing/ */
ssize_t sock_fd_write(int sock, void *buf, ssize_t buflen, int fd)
{
	struct msghdr   msg;
	struct iovec	iov;
	union {
		struct cmsghdr  cmsghdr;
		char		control[CMSG_SPACE(sizeof (int))];
	} cmsgu;
	struct cmsghdr  *cmsg;

	iov.iov_base = buf;
	iov.iov_len = buflen;

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	if (fd != -1) {
		msg.msg_control = cmsgu.control;
		msg.msg_controllen = sizeof(cmsgu.control);

		cmsg = CMSG_FIRSTHDR(&msg);
		cmsg->cmsg_len = CMSG_LEN(sizeof (int));
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;

		int *pfd = (int *) CMSG_DATA(cmsg);
		*pfd = fd;
	} else {
		msg.msg_control = NULL;
		msg.msg_controllen = 0;
	}

	return sendmsg(sock, &msg, 0);
}

/* http://keithp.com/blogs/fd-passing/ */
ssize_t sock_fd_read(int sock, void *buf, ssize_t bufsize, int *fd)
{
	ssize_t	 size;

	if (fd) {
		struct msghdr   msg;
		struct iovec	iov;
		union {
			struct cmsghdr  cmsghdr;
			char		control[CMSG_SPACE(sizeof (int))];
		} cmsgu;
		struct cmsghdr  *cmsg;

		iov.iov_base = buf;
		iov.iov_len = bufsize;

		msg.msg_name = NULL;
		msg.msg_namelen = 0;
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		msg.msg_control = cmsgu.control;
		msg.msg_controllen = sizeof(cmsgu.control);
		size = recvmsg (sock, &msg, 0);
		if (size < 0) {
			perror ("recvmsg");
			exit(1);
		}
		cmsg = CMSG_FIRSTHDR(&msg);
		if (cmsg && cmsg->cmsg_len == CMSG_LEN(sizeof(int))) {
			if (cmsg->cmsg_level != SOL_SOCKET) {
				fprintf (stderr, "invalid cmsg_level %d\n",
					 cmsg->cmsg_level);
				exit(1);
			}
			if (cmsg->cmsg_type != SCM_RIGHTS) {
				fprintf (stderr, "invalid cmsg_type %d\n",
					 cmsg->cmsg_type);
				exit(1);
			}

			int *pfd = (int *) CMSG_DATA(cmsg);
			*fd = *pfd;
		} else
			*fd = -1;
	} else {
		size = read (sock, buf, bufsize);
		if (size < 0) {
			perror("read");
			exit(1);
		}
	}
	return size;
}
