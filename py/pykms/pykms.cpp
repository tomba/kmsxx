#include <nanobind/nanobind.h>
#include <kms++/kms++.h>

namespace py = nanobind;

using namespace kms;
using namespace std;

void init_pykmsbase(py::module_& m);

#if HAS_KMSXXUTIL
void init_pykmsutils(py::module_& m);
#endif

#if HAS_LIBDRM_OMAP
void init_pykmsomap(py::module_& m);
#endif

NB_MODULE(pykms, m)
{
	m.doc() = "Python bindings for kms++.";

	init_pykmsbase(m);

#if HAS_KMSXXUTIL
	init_pykmsutils(m);
	m.def("has_pykmsutils", []() { return true; },
	      R"doc(Return whether kms++util helper bindings are available.)doc");
#else
	m.def("has_pykmsutils", []() { return false; },
	      R"doc(Return whether kms++util helper bindings are available.)doc");
#endif

#if HAS_LIBDRM_OMAP
	init_pykmsomap(m);
#endif
}
