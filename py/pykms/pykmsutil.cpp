#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <kms++/kms++.h>
#include <kms++util/kms++util.h>

namespace py = nanobind;

using namespace kms;
using namespace std;

void init_pykmsutils(py::module_& m)
{
	py::class_<RGB>(m, "RGB", R"doc(
Packed RGB color helper.
)doc")
		.def(py::init<>())
		.def(py::init<uint8_t, uint8_t, uint8_t>(),
		     py::arg("r"), py::arg("g"), py::arg("b"))
		.def(py::init<uint8_t, uint8_t, uint8_t, uint8_t>(),
		     py::arg("a"), py::arg("r"), py::arg("g"), py::arg("b"))
		.def_prop_ro("rgb888", &RGB::rgb888)
		.def_prop_ro("argb8888", &RGB::argb8888)
		.def_prop_ro("abgr8888", &RGB::abgr8888)
		.def_prop_ro("rgb565", &RGB::rgb565);

	py::class_<ResourceManager>(m, "ResourceManager", R"doc(
Simple allocator for connectors, CRTCs, and planes on a Card.
)doc")
		.def(py::init<Card&>(),
		     py::arg("card"), py::keep_alive<1, 2>())
		.def("reset", &ResourceManager::reset)
		.def("reserve_connector", (Connector * (ResourceManager::*)(const string& name)) & ResourceManager::reserve_connector,
		     py::arg("name") = string(), py::rv_policy::reference_internal)
		.def("reserve_crtc", (Crtc * (ResourceManager::*)(Connector*)) & ResourceManager::reserve_crtc,
		     py::arg("connector"), py::rv_policy::reference_internal)
		.def("reserve_plane", (Plane * (ResourceManager::*)(Crtc*, PlaneType, PixelFormat)) & ResourceManager::reserve_plane,
		     py::arg("crtc"),
		     py::arg("type"),
		     py::arg("format") = PixelFormat::Undefined, py::rv_policy::reference_internal)
		.def("reserve_generic_plane", &ResourceManager::reserve_generic_plane,
		     py::arg("crtc"),
		     py::arg("format") = PixelFormat::Undefined, py::rv_policy::reference_internal)
		.def("reserve_primary_plane", &ResourceManager::reserve_primary_plane,
		     py::arg("crtc"),
		     py::arg("format") = PixelFormat::Undefined, py::rv_policy::reference_internal)
		.def("reserve_overlay_plane", &ResourceManager::reserve_overlay_plane,
		     py::arg("crtc"),
		     py::arg("format") = PixelFormat::Undefined, py::rv_policy::reference_internal);
	py::enum_<YUVType>(m, "YUVType")
		.value("BT601_Lim", YUVType::BT601_Lim)
		.value("BT601_Full", YUVType::BT601_Full)
		.value("BT709_Lim", YUVType::BT709_Lim)
		.value("BT709_Full", YUVType::BT709_Full);

	// Use lambdas to handle IFramebuffer
	m.def(
		"draw_test_pattern", [](Framebuffer& fb, YUVType yuvt) { draw_test_pattern(fb); },
		py::arg("fb"),
		py::arg("yuvt") = YUVType::BT601_Lim,
		R"doc(
Draw the standard kms++ test pattern into a framebuffer.

Args:
    fb (Framebuffer): Destination framebuffer.
    yuvt (YUVType): YUV conversion type.
)doc");
	m.def("draw_color_bar", [](Framebuffer& fb, int old_xpos, int xpos, int width) {
		draw_color_bar(fb, old_xpos, xpos, width);
	}, py::arg("fb"), py::arg("old_xpos"), py::arg("xpos"), py::arg("width"),
	R"doc(Draw a moving color bar into a framebuffer.)doc");
	m.def("draw_rect", [](Framebuffer& fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h, RGB color) {
		draw_rect(fb, x, y, w, h, color);
	}, py::arg("fb"), py::arg("x"), py::arg("y"), py::arg("w"), py::arg("h"), py::arg("color"),
	R"doc(Draw a filled rectangle into a framebuffer.)doc");
	m.def("draw_circle", [](Framebuffer& fb, int32_t xCenter, int32_t yCenter, int32_t radius, RGB color) {
		draw_circle(fb, xCenter, yCenter, radius, color);
	}, py::arg("fb"), py::arg("xCenter"), py::arg("yCenter"), py::arg("radius"), py::arg("color"),
	R"doc(Draw a circle into a framebuffer.)doc");
	m.def("draw_text", [](Framebuffer& fb, uint32_t x, uint32_t y, const string& str, RGB color) { draw_text(fb, x, y, str, color); },
	      py::arg("fb"), py::arg("x"), py::arg("y"), py::arg("str"), py::arg("color"),
	      R"doc(Draw text into a framebuffer.)doc");
}
