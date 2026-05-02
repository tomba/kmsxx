#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <kms++/kms++.h>
#include <kms++/omap/omapkms++.h>

namespace py = nanobind;

using namespace kms;
using namespace std;

void init_pykmsomap(py::module_& m)
{
	py::class_<OmapCard, Card>(m, "OmapCard", R"doc(
OMAP DRM/KMS card handle.
)doc")
		.def(py::init<>());

	py::class_<OmapFramebuffer, Framebuffer> omapfb(m, "OmapFramebuffer", R"doc(
Framebuffer backed by OMAP-specific buffer allocation.
)doc");

	// XXX we should support or and and operators for the flag enum.
	py::enum_<OmapFramebuffer::Flags>(omapfb, "Flags")
		.value("None", OmapFramebuffer::Flags::None)
		.value("Tiled", OmapFramebuffer::Flags::Tiled)
		.value("MemContig", OmapFramebuffer::Flags::MemContig)
		.value("MemTiler", OmapFramebuffer::Flags::MemTiler)
		.value("MemPin", OmapFramebuffer::Flags::MemPin)
		.export_values();

	omapfb
		.def(py::init<OmapCard&, uint32_t, uint32_t, const string&, OmapFramebuffer::Flags>(),
		     py::keep_alive<1, 2>(), // Keep Card alive until this is destructed
		     py::arg("card"), py::arg("width"), py::arg("height"), py::arg("fourcc"), py::arg("flags") = OmapFramebuffer::None)
		.def(py::init<OmapCard&, uint32_t, uint32_t, PixelFormat, OmapFramebuffer::Flags>(),
		     py::keep_alive<1, 2>(), // Keep OmapCard alive until this is destructed
		     py::arg("card"), py::arg("width"), py::arg("height"), py::arg("pixfmt"), py::arg("flags") = OmapFramebuffer::None)
		.def_prop_ro("format", &OmapFramebuffer::format)
		.def_prop_ro("num_planes", &OmapFramebuffer::num_planes)
		.def("fd", &OmapFramebuffer::prime_fd, py::arg("plane"))
		.def("stride", &OmapFramebuffer::stride, py::arg("plane"))
		.def("offset", &OmapFramebuffer::offset, py::arg("plane"));
}
