#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <kms++/kms++.h>

namespace py = pybind11;

using namespace kms;
using namespace std;

void init_pykmsbase(py::module& m);

#if HAS_KMSXXUTIL
void init_pykmsutils(py::module& m);
#endif

#if HAS_LIBDRM_OMAP
void init_pykmsomap(py::module& m);
#endif

PYBIND11_MODULE(pykms, m)
{
	init_pykmsbase(m);

#if HAS_KMSXXUTIL
	init_pykmsutils(m);
#endif

#if HAS_LIBDRM_OMAP
	init_pykmsomap(m);
#endif
}
