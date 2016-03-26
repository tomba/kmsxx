#include <cstdio>
#include <algorithm>

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <poll.h>

#include "kms++.h"

#include "test.h"
#include "prodcons.h"

using namespace std;
using namespace kms;

class Server
{
public:
	Server()
	{
		int r;

		printf("starting server\n");

		unlink(SOCKNAME);

		int fd = socket(AF_UNIX, SOCK_STREAM, 0);
		ASSERT(fd >= 0);

		struct sockaddr_un addr = { 0 };
		addr.sun_family = AF_UNIX;
		strcpy(addr.sun_path, SOCKNAME);
		r = bind(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un));
		ASSERT(r == 0);

		r = listen(fd, 0);
		ASSERT(r == 0);

		printf("listening...\n");

		m_fd = fd;
	}

	~Server()
	{
		close(m_fd);
		unlink(SOCKNAME);
	}

	int fd() const { return m_fd; }

private:
	int m_fd;
};

static void receive_msg(Card& card, int cfd, MsgType msg_type)
{
	switch (msg_type) {
	case MsgType::SetCrtc:
	{
		int fd;
		size_t len;

		MsgSetCrtc crtcmsg;
		len = sock_fd_read(cfd, &crtcmsg, sizeof(crtcmsg), &fd);
		ASSERT(len == sizeof(crtcmsg));
		printf("recv set-crtc, fd %d\n", fd);

		auto fb = receive_fb(card, cfd);

		auto conn = card.get_connector(crtcmsg.connector);
		auto crtc = card.get_crtc(crtcmsg.crtc);
		auto mode = crtcmsg.mode;

		int r = crtc->set_mode(conn, *fb, mode);
		ASSERT(r == 0);

		break;
	}

	}
}

int main()
{
	Card card;
	Server server;

	struct pollfd fds[4] = { 0 };

	fds[0].fd = 0;
	fds[0].events = POLLIN;

	fds[1].fd = card.fd();
	fds[1].events = POLLIN;

	fds[2].fd = server.fd();
	fds[2].events = POLLIN;

	printf("press enter to exit\n");

	int clientfd = -1;

	while (true) {
		for (unsigned i = 0; i < ARRAY_SIZE(fds); ++i)
			fds[i].revents = 0;

		int n_fds = clientfd >= 0 ? 4 : 3;

		int r = poll(fds, n_fds, -1);
		ASSERT(r > 0);

		if (fds[0].revents) {
			fprintf(stderr, "exit due to user-input\n");
			break;
		}

		if (fds[1].revents) {
			card.call_page_flip_handlers();
		}

		if (fds[2].revents) {
			printf("server socket event\n");

			int new_fd = accept(server.fd(), NULL, NULL);
			ASSERT(new_fd > 0);
			if (clientfd != -1) {
				close(new_fd);
				printf("client connect attempt, but a client already connected\n");
			} else {
				clientfd = new_fd;
				printf("client connected\n");
			}
		}

		if (fds[3].revents) {
			printf("client socket event\n");

			MsgType id;
			size_t len = sock_fd_read(clientfd, &id, 1, nullptr);

			printf("recv %zd, id %d\n", len, (int)id);

			if (len == 0) {
				printf("client disconnect\n");
				close(clientfd);
				clientfd = -1;
			} else {
				receive_msg(card, clientfd, id);
			}
		}
	}

	if (clientfd >= 0)
		close(clientfd);

	printf("exit\n");
}
