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

	py::class_<OmapFramebuffer, Framebuffer> omapfb(m, "OmapFramebuffer");

	// XXX we should use py::arithmetic() here to support or and and operators, but it's not supported in the pybind11 we use
	py::enum_<OmapFramebuffer::Flags>(omapfb, "Flags")
			.value("None", OmapFramebuffer::Flags::None)
			.value("Tiled", OmapFramebuffer::Flags::Tiled)
			.value("MemContig", OmapFramebuffer::Flags::MemContig)
			.value("MemTiler", OmapFramebuffer::Flags::MemTiler)
			.value("MemPin", OmapFramebuffer::Flags::MemPin)
			.export_values()
			;

	omapfb
			.def(py::init<OmapCard&, uint32_t, uint32_t, const string&, OmapFramebuffer::Flags>(),
			     py::keep_alive<1, 2>(),	// Keep Card alive until this is destructed
			     py::arg("card"), py::arg("width"), py::arg("height"), py::arg("fourcc"), py::arg("flags") = OmapFramebuffer::None)
			.def(py::init<OmapCard&, uint32_t, uint32_t, PixelFormat, OmapFramebuffer::Flags>(),
			     py::keep_alive<1, 2>(),	// Keep OmapCard alive until this is destructed
			     py::arg("card"), py::arg("width"), py::arg("height"), py::arg("pixfmt"), py::arg("flags") = OmapFramebuffer::None)
			.def_property_readonly("format", &OmapFramebuffer::format)
			.def_property_readonly("num_planes", &OmapFramebuffer::num_planes)
			.def("fd", &OmapFramebuffer::prime_fd)
			.def("stride", &OmapFramebuffer::stride)
			.def("offset", &OmapFramebuffer::offset)
			;
}
