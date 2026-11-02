#version 450
// Native pass "fighters" (runtime/gs/gpu/native/fighters.cpp): a textured GS triangle's colour, as the GS combines it:
// S, T and Q interpolated linearly on the screen and divided (perspective), the texture from paraLLEl-GS's own image
// (a replacement's included) with the texture entry's clamp, MODULATE (texel x vertex / 128) or DECAL, the vertex's
// alpha without TCC. The blend is the pipeline's: the source as it is, or subtracted from the frame (the cel shading
// layer: (0 - Cs) x FIX / 128 + Cd, here Cs x FIX / 128 with a reverse subtraction, clamped at 0 by the format).
// With FLAG_LAYERED the triangle is a piece's base and its cel-shading layer at the same positions in one: the base as
// above, less the second layer's DECAL texel x FIX / 128 (clamped at 0), the base's alpha.
layout(set = 0, binding = 0) uniform sampler2D tex;
layout(set = 0, binding = 1) uniform sampler2D tex2;

layout(push_constant) uniform Push
{
	vec4 area;      // fighters.vert
	vec4 image;
	vec4 scale[2];  // Per layer: texels to the image's coordinates, xy multiply, zw add.
	vec4 region[2]; // Per layer: the first and last texels of the clamp (x, y, x, y).
	vec4 sizes;     // Width and height in texels: the first layer's xy, the second's zw.
	uvec4 mode;     // x: the first layer's flags, y: its FIX; z, w: the second's.
} push;

const uint FLAG_PERSPECTIVE = 1u, FLAG_TCC = 2u, FLAG_DECAL = 4u, FLAG_SUBTRACT = 8u, FLAG_LAYERED = 16u;
const uint FLAG_CLAMP_S = 32u, FLAG_CLAMP_T = 64u;

layout(location = 0) noperspective in vec3 v_stq;
layout(location = 1) noperspective in vec4 v_colour;
layout(location = 2) noperspective in vec3 v_stq2;
layout(location = 0) out vec4 out_colour;

vec4 texel(sampler2D image, vec3 stq, uint flags, vec2 size, vec4 scale, vec4 region)
{
	vec2 texels = (flags & FLAG_PERSPECTIVE) != 0u ? stq.xy / stq.z * size : stq.xy;
	vec2 clamped = clamp(texels, region.xy + 0.5, region.zw + 0.5);
	texels = mix(texels, clamped, bvec2((flags & FLAG_CLAMP_S) != 0u, (flags & FLAG_CLAMP_T) != 0u));
	return floor(textureLod(image, texels * scale.xy + scale.zw, 0.0) * 255.0 + 0.5);
}

void main()
{
	uint flags = push.mode.x;
	vec4 vertex = floor(v_colour + 0.5);
	vec4 t = texel(tex, v_stq, flags, push.sizes.xy, push.scale[0], push.region[0]);
	vec4 source = (flags & FLAG_DECAL) != 0u ? t : min(floor(t * vertex / 128.0), vec4(255.0));
	if ((flags & FLAG_TCC) == 0u)
		source.a = vertex.a;
	if ((flags & FLAG_SUBTRACT) != 0u)
		source.rgb = floor(source.rgb * float(push.mode.y) / 128.0);
	if ((flags & FLAG_LAYERED) != 0u)
	{
		vec4 cel = texel(tex2, v_stq2, push.mode.z, push.sizes.zw, push.scale[1], push.region[1]);
		source.rgb = max(source.rgb - floor(cel.rgb * float(push.mode.w) / 128.0), vec3(0.0));
	}
	out_colour = source / 255.0;
}
