#include "esTransform.h"
#include "cube-gles2.h"
#include "cube.h"

#include <kms++util/kms++util.h>

using namespace std;

GlScene::GlScene()
{
	GLuint vertex_shader, fragment_shader;
	GLint ret;

	static const GLfloat vVertices[] = {
		// front
		-1.0f, -1.0f, +1.0f, // point blue
		+1.0f, -1.0f, +1.0f, // point magenta
		-1.0f, +1.0f, +1.0f, // point cyan
		+1.0f, +1.0f, +1.0f, // point white
		// back
		+1.0f, -1.0f, -1.0f, // point red
		-1.0f, -1.0f, -1.0f, // point black
		+1.0f, +1.0f, -1.0f, // point yellow
		-1.0f, +1.0f, -1.0f, // point green
		// right
		+1.0f, -1.0f, +1.0f, // point magenta
		+1.0f, -1.0f, -1.0f, // point red
		+1.0f, +1.0f, +1.0f, // point white
		+1.0f, +1.0f, -1.0f, // point yellow
		// left
		-1.0f, -1.0f, -1.0f, // point black
		-1.0f, -1.0f, +1.0f, // point blue
		-1.0f, +1.0f, -1.0f, // point green
		-1.0f, +1.0f, +1.0f, // point cyan
		// top
		-1.0f, +1.0f, +1.0f, // point cyan
		+1.0f, +1.0f, +1.0f, // point white
		-1.0f, +1.0f, -1.0f, // point green
		+1.0f, +1.0f, -1.0f, // point yellow
		// bottom
		-1.0f, -1.0f, -1.0f, // point black
		+1.0f, -1.0f, -1.0f, // point red
		-1.0f, -1.0f, +1.0f, // point blue
		+1.0f, -1.0f, +1.0f  // point magenta
	};

	static const GLfloat vColors[] = {
		// front
		0.0f,  0.0f,  1.0f, // blue
		1.0f,  0.0f,  1.0f, // magenta
		0.0f,  1.0f,  1.0f, // cyan
		1.0f,  1.0f,  1.0f, // white
		// back
		1.0f,  0.0f,  0.0f, // red
		0.0f,  0.0f,  0.0f, // black
		1.0f,  1.0f,  0.0f, // yellow
		0.0f,  1.0f,  0.0f, // green
		// right
		1.0f,  0.0f,  1.0f, // magenta
		1.0f,  0.0f,  0.0f, // red
		1.0f,  1.0f,  1.0f, // white
		1.0f,  1.0f,  0.0f, // yellow
		// left
		0.0f,  0.0f,  0.0f, // black
		0.0f,  0.0f,  1.0f, // blue
		0.0f,  1.0f,  0.0f, // green
		0.0f,  1.0f,  1.0f, // cyan
		// top
		0.0f,  1.0f,  1.0f, // cyan
		1.0f,  1.0f,  1.0f, // white
		0.0f,  1.0f,  0.0f, // green
		1.0f,  1.0f,  0.0f, // yellow
		// bottom
		0.0f,  0.0f,  0.0f, // black
		1.0f,  0.0f,  0.0f, // red
		0.0f,  0.0f,  1.0f, // blue
		1.0f,  0.0f,  1.0f  // magenta
	};

	static const GLfloat vNormals[] = {
		// front
		+0.0f, +0.0f, +1.0f, // forward
		+0.0f, +0.0f, +1.0f, // forward
		+0.0f, +0.0f, +1.0f, // forward
		+0.0f, +0.0f, +1.0f, // forward
		// back
		+0.0f, +0.0f, -1.0f, // backbard
		+0.0f, +0.0f, -1.0f, // backbard
		+0.0f, +0.0f, -1.0f, // backbard
		+0.0f, +0.0f, -1.0f, // backbard
		// right
		+1.0f, +0.0f, +0.0f, // right
		+1.0f, +0.0f, +0.0f, // right
		+1.0f, +0.0f, +0.0f, // right
		+1.0f, +0.0f, +0.0f, // right
		// left
		-1.0f, +0.0f, +0.0f, // left
		-1.0f, +0.0f, +0.0f, // left
		-1.0f, +0.0f, +0.0f, // left
		-1.0f, +0.0f, +0.0f, // left
		// top
		+0.0f, +1.0f, +0.0f, // up
		+0.0f, +1.0f, +0.0f, // up
		+0.0f, +1.0f, +0.0f, // up
		+0.0f, +1.0f, +0.0f, // up
		// bottom
		+0.0f, -1.0f, +0.0f, // down
		+0.0f, -1.0f, +0.0f, // down
		+0.0f, -1.0f, +0.0f, // down
		+0.0f, -1.0f, +0.0f  // down
	};

	static const char *vertex_shader_source =
			"uniform mat4 modelviewMatrix;      \n"
			"uniform mat4 modelviewprojectionMatrix;\n"
			"uniform mat3 normalMatrix;         \n"
			"                                   \n"
			"attribute vec4 in_position;        \n"
			"attribute vec3 in_normal;          \n"
			"attribute vec4 in_color;           \n"
			"\n"
			"vec4 lightSource = vec4(2.0, 2.0, 20.0, 0.0);\n"
			"                                   \n"
			"varying vec4 vVaryingColor;        \n"
			"                                   \n"
			"void main()                        \n"
			"{                                  \n"
			"    gl_Position = modelviewprojectionMatrix * in_position;\n"
			"    vec3 vEyeNormal = normalMatrix * in_normal;\n"
			"    vec4 vPosition4 = modelviewMatrix * in_position;\n"
			"    vec3 vPosition3 = vPosition4.xyz / vPosition4.w;\n"
			"    vec3 vLightDir = normalize(lightSource.xyz - vPosition3);\n"
			"    float diff = max(0.0, dot(vEyeNormal, vLightDir));\n"
			"    vVaryingColor = vec4(diff * in_color.rgb, 1.0);\n"
			"}                                  \n";

	static const char *fragment_shader_source =
			"precision mediump float;           \n"
			"                                   \n"
			"varying vec4 vVaryingColor;        \n"
			"                                   \n"
			"void main()                        \n"
			"{                                  \n"
			"    gl_FragColor = vVaryingColor;  \n"
			"}                                  \n";


	if (s_verbose) {
		printf("GL_VENDOR:       %s\n", glGetString(GL_VENDOR));
		printf("GL_VERSION:      %s\n", glGetString(GL_VERSION));
		printf("GL_RENDERER:     %s\n", glGetString(GL_RENDERER));
		printf("GL_EXTENSIONS:   %s\n", glGetString(GL_EXTENSIONS));
	}

	vertex_shader = glCreateShader(GL_VERTEX_SHADER);

	glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
	glCompileShader(vertex_shader);

	glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &ret);
	FAIL_IF(!ret, "vertex shader compilation failed!");

	fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

	glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
	glCompileShader(fragment_shader);

	glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &ret);
	FAIL_IF(!ret, "fragment shader compilation failed!");

	GLuint program = glCreateProgram();

	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);

	glBindAttribLocation(program, 0, "in_position");
	glBindAttribLocation(program, 1, "in_normal");
	glBindAttribLocation(program, 2, "in_color");

	glLinkProgram(program);

	glGetProgramiv(program, GL_LINK_STATUS, &ret);
	FAIL_IF(!ret, "program linking failed!");

	glUseProgram(program);

	m_modelviewmatrix = glGetUniformLocation(program, "modelviewMatrix");
	m_modelviewprojectionmatrix = glGetUniformLocation(program, "modelviewprojectionMatrix");
	m_normalmatrix = glGetUniformLocation(program, "normalMatrix");

	glEnable(GL_CULL_FACE);

	GLintptr positionsoffset = 0;
	GLintptr colorsoffset = sizeof(vVertices);
	GLintptr normalsoffset = sizeof(vVertices) + sizeof(vColors);
	GLuint vbo;

	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vVertices) + sizeof(vColors) + sizeof(vNormals), 0, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, positionsoffset, sizeof(vVertices), &vVertices[0]);
	glBufferSubData(GL_ARRAY_BUFFER, colorsoffset, sizeof(vColors), &vColors[0]);
	glBufferSubData(GL_ARRAY_BUFFER, normalsoffset, sizeof(vNormals), &vNormals[0]);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (const GLvoid*)positionsoffset);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (const GLvoid*)normalsoffset);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, (const GLvoid*)colorsoffset);
	glEnableVertexAttribArray(2);
}

