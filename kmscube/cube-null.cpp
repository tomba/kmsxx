#include <poll.h>

#include "cube-egl.h"
#include "cube-gles2.h"
#include "cube.h"

#include <kms++util/kms++util.h>

using namespace std;

void main_null()
{
	EglState egl(EGL_DEFAULT_DISPLAY);
	EglSurface surface(egl, 0);
	GlScene scene;

	scene.set_viewport(600, 600);

	int framenum = 0;

	struct pollfd fds[1] = { };
	fds[0].fd = 0;
	fds[0].events =  POLLIN;

	while (true) {
		int r = poll(fds, ARRAY_SIZE(fds), 0);
		ASSERT(r >= 0);

		if (fds[0].revents)
			break;

		surface.make_current();
		scene.draw(framenum++);
		surface.swap_buffers();
	}
}
