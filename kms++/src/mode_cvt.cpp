// Supports CVT 1.2 reduced blanking modes v1 and v2

#include <kms++/kms++.h>
#include <cmath>

using namespace std;

namespace kms
{

static float CELL_GRAN = 8;
static float CELL_GRAN_RND = round(CELL_GRAN);

struct CVTConsts
{
	float CLOCK_STEP;
	float MIN_V_BPORCH;
	float RB_H_BLANK;
	float RB_H_FPORCH;
	float RB_H_SYNC;
	float RB_H_BPORCH;
	float RB_MIN_V_BLANK;
	float RB_V_FPORCH;
	float REFRESH_MULTIPLIER;
};

static const CVTConsts cvt_consts_v1 =
{
	.CLOCK_STEP = 0.25,	// Fixed
	.MIN_V_BPORCH = 6,	// Min
	.RB_H_BLANK = 160,	// Fixed
	.RB_H_FPORCH = 48,	// Fixed
	.RB_H_SYNC = 32,	// Fixed
	.RB_H_BPORCH = 80,	// Fixed
	.RB_MIN_V_BLANK = 460,	// Min
	.RB_V_FPORCH = 3,	// Fixed
	.REFRESH_MULTIPLIER = 1,// Fixed
};

static const CVTConsts cvt_consts_v2 =
{
	.CLOCK_STEP = 0.001,	// Fixed
	.MIN_V_BPORCH = 6,	// Fixed
	.RB_H_BLANK = 80,	// Fixed
	.RB_H_FPORCH = 8,	// Fixed
	.RB_H_SYNC = 32,	// Fixed
	.RB_H_BPORCH = 40,	// Fixed
	.RB_MIN_V_BLANK = 460,	// Min
	.RB_V_FPORCH = 1,	// Min
	.REFRESH_MULTIPLIER = 1,// or 1000/1001
};

Videomode videomode_from_cvt(uint32_t hact, uint32_t vact, uint32_t refresh, bool ilace, bool reduced_v2, bool video_optimized)
{
	CVTConsts c = reduced_v2 ? cvt_consts_v2 : cvt_consts_v1;

	if (video_optimized)
		c.REFRESH_MULTIPLIER = 1000.0/1001.0;

	bool INT_RQD = ilace;

	float H_PIXELS = hact;
	float V_LINES = vact;
	float IP_FREQ_RQD = refresh ? refresh : 60;
	if (ilace)
		IP_FREQ_RQD /= 2;

	float V_SYNC_RND;

	if (reduced_v2) {
		V_SYNC_RND = 8;
	} else {
		if (hact * 3 == vact * 4)
			V_SYNC_RND = 4;
		else if (hact * 9 == vact * 16)
			V_SYNC_RND = 5;
		else if (hact * 10 == vact * 16)
			V_SYNC_RND = 6;
		else if (hact == 1280 && (vact == 1024 || vact == 768))
			V_SYNC_RND = 7;
		else
			V_SYNC_RND = 10;
	}

	// 5.2.1
	float V_FIELD_RATE_RQD = INT_RQD ? IP_FREQ_RQD * 2 : IP_FREQ_RQD;

	// 5.2.2
	float H_PIXELS_RND = floor(H_PIXELS / CELL_GRAN_RND) * CELL_GRAN_RND;

	// 5.2.3
	float LEFT_MARGIN = 0;
	float RIGHT_MARGIN = 0;

	// 5.2.4
	float TOTAL_ACTIVE_PIXELS = H_PIXELS_RND + LEFT_MARGIN + RIGHT_MARGIN;

	// 5.2.5
	float V_LINES_RND = INT_RQD ? floor(V_LINES / 2) : floor(V_LINES);

	// 5.2.6
	float TOP_MARGIN = 0;
	float BOT_MARGIN = 0;

	// 5.2.7
	float INTERLACE = INT_RQD ? 0.5 : 0;

	// 5.4.8
	float H_PERIOD_EST = ((1000000 / V_FIELD_RATE_RQD) - c.RB_MIN_V_BLANK) / (V_LINES_RND + TOP_MARGIN + BOT_MARGIN);

	// 5.4.9
	float VBI_LINES = floor(c.RB_MIN_V_BLANK / H_PERIOD_EST) + 1;

	// 5.4.10
	float RB_MIN_VBI = c.RB_V_FPORCH + V_SYNC_RND + c.MIN_V_BPORCH;
	float ACT_VBI_LINES = VBI_LINES < RB_MIN_VBI ? RB_MIN_VBI : VBI_LINES;

	// 5.4.11
	float TOTAL_V_LINES = ACT_VBI_LINES + V_LINES_RND + TOP_MARGIN + BOT_MARGIN + INTERLACE;

	// 5.4.12
	float TOTAL_PIXELS = c.RB_H_BLANK + TOTAL_ACTIVE_PIXELS;

	// 5.4.13
	float ACT_PIXEL_FREQ = c.CLOCK_STEP * floor((V_FIELD_RATE_RQD * TOTAL_V_LINES * TOTAL_PIXELS / 1000000 * c.REFRESH_MULTIPLIER) / c.CLOCK_STEP);

	// 5.4.14
	//float ACT_H_FREQ = 1000 * ACT_PIXEL_FREQ / TOTAL_PIXELS;

	// 5.4.15
	//float ACT_FIELD_RATE = 1000 * ACT_H_FREQ / TOTAL_V_LINES;

	// 5.4.16
	//float ACT_FRAME_RATE = INT_RQD ? ACT_FIELD_RATE / 2 : ACT_FIELD_RATE;

	// 3.4.3.7 Adjust vfp
	if (reduced_v2)
		c.RB_V_FPORCH = ACT_VBI_LINES - V_SYNC_RND - c.MIN_V_BPORCH;

	Videomode mode;

	mode = videomode_from_timings(ACT_PIXEL_FREQ * 1000,
				      H_PIXELS_RND, c.RB_H_FPORCH, c.RB_H_SYNC, c.RB_H_BPORCH,
				      V_LINES_RND * (INT_RQD ? 2 : 1), c.RB_V_FPORCH, V_SYNC_RND, ACT_VBI_LINES - V_SYNC_RND - c.RB_V_FPORCH);

	mode.set_hsync(SyncPolarity::Positive);
	mode.set_vsync(SyncPolarity::Negative);

	mode.set_interlace(INT_RQD);

	return mode;
}

}
