#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <kms++/kms++.h>
#include "../../kms++/inc/kms++/ion/ionkms++.h"

namespace py = pybind11;

using namespace kms;
using namespace std;

void init_pykmsion(py::module &m)
{
	py::class_<Ion>(m, "Ion")
			.def(py::init<>())
			.def_property_readonly("fd", &Ion::fd)
			;
}
