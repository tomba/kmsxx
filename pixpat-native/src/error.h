#pragma once

#include <stdexcept>

namespace pixpat
{

struct error : std::runtime_error {
	using std::runtime_error::runtime_error;
};

struct invalid_argument : error {
	using error::error;
};

} // namespace pixpat
