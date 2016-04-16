#pragma once

#include <GLES2/gl2.h>

class GlScene
{
public:
	GlScene();

	GlScene(const GlScene& other) = delete;
	GlScene& operator=(const GlScene& other) = delete;

	void set_viewport(uint32_t width, uint32_t height);

	void draw(uint32_t framenum);

private:
	GLint m_modelviewmatrix, m_modelviewprojectionmatrix, m_normalmatrix;

	uint32_t m_width;
	uint32_t m_height;
};
