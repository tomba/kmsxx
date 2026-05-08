#pragma once

// Aggregator: every Source / Sink template lives in one of the
// per-iteration-shape headers under io/. Encode/decode helpers and
// load_word/store_word are in io/detail.h, used by all the others.

#include "io/detail.h"
#include "io/packed.h"
#include "io/semiplanar.h"
#include "io/planar.h"
#include "io/packed_yuv.h"
#include "io/gray.h"
#include "io/bayer.h"
