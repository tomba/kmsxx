#pragma once

#include <cstdint>
#include "videomode.h"

namespace kms
{
struct Videomode;

extern const Videomode dmt_modes[];
extern const Videomode cea_modes[];

const Videomode& find_dmt(uint32_t width, uint32_t height, float vrefresh, bool ilace);
const Videomode& find_cea(uint32_t width, uint32_t height, float vrefresh, bool ilace);

}
