#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <kms++/kms++.h>
#include <kms++/omap/omapkms++.h>

namespace py = pybind11;

using namespace kms;
using namespace std;

void init_pykmsomap(py::module &m)
{
	py::class_<OmapCard>(m, "OmapCard", py::base<Card>())
			.def(py::init<>())
			;

	py::class_<OmapFramebuffer>(m, "OmapFramebuffer", py::base<MappedFramebuffer>())
			.def(py::init<OmapCard&, uint32_t, uint32_t, const string&>(),
			     py::keep_alive<1, 2>())	// Keep Card alive until this is destructed
			.def(py::init<OmapCard&, uint32_t, uint32_t, PixelFormat>(),
			     py::keep_alive<1, 2>())	// Keep OmapCard alive until this is destructed
			.def_property_readonly("format", &OmapFramebuffer::format)
			.def_property_readonly("num_planes", &OmapFramebuffer::num_planes)
			.def("fd", &OmapFramebuffer::prime_fd)
			.def("stride", &OmapFramebuffer::stride)
			.def("offset", &OmapFramebuffer::offset)
			;
}
