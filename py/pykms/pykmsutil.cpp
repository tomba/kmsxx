#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <kms++/kms++.h>
#include <kms++util/kms++util.h>

namespace py = pybind11;

using namespace kms;
using namespace std;

void init_pykmstest(py::module &m)
{
	py::class_<RGB>(m, "RGB")
			.def(py::init<>())
			.def(py::init<uint8_t, uint8_t, uint8_t&>())
			.def(py::init<uint8_t, uint8_t, uint8_t, uint8_t&>())
			.def_property_readonly("rgb888", &RGB::rgb888)
			.def_property_readonly("argb8888", &RGB::argb8888)
			.def_property_readonly("abgr8888", &RGB::abgr8888)
			.def_property_readonly("rgb565", &RGB::rgb565)
			;

	py::class_<ResourceManager>(m, "ResourceManager")
			.def(py::init<Card&>())
			.def("reset", &ResourceManager::reset)
			.def("reserve_connector", (Connector* (ResourceManager::*)(const string& name))&ResourceManager::reserve_connector,
			     py::arg("name") = string())
			.def("reserve_crtc", &ResourceManager::reserve_crtc)
			.def("reserve_plane", (Plane* (ResourceManager::*)(Crtc*, PlaneType, PixelFormat)) &ResourceManager::reserve_plane,
			     py::arg("crtc"),
			     py::arg("type"),
			     py::arg("format") = PixelFormat::Undefined)
			.def("reserve_plane", (Plane* (ResourceManager::*)(Crtc*, PixelFormat)) &ResourceManager::reserve_plane,
			     py::arg("crtc"),
			     py::arg("format") = PixelFormat::Undefined)
			.def("reserve_primary_plane", &ResourceManager::reserve_primary_plane,
			     py::arg("crtc"),
			     py::arg("format") = PixelFormat::Undefined)
			.def("reserve_overlay_plane", &ResourceManager::reserve_overlay_plane,
			     py::arg("crtc"),
			     py::arg("format") = PixelFormat::Undefined)
			;

	// Use lambdas to handle IMappedFramebuffer
	m.def("draw_test_pattern", [](MappedFramebuffer& fb) { draw_test_pattern(fb); } );
	m.def("draw_color_bar", [](MappedFramebuffer& fb, int old_xpos, int xpos, int width) {
		draw_color_bar(fb, old_xpos, xpos, width);
	} );
	m.def("draw_rect", [](MappedFramebuffer& fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h, RGB color) {
		draw_rect(fb, x, y, w, h, color);
	} );
}
