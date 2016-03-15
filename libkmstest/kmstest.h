#pragma once

#include "color.h"
#include "framebuffer.h"

namespace kms
{
class IMappedFramebuffer;

void draw_color_bar(IMappedFramebuffer& buf, int old_xpos, int xpos, int width);

void draw_test_pattern(IMappedFramebuffer &fb);

void draw_rect(IMappedFramebuffer &fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h, RGB color);
}
