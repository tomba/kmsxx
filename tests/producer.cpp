#include <cstdio>
#include <algorithm>
#include <vector>

#include <poll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "kms++.h"

#include "test.h"
#include "prodcons.h"

using namespace std;
using namespace kms;

const unsigned NUM_FBS = 5;

class Client
{
public:
	Client()
	{
		printf("connecting\n");

		int fd = socket(AF_UNIX, SOCK_STREAM, 0);
		ASSERT(fd != 0);

		struct sockaddr_un addr = { 0 };
		addr.sun_family = AF_UNIX;
		strcpy(addr.sun_path, SOCKNAME);

		int r = connect(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un));
		ASSERT(r == 0);

		printf("connected, fd %d\n", fd);

		m_fd = fd;
	}

	~Client()
	{
		close(m_fd);
	}

	int fd() const { return m_fd; }

private:
	int m_fd;
};

static void main_loop(Card& card, Client& client, const vector<Framebuffer*>& fbs)
{
	vector<Framebuffer*> available_fbs(fbs);
	vector<Framebuffer*> sent_fbs;

	struct pollfd fds[2] = { 0 };

	fds[0].fd = 0;
	fds[0].events = POLLIN;

	fds[1].fd = client.fd();
	fds[1].events = POLLIN;

	printf("press enter to exit\n");

	while (true) {
		for (unsigned i = 0; i < ARRAY_SIZE(fds); ++i)
			fds[i].revents = 0;

		int r = poll(fds, ARRAY_SIZE(fds), -1);
		ASSERT(r > 0);

		if (fds[0].revents) {
			fprintf(stderr, "exit due to user-input\n");
			break;
		}

		if (fds[1].revents) {
			printf("socket event\n");

		}
	}
}

int main()
{
	Card card;
	card.drop_master();

	auto conn = card.get_first_connected_connector();
	if (!conn) {
		printf("No connected connectors\n");
		return -1;
	}

	auto crtc = conn->get_current_crtc();
	if (!crtc) {
		printf("No crtc\n");
		return -1;
	}

	Client client;

	vector<Framebuffer*> fbs;

	for (unsigned i = 0; i < NUM_FBS; ++i) {
		auto fb = new DumbFramebuffer(card, crtc->width(), crtc->height(), PixelFormat::XRGB8888);
		draw_test_pattern(*fb);
		fbs.push_back(fb);
	}

	main_loop(card, client, fbs);

	for(auto fb : fbs)
		delete fb;
}
