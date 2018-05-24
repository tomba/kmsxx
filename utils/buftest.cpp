#include <cstdio>
#include <fstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sys/mman.h>

extern "C" {
#include <libdrm/etnaviv_drmif.h>
};

#define FAIL_IF(x, fmt, ...) \
	if (x) { \
	fprintf(stderr, "%s:%d: %s:\n" fmt "\n", __FILE__, __LINE__, __PRETTY_FUNCTION__, ##__VA_ARGS__); \
	abort(); \
	}

int main(int argc, char** argv)
{
	int r;

	int disp_fd = open("/dev/dri/card1", O_RDWR | O_CLOEXEC);
	FAIL_IF(disp_fd < 0, "failed to open card0");

	int etna_fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
	FAIL_IF(etna_fd < 0, "failed to open card1");
	struct etna_device* etna = etna_device_new(etna_fd);

	struct drm_mode_create_dumb creq = drm_mode_create_dumb();
	creq.width = 640;
	creq.height = 480;
	creq.bpp = 32;
	r = drmIoctl(disp_fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq);
	FAIL_IF(r, "DRM_IOCTL_MODE_CREATE_DUMB failed");

	uint32_t bo_handle = creq.handle;

	int bo_fd;
	r = drmPrimeHandleToFD(disp_fd, bo_handle, DRM_CLOEXEC | O_RDWR, &bo_fd);
	FAIL_IF(r, "drmPrimeHandleToFD failed");

	struct etna_bo* etna_bo = etna_bo_from_dmabuf(etna, bo_fd);

	void* etna_p = etna_bo_map(etna_bo);
	FAIL_IF(etna_p == MAP_FAILED, "etna_bo_map failed");
}
