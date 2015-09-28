#pragma once

#include "decls.h"

namespace kms
{
struct Pipeline {
	Crtc* crtc;
	Connector* connector;
};
}
