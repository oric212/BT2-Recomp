#version 450
// Native pass "fighters" (runtime/gs/gpu/native/fighters.cpp): one triangle over the whole sample image, for
// fighters_load.frag.
void main()
{
	vec2 p = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
