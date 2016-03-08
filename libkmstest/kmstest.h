#pragma once

#include "color.h"

namespace kms
{
class MappedBuffer;
class DumbFramebuffer;

void draw_color_bar(kms::DumbFramebuffer& buf, int old_xpos, int xpos, int width);

void draw_test_pattern(MappedBuffer& fb);
void draw_test_pattern(DumbFramebuffer &fb);

void draw_rect(MappedBuffer &fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h, RGB color);
void draw_rect(DumbFramebuffer &fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h, RGB color);
}
