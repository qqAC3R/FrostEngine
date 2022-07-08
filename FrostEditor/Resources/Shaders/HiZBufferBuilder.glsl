#type compute
#version 460

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D i_Depth;
layout(set = 0, binding = 1, r32f) uniform writeonly image2D o_Depth;

layout(push_constant) uniform PushConstant
{
	vec2 u_ImageSize;
} u_PushConstant;

void main()
{
	uvec2 pos = gl_GlobalInvocationID.xy;

#if 0
	// Method 1:
	// Sampler is set up to do min reduction, so this computes the minimum depth of a 2x2 texel quad
	float depth = texture(i_Depth, (vec2(pos) + vec2(0.5f)) / u_PushConstant.u_ImageSize).x;
#endif
	
#if 0
	// Method 2:
	// Texture gather approach
	vec4 depths = textureGather(i_Depth, vec2(pos) / u_PushConstant.u_ImageSize, 0);
	
	float depth = max(max(depths.x, depths.y), max(depths.z, depths.w));
#endif
	

#if 1
	// Method 3:
	// Texture offset approach
	float depth1 = textureOffset(i_Depth, (vec2(pos) + vec2(0.5f)) / u_PushConstant.u_ImageSize, ivec2(-1,  0)).x; // left
	float depth2 = textureOffset(i_Depth, (vec2(pos) + vec2(0.5f)) / u_PushConstant.u_ImageSize, ivec2( 1,  0)).x; // right
	float depth3 = textureOffset(i_Depth, (vec2(pos) + vec2(0.5f)) / u_PushConstant.u_ImageSize, ivec2( 0,  1)).x; // up
	float depth4 = textureOffset(i_Depth, (vec2(pos) + vec2(0.5f)) / u_PushConstant.u_ImageSize, ivec2( 0, -1)).x; // down
	
	float depth = max(max(depth1, depth2), max(depth3, depth4));
#endif

	imageStore(o_Depth, ivec2(pos), vec4(depth));
}