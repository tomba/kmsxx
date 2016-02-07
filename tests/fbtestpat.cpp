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

#include "test.h"
#include "mappedbuffer.h"

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

	void* ptr = mmap(NULL,
			 var.yres_virtual * fix.line_length,
			 PROT_WRITE | PROT_READ,
			 MAP_SHARED, fd, 0);

	FAIL_IF(ptr == MAP_FAILED, "mmap failed");

	MappedCPUBuffer buf(var.xres_virtual, var.yres_virtual, PixelFormat::XRGB8888);

	printf("%s: res %dx%d, virtual %dx%d, line_len %d\n",
	       fbdev,
	       var.xres, var.yres,
	       var.xres_virtual, var.yres_virtual,
	       fix.line_length);

	draw_test_pattern(buf);

	memcpy(ptr, buf.map(0), buf.size(0));

	close(fd);

	return 0;
}
