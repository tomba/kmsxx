#pragma once

namespace kms
{
class DumbFramebuffer;

void draw_color_bar(kms::DumbFramebuffer& buf, int old_xpos, int xpos, int width);
void draw_test_pattern(DumbFramebuffer& fb);
}
