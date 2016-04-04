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


/*
 * Tests that accessing out of bounds resources does not produce non-zero data
 * or crashes.
 */

#include "piglit-util-gl.h"

static struct piglit_gl_test_config *piglit_config;

PIGLIT_GL_TEST_CONFIG_BEGIN
	piglit_config = &config;
	config.supports_gl_compat_version = 43;
	config.supports_gl_core_version = 43;
	config.require_robust_context = true;
PIGLIT_GL_TEST_CONFIG_END


enum piglit_result
piglit_display(void)
{
	return PIGLIT_FAIL;
}

static const char *compute_shader_source_constants =
	"#version 430\n"
	"layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
	"\n"
	"layout(binding = 0) uniform Test {\n"
	"    vec4 data;\n"
	"} arr[2];\n"
	"\n"
	"layout(binding = 0) buffer Out {\n"
	"    vec4 results[];\n"
	"};\n"
	"void main()\n"
	"{\n"
	"    for (int i = 0; i < 65536; ++i)\n"
	"        results[i] = arr[i].data;\n"
	"}\n";

static const char *compute_shader_source_ssbo =
	"#version 430\n"
	"layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
	"\n"
	"layout(binding = 1) buffer Test {\n"
	"    vec4 data;\n"
	"} arr[2];\n"
	"\n"
	"layout(binding = 0) buffer Out {\n"
	"    vec4 results[];\n"
	"};\n"
	"void main()\n"
	"{\n"
	"    for (int i = 0; i < 65536; ++i)\n"
	"        results[i] = arr[i].data;\n"
	"}\n";

static const char *compute_shader_source_texture =
	"#version 430\n"
	"layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
	"\n"
	"layout(binding = 0) uniform sampler2D  arr[2];\n"
	"\n"
	"layout(binding = 0) buffer Out {\n"
	"    vec4 results[];\n"
	"};\n"
	"void main()\n"
	"{\n"
	"    for (int i = 0; i < 65536; ++i)\n"
	"        results[i] = texelFetch(arr[i], ivec2(0, 0), 0);\n"
	"}\n";

static const char *compute_shader_source_image =
	"#version 430\n"
	"layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
	"\n"
	"layout(binding = 0,rgba8) uniform image2D  arr[2];\n"
	"\n"
	"layout(binding = 0) buffer Out {\n"
	"    vec4 results[];\n"
	"};\n"
	"void main()\n"
	"{\n"
	"    for (int i = 0; i < 65536; ++i)\n"
	"        results[i] = imageLoad(arr[i], ivec2(0, 0));\n"
	"}\n";

bool test_single(const char *name, const char *source,
                float alpha) {
	GLuint write_buffer, shader, prog;
	float *data;
	int i, j;
	bool result = true;
	int size = 65536;

	glGenBuffers(1, &write_buffer);
	if (!piglit_check_gl_error(GL_NO_ERROR)) {
		piglit_report_subtest_result(PIGLIT_FAIL, "%s", name);
		return false;
	}


	glBindBuffer(GL_SHADER_STORAGE_BUFFER, write_buffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, 16 * size, NULL,GL_DYNAMIC_DRAW);
	data = (float*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, 16 * size,
	                                GL_MAP_WRITE_BIT);
	for (i = 0; i < 4*size; ++i)
		data[i] = -1.0;
	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

	shader = piglit_compile_shader_text_nothrow(GL_COMPUTE_SHADER, source);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, write_buffer);
	prog = glCreateProgram();
	glAttachShader(prog, shader);
	glLinkProgram(prog);
	glDeleteShader(shader);

	glUseProgram(prog);
	glDispatchCompute(1, 1, 1);

	data = (float*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, 16 * size,
	                                GL_MAP_READ_BIT);

	for (i = 0; i < 4*size; i += 4)
		for (j = i; j < i + 4; ++j)
			if (data[j] != 0.0 && (j != i  + 3 || data[j] != alpha)) {
				result = false;
			}

	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

	glDeleteBuffers(1, &write_buffer);
	glDeleteProgram(prog);

	piglit_report_subtest_result(result ? PIGLIT_PASS : PIGLIT_FAIL,
	                             "%s", name);
	return result;
}

void
piglit_init(int argc, char **argv)
{
	bool result = true;

	result &= test_single("ubo", compute_shader_source_constants, 0.0f);
	result &= test_single("ssbo", compute_shader_source_ssbo, 0.0f);
	result &= test_single("texture", compute_shader_source_texture, 1.0f);
	result &= test_single("image", compute_shader_source_image, 0.0f);

	piglit_report_result(result ? PIGLIT_PASS : PIGLIT_FAIL);
}
