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
 * Tests that out of bounds accesses to resources do not produce non-zero data
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
	"    vec4 data[256];\n"
	"};\n"
	"\n"
	"layout(binding = 0) buffer Out {\n"
	"    vec4 results[];\n"
	"};\n"
	"void main()\n"
	"{\n"
	"    results[256 * gl_GlobalInvocationID.y + gl_GlobalInvocationID.x] =\n"
	"                                       data[gl_GlobalInvocationID.x];\n"
	"}\n";

static const char *compute_shader_source_ssbo =
	"#version 430\n"
	"layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
	"\n"
	"layout(binding = 1) buffer Test {\n"
	"    vec4 data[256];\n"
	"};\n"
	"\n"
	"layout(binding = 0) buffer Out {\n"
	"    vec4 results[];\n"
	"};\n"
	"void main()\n"
	"{\n"
	"    results[256 * gl_GlobalInvocationID.y + gl_GlobalInvocationID.x] =\n"
	"                                       data[gl_GlobalInvocationID.x];\n"
	"}\n";

static const char *compute_shader_source_texture =
	"#version 430\n"
	"layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
	"\n"
	"layout(binding = 0) uniform sampler2D  tex;\n"
	"\n"
	"layout(binding = 0) buffer Out {\n"
	"    vec4 results[];\n"
	"};\n"
	"void main()\n"
	"{\n"
	"    results[256 * gl_GlobalInvocationID.y + gl_GlobalInvocationID.x] =\n"
	"             texelFetch(tex, ivec2(gl_GlobalInvocationID.xy), 0);\n"
	"}\n";

static const char *compute_shader_source_image =
	"#version 430\n"
	"layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
	"\n"
	"layout(binding = 0,rgba8) uniform image2D  tex;\n"
	"\n"
	"layout(binding = 0) buffer Out {\n"
	"    vec4 results[];\n"
	"};\n"
	"void main()\n"
	"{\n"
	"    results[256 * gl_GlobalInvocationID.y + gl_GlobalInvocationID.x] =\n"
	"                 imageLoad(tex, ivec2(gl_GlobalInvocationID.xy));\n"
	"}\n";

static const char *compute_shader_source_atomic =
	"#version 430\n"
	"layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
	"\n"
	"layout(binding = 0) uniform atomic_uint counter;\n"
	"\n"
	"layout(binding = 0) buffer Out {\n"
	"    vec4 results[];\n"
	"};\n"
	"void main()\n"
	"{\n"
	"    results[256 * gl_GlobalInvocationID.y + gl_GlobalInvocationID.x] =\n"
	"                 vec4(float(atomicCounterIncrement(counter)),\n"
	"                      0.0, 0.0, 0.0);\n"
	"}\n";

bool test_single(const char *name, const char *source, int defined_w,
                 int defined_h) {
	GLuint write_buffer, shader, prog;
	float *data;
	int i, j, k;
	bool result = true;

	glGenBuffers(1, &write_buffer);
	if (!piglit_check_gl_error(GL_NO_ERROR)) {
		piglit_report_subtest_result(PIGLIT_FAIL, "%s", name);
		return false;
	}


	glBindBuffer(GL_SHADER_STORAGE_BUFFER, write_buffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, 16 * 256*256, NULL,GL_DYNAMIC_DRAW);
	data = (float*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, 16 * 256 * 256,
	                                GL_MAP_WRITE_BIT);
	for (i = 0; i < 4*256*256; ++i)
		data[i] = -1.0;
	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

	shader = piglit_compile_shader_text_nothrow(GL_COMPUTE_SHADER, source);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, write_buffer);
	prog = glCreateProgram();
	glAttachShader(prog, shader);
	glLinkProgram(prog);
	glDeleteShader(shader);

	glUseProgram(prog);
	glDispatchCompute(256, 256, 1);

	data = (float*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, 16 * 256 * 256,
	                                GL_MAP_READ_BIT);



	for (i = 0; i < 256; ++i) {
		for (j = 0; j < 256; ++j) {
			float comp = 0.0;
			if (i < defined_h && j < defined_w)
				comp = 1.0;
			for (k = 0; k < 4; ++k) {
				if(data[i * 1024 + j * 4 + k] != comp)
					result = false;
			}
		}
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
	GLuint buffer, texture;
	bool result = true;
	int i;
	float *data = malloc(128 * 128 * 4 * sizeof(float));

	for (i = 0; i < 128 * 128 * 4; ++i)
		data[i] = 1.0;

	glGenBuffers(1, &buffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, buffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, 128 * 4 * sizeof(float), data, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_UNIFORM_BUFFER, 0, buffer);

	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 128, 128, 0, GL_RGBA, GL_FLOAT, data);
	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL,  0);
	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL,  0);

	glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);

	result &= test_single("ubo", compute_shader_source_constants, 128, 256);
	result &= test_single("ssbo", compute_shader_source_ssbo, 128, 256);
	result &= test_single("texture", compute_shader_source_texture, 128, 128);
	result &= test_single("image", compute_shader_source_image, 128, 128);
	result &= test_single("atomic", compute_shader_source_atomic, 0, 0);

	piglit_report_result(result ? PIGLIT_PASS : PIGLIT_FAIL);

	glDeleteBuffers(1, &buffer);
	glDeleteTextures(1, &texture);
	free(data);
}
