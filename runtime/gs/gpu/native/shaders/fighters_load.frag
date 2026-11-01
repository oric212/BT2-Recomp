#version 450
// Native pass "fighters" (runtime/gs/gpu/native/fighters.cpp): the sample image's colour and depth from GS memory as
// paraLLEl-GS holds it (per 32-bit word the resolved value, the super-sample reference one plane on, sample s at
// plane s + 2; a pixel without samples has only its resolved value). Each image pixel is one sample of a game pixel.
layout(set = 0, binding = 0, std430) readonly buffer GSMemory { uint words[]; };

layout(push_constant) uniform Push
{
	uvec4 area;    // The image's first game pixel (x, y), samples per game pixel along a side, reference planes (1: present).
	uvec4 buffers; // The frame's block and width; the depth buffer's block and width (64-word blocks, 64-pixel units).
	uvec4 memory;  // Words per plane.
} push;

layout(location = 0) out vec4 out_colour;

// The PSMCT32 word of pixel p (64 x 32 pixel pages of 32 blocks, 8 x 8 pixel blocks); Z formats take the same layout
// with the block index XORed by 0x18.
uint word_at(uint block, uint width, uvec2 p, uint block_xor)
{
	uint bx = (p.x & 63u) >> 3u, by = (p.y & 31u) >> 3u;
	uint b = ((bx & 1u) | ((by & 1u) << 1u) | ((bx & 2u) << 1u) | ((by & 2u) << 2u) | ((bx & 4u) << 2u)) ^ block_xor;
	uint px = p.x & 7u, py = p.y & 7u;
	uint pixel = (px & 1u) | ((py & 1u) << 1u) | ((px & 6u) << 1u) | ((py & 6u) << 3u);
	return (block * 64u + ((p.y >> 5u) * width + (p.x >> 6u)) * 2048u + b * 64u + pixel) & 0xFFFFFu;
}

void main()
{
	uint side = push.area.z;
	uvec2 f = uvec2(gl_FragCoord.xy);
	uvec2 p = push.area.xy + f / side, o = f % side;
	// The renderer's sample index of the offset (o.x, o.y): bits 0 and 2 are y, bits 1 and 3 are x.
	uint s = (o.y & 1u) | ((o.x & 1u) << 1u) | ((o.y >> 1u) << 2u) | ((o.x >> 1u) << 3u);
	uint plane = push.memory.x;
	uint ci = word_at(push.buffers.x, push.buffers.y, p, 0u);
	uint zi = word_at(push.buffers.z, push.buffers.w, p, 0x18u);
	bool reference = push.area.w != 0u && side > 1u;
	uint colour = reference && words[ci + plane] == ~0u ? words[ci + (s + 2u) * plane] : words[ci];
	uint depth = reference && (words[zi + plane] & 0xFFFFFFu) == 0xFFFFFFu ? words[zi + (s + 2u) * plane] : words[zi];
	out_colour = unpackUnorm4x8(colour);
	gl_FragDepth = float(depth & 0xFFFFFFu) / 16777215.0;
}
