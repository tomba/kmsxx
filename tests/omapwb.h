/*
 * File: include/linux/omapwb.h
 *
 * Writeback driver for TI OMAP boards
 *
 * Copyright (C) 2015 Garmin Ltd. or its subsidiaries
 * Author: Cliff Hofman <cliff.hofman@garmin.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef _UAPI__LINUX_OMAPWB_H__
#define _UAPI__LINUX_OMAPWB_H__

#include <linux/ioctl.h>
#include <linux/types.h>

/* IOCTL commands. */

#define OMAP_IOWR(num, dtype)	_IOWR('O', num, dtype)

#define OMAP_WB_CONVERT		OMAP_IOWR(0, struct omap_wb_convert_info)

enum omap_wb_mode {
	/* mem2mem from single ovl to wb */
	OMAP_WB_MEM2MEM_OVL = 1,
	/* mem2mem from N overlays via single mgr to wb */
	OMAP_WB_MEM2MEM_MGR = 2,
	/* capture from single mgr to wb */
	OMAP_WB_CAPTURE_MGR = 3,
};

/* the same as enum omap_plane */
enum omap_wb_src_plane {
	OMAP_WB_GFX	= 0,
	OMAP_WB_VIDEO1	= 1,
	OMAP_WB_VIDEO2	= 2,
	OMAP_WB_VIDEO3	= 3,
};

/* the same as enum omap_channel */
enum omap_wb_src_channel {
	OMAP_WB_CHANNEL_LCD	= 0,
	OMAP_WB_CHANNEL_DIGIT	= 1,
	OMAP_WB_CHANNEL_LCD2	= 2,
	OMAP_WB_CHANNEL_LCD3	= 3,
};

struct omap_wb_plane {
	int fd;
	uint16_t pitch;
};

struct omap_wb_buffer {
	uint32_t pipe; /* enum omap_plane */
	uint32_t fourcc;
	uint16_t x, y;
	uint16_t width, height;
	uint16_t out_width, out_height;

	uint8_t num_planes;
	struct omap_wb_plane plane[2];
};

struct omap_wb_convert_info {
	uint32_t wb_mode; /* enum omap_wb_src_plane */

	uint32_t wb_channel; /* enum omap_wb_src_channel */
	uint32_t channel_width;
	uint32_t channel_height;

	uint32_t num_srcs;
	struct omap_wb_buffer src[3];
	struct omap_wb_buffer dst;
};

#endif /* _UAPI__LINUX_OMAPWB_H__ */
