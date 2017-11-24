#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <kms++/kms++.h>

namespace py = pybind11;

using namespace kms;
using namespace std;

void init_pykmstest(py::module &m);
void init_pykmsbase(py::module &m);
void init_pyvid(py::module &m);

#if HAS_LIBDRM_OMAP
void init_pykmsomap(py::module &m);
#endif

PYBIND11_MODULE(pykms, m) {
	init_pykmsbase(m);

	init_pykmstest(m);

	init_pyvid(m);

#if HAS_LIBDRM_OMAP
	init_pykmsomap(m);
#endif
}
