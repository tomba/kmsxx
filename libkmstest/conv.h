#pragma once

namespace kms
{
class DumbFramebuffer;

void color_convert(DumbFramebuffer& dst, const DumbFramebuffer &src);
}