void GlScene::set_viewport(uint32_t width, uint32_t height)
{
	m_width = width;
	m_height = height;
}

void GlScene::draw(uint32_t framenum)
{
	glViewport(0, 0, m_width, m_height);

	glClearColor(0.5, 0.5, 0.5, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);

	ESMatrix modelview;

	esMatrixLoadIdentity(&modelview);
	esTranslate(&modelview, 0.0f, 0.0f, -8.0f);
	esRotate(&modelview, 45.0f + (0.75f * framenum), 1.0f, 0.0f, 0.0f);
	esRotate(&modelview, 45.0f - (0.5f * framenum), 0.0f, 1.0f, 0.0f);
	esRotate(&modelview, 10.0f + (0.45f * framenum), 0.0f, 0.0f, 1.0f);

	GLfloat aspect = (float)m_height / m_width;

	ESMatrix projection;
	esMatrixLoadIdentity(&projection);
	esFrustum(&projection, -2.8f, +2.8f, -2.8f * aspect, +2.8f * aspect, 6.0f, 10.0f);

	ESMatrix modelviewprojection;
	esMatrixLoadIdentity(&modelviewprojection);
	esMatrixMultiply(&modelviewprojection, &modelview, &projection);

	float normal[9];
	normal[0] = modelview.m[0][0];
	normal[1] = modelview.m[0][1];
	normal[2] = modelview.m[0][2];
	normal[3] = modelview.m[1][0];
	normal[4] = modelview.m[1][1];
	normal[5] = modelview.m[1][2];
	normal[6] = modelview.m[2][0];
	normal[7] = modelview.m[2][1];
	normal[8] = modelview.m[2][2];

	glUniformMatrix4fv(m_modelviewmatrix, 1, GL_FALSE, &modelview.m[0][0]);
	glUniformMatrix4fv(m_modelviewprojectionmatrix, 1, GL_FALSE, &modelviewprojection.m[0][0]);
	glUniformMatrix3fv(m_normalmatrix, 1, GL_FALSE, normal);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glDrawArrays(GL_TRIANGLE_STRIP, 4, 4);
	glDrawArrays(GL_TRIANGLE_STRIP, 8, 4);
	glDrawArrays(GL_TRIANGLE_STRIP, 12, 4);
	glDrawArrays(GL_TRIANGLE_STRIP, 16, 4);
	glDrawArrays(GL_TRIANGLE_STRIP, 20, 4);
}
