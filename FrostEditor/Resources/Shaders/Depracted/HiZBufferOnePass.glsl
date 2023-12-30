#type compute
#version 460

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(binding = 0) uniform sampler2D i_Depth;
layout(binding = 1, rg32f) uniform image2D o_Depth[2];

layout(push_constant) uniform PushConstant
{
	vec2 u_ImageSize;
	float MipLevel;
} u_PushConstant;

void main()
{
	ivec2 invoke = ivec2(gl_GlobalInvocationID.xy);
	{
		ivec2 currentImageSize = imageSize(o_Depth[0]).xy;
		float depth = texelFetch(i_Depth, invoke, 0).r;

		imageStore(o_Depth[0], invoke, vec4(depth, depth, 0.0f, 0.0f));
	}

	barrier();

	if(gl_LocalInvocationID.x < 16 && gl_LocalInvocationID.y < 16)
	{
		ivec2 tileID = ivec2(gl_WorkGroupID.xy);
		//ivec2 localInvocation = ivec2(gl_LocalInvocationID.xy);
		ivec2 tileNumber = ivec2(gl_NumWorkGroups.xy);
		//ivec2 coordsLoad = (tileID * 16) + (localInvocation * 2);
		//ivec2 coordsLoad = (tileID.y * tileNumber.x + tileID.x) + (localInvocation * 2);
		//ivec2 coordsWrite = (tileID * 16) + (localInvocation);



		float depth = imageLoad(o_Depth[0], coordsLoad).r;

		//uint index = tileID.y * tileNumber.x + tileID.x;

		//float depth1 = imageLoad(o_Depth[0], coordsLoad + ivec2(-1,  0)).r; // left
		//float depth2 = imageLoad(o_Depth[0], coordsLoad + ivec2( 1,  0)).r; // right
		//float depth3 = imageLoad(o_Depth[0], coordsLoad + ivec2( 0,  1)).r; // up
		//float depth4 = imageLoad(o_Depth[0], coordsLoad + ivec2( 0, -1)).r; // down
	
		//float depthMin = min(min(depth1, depth2), min(depth3, depth4));
		//float depthMax = max(max(depth1, depth2), max(depth3, depth4));
		imageStore(o_Depth[1], coordsWrite, vec4(depth, depth, 0.0f, 0.0f));
	}
}