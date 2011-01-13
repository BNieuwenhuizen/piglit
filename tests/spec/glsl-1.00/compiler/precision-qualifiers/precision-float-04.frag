// [config]
// expect_result: pass
// glsl_version: 1.00
// [end config]
//
// Precision qualifiers can be applied to float matrices.
//
// From section 4.5.2 of the GLSL 1.00 spec:
//     Any floating point or any integer declaration can have the type
//     preceded by one of these precision qualifiers

#version 100

float f() {
	lowp mat4 m;
	return 0.0;
}
