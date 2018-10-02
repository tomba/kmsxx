#include <cstdio>
#include <fstream>
#include <unistd.h>
#include <string.h>

#include <kms++/kms++.h>
#include <kms++util/kms++util.h>

#ifdef HAS_LIBPNG
#include <libpng16/png.h>
#endif

using namespace std;
using namespace kms;

#ifdef HAS_LIBPNG

uint32_t width, height;
png_byte color_type;
png_byte bit_depth;

png_structp png_ptr;
png_infop info_ptr;
int number_of_passes;
png_bytep * row_pointers;

void read_png_file(const char* file_name, DumbFramebuffer* fb, Crtc* crtc, Plane* plane)
{
	uint8_t header[8];

	FILE *fp = fopen(file_name, "rb");
	if (!fp)
		EXIT("[read_png_file] File %s could not be opened for reading", file_name);

	size_t s = fread(header, 1, 8, fp);
	if (s != 8)
		EXIT("failed to read PNG header\n");
	if (png_sig_cmp(header, 0, 8))
		EXIT("[read_png_file] File %s is not recognized as a PNG file", file_name);

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr)
		EXIT("[read_png_file] png_create_read_struct failed");

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
		EXIT("[read_png_file] png_create_info_struct failed");

	if (setjmp(png_jmpbuf(png_ptr)))
		EXIT("[read_png_file] Error during init_io");

	png_init_io(png_ptr, fp);
	png_set_sig_bytes(png_ptr, 8);

	png_read_info(png_ptr, info_ptr);

	width = png_get_image_width(png_ptr, info_ptr);
	height = png_get_image_height(png_ptr, info_ptr);
	color_type = png_get_color_type(png_ptr, info_ptr);
	bit_depth = png_get_bit_depth(png_ptr, info_ptr);

	printf("%d,%d,%d,%d\n", width, height, color_type, bit_depth);

	number_of_passes = png_set_interlace_handling(png_ptr);
	png_read_update_info(png_ptr, info_ptr);

	/* read file */
	if (setjmp(png_jmpbuf(png_ptr)))
		EXIT("[read_png_file] Error during read_image");

	CPUFramebuffer cfb(width, height, PixelFormat::XRGB8888);
	uint8_t* cfb_p = cfb.map(0);

	row_pointers = (png_bytep*) malloc(sizeof(png_bytep) * height);
	for (uint32_t y = 0; y < height; y++)
		row_pointers[y] = cfb_p + y * cfb.stride(0);

	png_read_image(png_ptr, row_pointers);

	fclose(fp);

	return cfb;
/*

	unsigned w = min(crtc->width(), fb->width());
	unsigned h = min(crtc->height(), fb->height());

	int r = crtc->set_plane(plane, *fb,
				0, 0, w, h,
				0, 0, fb->width(), fb->height());

	ASSERT(r == 0);
}
*/
#endif

static void read_frame(ifstream& is, DumbFramebuffer* fb, Crtc* crtc, Plane* plane)
{
	for (unsigned i = 0; i < fb->num_planes(); ++i)
		is.read((char*)fb->map(i), fb->size(i));

	unsigned w = min(crtc->width(), fb->width());
	unsigned h = min(crtc->height(), fb->height());

	int r = crtc->set_plane(plane, *fb,
				0, 0, w, h,
				0, 0, fb->width(), fb->height());

	ASSERT(r == 0);
}

static const char* usage_str =
		"Usage: kmsview [options] <file> <width> <height> <fourcc>\n\n"
		"Options:\n"
		"  -c, --connector <name>	Output connector\n"
		"  -t, --time <ms>		Milliseconds to sleep between frames\n"
		;

static void usage()
{
	puts(usage_str);
}

int main(int argc, char** argv)
{
	uint32_t time = 0;
	string dev_path = "/dev/dri/card0";
	string conn_name;

	OptionSet optionset = {
		Option("c|connector=", [&conn_name](string s)
		{
			conn_name = s;
		}),
		Option("|device=", [&dev_path](string s)
		{
			dev_path = s;
		}),
		Option("t|time=", [&time](const string& str)
		{
			time = stoul(str);
		}),
		Option("h|help", []()
		{
			usage();
			exit(-1);
		}),
	};

	optionset.parse(argc, argv);

	vector<string> params = optionset.params();

	if (params.size() != 4) {
		usage();
		exit(-1);
	}

	string filename = params[0];
	uint32_t w = stoi(params[1]);
	uint32_t h = stoi(params[2]);
	string modestr = params[3];

	auto pixfmt = FourCCToPixelFormat(modestr);

	Card card(dev_path);
	ResourceManager res(card);

	auto conn = res.reserve_connector(conn_name);
	auto crtc = res.reserve_crtc(conn);
	auto plane = res.reserve_overlay_plane(crtc, pixfmt);
	FAIL_IF(!plane, "available plane not found");

	auto fb = new DumbFramebuffer(card, w, h, pixfmt);

	unsigned frame_size = 0;
	for (unsigned i = 0; i < fb->num_planes(); ++i)
		frame_size += fb->size(i);

	read_png_file("wayland-screenshot.png", fb, crtc, plane);

#if 0
	ifstream is(filename, ifstream::binary);

	is.seekg(0, std::ios::end);
	unsigned fsize = is.tellg();
	is.seekg(0);

	unsigned num_frames = fsize / frame_size;
	printf("file size %u, frame size %u, frames %u\n", fsize, frame_size, num_frames);

	for (unsigned i = 0; i < num_frames; ++i) {
		printf("frame %d", i); fflush(stdout);
		read_frame(is, fb, crtc, plane);
		if (!time) {
			getchar();
		} else {
			usleep(time * 1000);
			printf("\n");
		}
	}

	is.close();
#endif

	if (!time) {
		printf("press enter to exit\n");
		getchar();
	}

	delete fb;
}
