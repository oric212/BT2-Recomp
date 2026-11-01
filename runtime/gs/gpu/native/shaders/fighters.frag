#version 450
// Native pass "fighters" (runtime/gs/gpu/native/fighters.cpp): a textured GS triangle's colour, as the GS combines it:
// S, T and Q interpolated linearly on the screen and divided (perspective), the texture from paraLLEl-GS's own image
// (a replacement's included) with the texture entry's clamp, MODULATE (texel x vertex / 128) or DECAL, the vertex's
// alpha without TCC. The blend is the pipeline's: the source as it is, or subtracted from the frame (the cel shading
// layer: (0 - Cs) x FIX / 128 + Cd, here Cs x FIX / 128 with a reverse subtraction, clamped at 0 by the format).
layout(set = 0, binding = 0) uniform sampler2D tex;

layout(push_constant) uniform Push
{
	vec4 area;   // fighters.vert
	vec4 image;
	vec4 scale;  // Texels to the image's coordinates: xy multiply, zw add.
	vec4 size;   // Width and height in texels; z, w non-zero: S, T clamped to the region.
	vec4 region; // The first and last texels of the clamp (x, y, x, y).
	uvec4 mode;  // x: flags; y: FIX.
} push;

const uint FLAG_PERSPECTIVE = 1u, FLAG_TCC = 2u, FLAG_DECAL = 4u, FLAG_SUBTRACT = 8u;

layout(location = 0) noperspective in vec3 v_stq;
layout(location = 1) noperspective in vec4 v_colour;
layout(location = 0) out vec4 out_colour;

void main()
{
	uint flags = push.mode.x;
	vec4 vertex = floor(v_colour + 0.5);
	vec2 texels = (flags & FLAG_PERSPECTIVE) != 0u ? v_stq.xy / v_stq.z * push.size.xy : v_stq.xy;
	vec2 clamped = clamp(texels, push.region.xy + 0.5, push.region.zw + 0.5);
	texels = mix(texels, clamped, notEqual(push.size.zw, vec2(0.0)));
	vec4 texel = floor(textureLod(tex, texels * push.scale.xy + push.scale.zw, 0.0) * 255.0 + 0.5);
	vec4 source = (flags & FLAG_DECAL) != 0u ? texel : min(floor(texel * vertex / 128.0), vec4(255.0));
	if ((flags & FLAG_TCC) == 0u)
		source.a = vertex.a;
	if ((flags & FLAG_SUBTRACT) != 0u)
		source.rgb = floor(source.rgb * float(push.mode.y) / 128.0);
	out_colour = source / 255.0;
}
