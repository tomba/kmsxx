#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <linux/fb.h>

#include <kms++util.h>
#include <extcpuframebuffer.h>

using namespace kms;

int main(int argc, char** argv)
{
	const char* fbdev = "/dev/fb0";
	int r;

	int fd = open(fbdev, O_RDWR);
	FAIL_IF(fd < 0, "open %s failed\n", fbdev);

	struct fb_var_screeninfo var;

	r = ioctl(fd, FBIOGET_VSCREENINFO, &var);
	FAIL_IF(r, "FBIOGET_VSCREENINFO failed");

	struct fb_fix_screeninfo fix;

	r = ioctl(fd, FBIOGET_FSCREENINFO, &fix);
	FAIL_IF(r, "FBIOGET_FSCREENINFO failed");

	uint8_t* ptr = (uint8_t*)mmap(NULL,
			 var.yres_virtual * fix.line_length,
			 PROT_WRITE | PROT_READ,
			 MAP_SHARED, fd, 0);

	FAIL_IF(ptr == MAP_FAILED, "mmap failed");

	ExtCPUFramebuffer buf(var.xres, var.yres_virtual, PixelFormat::XRGB8888, ptr, fix.line_length);

	printf("%s: res %dx%d, virtual %dx%d, line_len %d\n",
	       fbdev,
	       var.xres, var.yres,
	       var.xres_virtual, var.yres_virtual,
	       fix.line_length);

	draw_test_pattern(buf);

	for (unsigned y = 0; y < var.yres_virtual; ++y)
		memcpy(ptr + fix.line_length * y, buf.map(0) + buf.stride(0) * y, buf.stride(0));

	close(fd);

	return 0;
}
