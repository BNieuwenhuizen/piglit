/*
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "piglit-util-gl.h"

/*
 * Checks that out of bounds SSBO writes are discard. Is stricter than the spec
 * requires as we check containment into the bound range, while the spec only
 * requires containment into the bound object.
 */

static struct piglit_gl_test_config *piglit_config;

PIGLIT_GL_TEST_CONFIG_BEGIN
	piglit_config = &config;
	config.supports_gl_compat_version = 42;
	config.supports_gl_core_version = 42;
	config.require_robust_context = true;
PIGLIT_GL_TEST_CONFIG_END


enum piglit_result
piglit_display(void)
{
	return PIGLIT_FAIL;
}

static const char *compute_shader_source =
	"#version 420\n"
	"#extension GL_ARB_compute_shader : require\n"
	"#extension GL_ARB_shader_storage_buffer_object : require\n"
	"layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;\n"
	"\n"
	"layout(binding = 0, std430) buffer Test {\n"
	"    int arr[];\n"
	"};\n"
	"\n"
	"void main()\n"
	"{\n"
	"    for(int i = 2048 - 64 + int(gl_LocalInvocationID.x); i >= 0; i -= 64)\n"
	"        arr[i] = 1000000000;\n"
	"}\n";

void
piglit_init(int argc, char **argv)
{
	GLuint write_buffer;
	int *data, i;
	GLuint shader, prog;

	piglit_require_extension("GL_ARB_compute_shader");
	piglit_require_extension("GL_ARB_shader_storage_buffer_object");
	piglit_require_extension("GL_ARB_robust_buffer_access_behavior");

	glGenBuffers(1, &write_buffer);
	if (!piglit_check_gl_error(GL_NO_ERROR)) {
		piglit_report_result(PIGLIT_FAIL);
		return;
	}


	glBindBuffer(GL_SHADER_STORAGE_BUFFER, write_buffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, 4*1024, NULL,GL_DYNAMIC_DRAW);
	data = (int*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, 4*1024,
	                              GL_MAP_WRITE_BIT);
	for (i = 0; i < 1024; ++i)
		data[i] = i;
	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

	shader = piglit_compile_shader_text_nothrow(GL_COMPUTE_SHADER, compute_shader_source);

	glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 0, write_buffer, 0, 256);
	prog = glCreateProgram();
	glAttachShader(prog, shader);
	glLinkProgram(prog);
	glDeleteShader(shader);

	glUseProgram(prog);
	glDispatchCompute(1, 1, 1);

	data = (int*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, 4*1024,
	                              GL_MAP_READ_BIT);
	for (i = 0; i < 1024; ++i) {
		if ((i < 64 && data[i] != 1000000000) ||
		    (i >= 64 && data[i] != i)) {
			piglit_report_result(PIGLIT_FAIL);
			return;
		}
	}
	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
	piglit_report_result(PIGLIT_PASS);
}
