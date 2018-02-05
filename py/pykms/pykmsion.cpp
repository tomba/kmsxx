#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <kms++/kms++.h>
#include <kms++/ion/ionkms++.h>
#include <kms++/ion/ionbuffer.h>

namespace py = pybind11;

using namespace kms;
using namespace std;

void init_pykmsion(py::module &m)
{
	py::class_<Ion>(m, "Ion")
			.def(py::init<>())
			.def_property_readonly("fd", &Ion::fd)
			.def("alloc", (int (Ion::*)(int, int))&Ion::alloc)
			.def("free", (int (Ion::*)(int))&Ion::free)
			;

	py::class_<IonBuffer>(m, "IonBuffer")
			.def(py::init<Ion&, int, int, uint32_t, uint32_t, const string&>(),
			     py::keep_alive<1, 2>())	// Keep Ion alive until this is destructed
			.def(py::init<Ion&, int, int, uint32_t, uint32_t, PixelFormat>(),
			     py::keep_alive<1, 2>())	// Keep Ion alive until this is destructed
			.def_property_readonly("format", &IonBuffer::format)
			.def_property_readonly("width", &IonBuffer::width)
			.def_property_readonly("height", &IonBuffer::height)
			.def_property_readonly("stride", &IonBuffer::stride)
			.def_property_readonly("offset", &IonBuffer::offset)
			.def_property_readonly("prime_fd", &IonBuffer::prime_fd)
			;
	m.attr("ION_HEAP_TYPE_SYSTEM") = py::int_(static_cast<int>(ION_HEAP_TYPE_SYSTEM));
	m.attr("ION_HEAP_TYPE_SYSTEM_CONTIG") = py::int_(static_cast<int>(ION_HEAP_TYPE_SYSTEM_CONTIG));
	m.attr("ION_HEAP_TYPE_CARVEOUT") = py::int_(static_cast<int>(ION_HEAP_TYPE_CARVEOUT));
	m.attr("ION_HEAP_TYPE_CHUNK") = py::int_(static_cast<int>(ION_HEAP_TYPE_CHUNK));
	m.attr("ION_HEAP_TYPE_DMA") = py::int_(static_cast<int>(ION_HEAP_TYPE_DMA));
}
