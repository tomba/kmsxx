#pragma once

// Aggregator: every named layout the X-macro registers lives in one of
// the headers under formats/, organized by color kind. Format names
// follow the kms++/pixutils convention (see formats/rgb.h for the
// longer note; the YUYV group is an exception, see formats/yuv_packed.h).

#include "formats/rgb.h"
#include "formats/yuv_semiplanar.h"
#include "formats/yuv_planar.h"
#include "formats/yuv_packed.h"
#include "formats/grayscale.h"
#include "formats/bayer.h"
