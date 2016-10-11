#pragma once

#include <kms++/videomode.h>

namespace kms
{

Videomode videomode_from_cvt(uint32_t hact, uint32_t vact, uint32_t refresh, bool ilace, bool reduced_v2, bool video_optimized);

}
