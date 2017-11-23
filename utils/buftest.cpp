#include <cstdio>
#include <fstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>

#include <kms++/kms++.h>
#include <kms++util/kms++util.h>

using namespace std;
using namespace kms;

/*
 * struct dma_buf_test_rw_data - metadata passed to the kernel to read handle
 * @ptr:       a pointer to an area at least as large as size
 * @offset:    offset into the dma_buf buffer to start reading
 * @size:      size to read or write
 * @write:     1 to write, 0 to read
 */
struct dma_buf_test_rw_data {
	uint64_t ptr;
	uint64_t offset;
	uint64_t size;
	int write;
	int __padding;
};

#define DMA_BUF_IOC_MAGIC              'I'
#define DMA_BUF_IOC_TEST_SET_FD \
	_IO(DMA_BUF_IOC_MAGIC, 0xf0)
#define DMA_BUF_IOC_TEST_DMA_MAPPING \
	_IOW(DMA_BUF_IOC_MAGIC, 0xf1, struct dma_buf_test_rw_data)
#define DMA_BUF_IOC_TEST_KERNEL_MAPPING \
	_IOW(DMA_BUF_IOC_MAGIC, 0xf2, struct dma_buf_test_rw_data)

int main(int argc, char** argv)
{
	Card card("/dev/dri/card0");
	ResourceManager res(card);

	auto pixfmt = PixelFormat::XRGB8888;

	auto conn = res.reserve_connector();
	auto crtc = res.reserve_crtc(conn);
	auto plane = res.reserve_overlay_plane(crtc, pixfmt);
	FAIL_IF(!plane, "available plane not found");
	auto mode = conn->get_default_mode();

	uint32_t w = mode.hdisplay;
	uint32_t h = mode.vdisplay;

	w = 800/4;
	h = 480/4;

	auto fb = new DumbFramebuffer(card, w, h, pixfmt);

	//draw_test_pattern(*fb);

	int r;
#if 0
	AtomicReq req(card);

	unique_ptr<Blob> mode_blob = mode.to_blob(card);

	req.add(conn, {
			{ "CRTC_ID", crtc->id() },
		});

	req.add(crtc, {
			{ "ACTIVE", 1 },
			{ "MODE_ID", mode_blob->id() },
		});

	req.add(plane, {
			{ "FB_ID", fb->id() },
			{ "CRTC_ID", crtc->id() },
			{ "SRC_X", 0 << 16 },
			{ "SRC_Y", 0 << 16 },
			{ "SRC_W", fb->width() << 16 },
			{ "SRC_H", fb->height() << 16 },
			{ "CRTC_X", 0 },
			{ "CRTC_Y", 0 },
			{ "CRTC_W", fb->width() },
			{ "CRTC_H", fb->height() },
		});

	r = req.test(true);
	if (r)
		EXIT("Atomic test failed: %d\n", r);

	r = req.commit_sync(true);
	if (r)
		EXIT("Atomic commit failed: %d\n", r);
#endif

	int testfd = open("/dev/dma_buf-test", O_RDWR);
	if (testfd < 0)
		EXIT("open failed\n");

	r = ioctl(testfd, DMA_BUF_IOC_TEST_SET_FD, fb->prime_fd(0));
	if (r)
		EXIT("ioctl failed: %s\n", strerror(errno));

	//printf("press enter to test\n");
	//getchar();

	uint8_t* buf = (uint8_t*)malloc(fb->size(0));
	for (uint32_t i = 0; i < fb->size(0); ++i)
		buf[i] = 0xff;

	struct dma_buf_test_rw_data data;
	data.ptr = (uint64_t)buf;
	data.offset = 0;
	data.size = w * h * 4;
	data.write = 0;

	r = ioctl(testfd, DMA_BUF_IOC_TEST_DMA_MAPPING, &data);
	if (r)
		EXIT("DMA_BUF_IOC_TEST_DMA_MAPPING failed: %s\n", strerror(errno));


	printf("press enter to exit\n");
	getchar();

	delete fb;
}
