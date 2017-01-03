#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <kms++/kms++.h>
#include <kms++/omap/omapkms++.h>

namespace py = pybind11;

using namespace kms;
using namespace std;

void init_pykmsomap(py::module &m)
{
	py::class_<OmapCard, Card>(m, "OmapCard")
			.def(py::init<>())
			;

	py::class_<OmapFramebuffer, MappedFramebuffer>(m, "OmapFramebuffer")
			.def(py::init<OmapCard&, uint32_t, uint32_t, const string&>(),
			     py::keep_alive<1, 2>())	// Keep Card alive until this is destructed
			.def(py::init<OmapCard&, uint32_t, uint32_t, PixelFormat>(),
			     py::keep_alive<1, 2>())	// Keep OmapCard alive until this is destructed
			;
}
