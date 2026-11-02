#version 450
// Native pass "fighters" (runtime/gs/gpu/native/fighters.cpp): a GS vertex onto the pass's sample image. The image
// has `side` x `side` pixels per game pixel, one per super-sample of paraLLEl-GS's regular grid (sample (i, j) at
// i / side, j / side of the game pixel), so a pixel's centre stands on its sample: game x at x * side + 0.5.
layout(location = 0) in ivec2 position; // 1/16 game pixels, after the offset.
layout(location = 1) in uint depth;     // The 24-bit depth.
layout(location = 2) in uvec4 colour;   // RGBA, 0 to 255 (a flat triangle's three alike).
layout(location = 3) in vec3 stq;       // S, T, Q; or U, V in texels and 1 (FST).
layout(location = 4) in vec3 stq2;      // The same of the cel-shading layer drawn with it (fighters.frag).

layout(push_constant) uniform Push
{
	vec4 area;   // The image's first game pixel (x, y), samples per game pixel along a side, unused.
	vec4 image;  // The image's size in pixels, unused.
	vec4 scale[2]; // fighters.frag
	vec4 region[2];
	vec4 sizes;
	uvec4 mode;
} push;

layout(location = 0) noperspective out vec3 v_stq;
layout(location = 1) noperspective out vec4 v_colour;
layout(location = 2) noperspective out vec3 v_stq2;

void main()
{
	vec2 p = (vec2(position) / 16.0 - push.area.xy) * push.area.z + 0.5;
	gl_Position = vec4(p / push.image.xy * 2.0 - 1.0, float(depth) / 16777215.0, 1.0);
	v_stq = stq;
	v_stq2 = stq2;
	v_colour = vec4(colour);
}
