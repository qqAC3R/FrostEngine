#type compute
#version 460

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D i_Depth_MinReduction;
layout(set = 0, binding = 1) uniform sampler2D i_Depth_MaxReduction;
layout(set = 0, binding = 2, rg32f) uniform writeonly image2D o_Depth;

layout(push_constant) uniform PushConstant
{
	vec2 u_ImageSize;
	float MipLevel;
} u_PushConstant;

void main()
{
	ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
	vec2 uv = (vec2(pos) + vec2(0.5)) / u_PushConstant.u_ImageSize; 
	ivec2 lastMipSize = ivec2(textureSize(i_Depth_MaxReduction, 0));
	

#if 1

	// Sampler is set up to do min/max reduction, so this computes the minimum depth of a 2x2 texel quad
	float depthMin = texture(i_Depth_MinReduction, uv).x;
	
	// More precision needed for occlusion culling!
	vec4 depths = vec4(1.0);
	float depthMax = 0.0;

	if(u_PushConstant.MipLevel == 0.0)
		depthMax = texture(i_Depth_MaxReduction, uv).x;
	else
	{
		depthMax = texture(i_Depth_MaxReduction, uv).y;
		if ((lastMipSize.x % 2 != 0) || (lastMipSize.y % 2 != 0))
		{
			float newDepth = texture(i_Depth_MaxReduction, (vec2(pos) + vec2(1.5)) / u_PushConstant.u_ImageSize).y;
			depthMax = max(newDepth, depthMax);
		}
	}

	/*
	if(u_PushConstant.MipLevel == 0.0)
	{
		depthMax = texture(i_Depth_MaxReduction, uv).x;
	}
	else
	{
		vec4 texels;
		texels.x = texture(i_Depth_MaxReduction, uv).y;
		texels.y = textureOffset(i_Depth_MaxReduction, uv, ivec2(-1, 0)).y;
		texels.z = textureOffset(i_Depth_MaxReduction, uv, ivec2(-1,-1)).y;
		texels.w = textureOffset(i_Depth_MaxReduction, uv, ivec2( 0,-1)).y;
 
		depthMax = max(max(texels.x, texels.y), max(texels.z, texels.w));

		vec3 extra;
		// if we are reducing an odd-width texture then fetch the edge texels
		if (((lastMipSize.x & 1) != 0) && (pos.x == lastMipSize.x-3))
		{
			// if both edges are odd, fetch the top-left corner texel
			if (((lastMipSize.y & 1) != 0) && (pos.y == lastMipSize.y-3))
			{
				extra.z = textureOffset(i_Depth_MaxReduction, uv, ivec2(1, 1)).y;
				depthMax = max(depthMax, extra.z);
			}
			extra.x = textureOffset(i_Depth_MaxReduction, uv, ivec2(1,  0)).y;
			extra.y = textureOffset(i_Depth_MaxReduction, uv, ivec2(1, -1)).y;
			depthMax = max(depthMax, max(extra.x, extra.y));
		} 

		// if we are reducing an odd-height texture then fetch the edge texels
		else if (((lastMipSize.y & 1) != 0) && (pos.y == lastMipSize.y-3))
		{
			extra.x = textureOffset(i_Depth_MaxReduction, uv, ivec2( 0, 1)).y;
			extra.y = textureOffset(i_Depth_MaxReduction, uv, ivec2(-1, 1)).y;
			depthMax = max(depthMax, max(extra.x, extra.y));
		}
	}
	*/

	//depths = textureGather(i_Depth_MaxReduction, (vec2(pos) + vec2(0.5)) / u_PushConstant.u_ImageSize, 0);

	
	

#endif
	

#if 0
	// Method 3:
	// Texture offset approach

	//if(u_PushConstant.MipLevel == 0.0f)
	//{
	//	float depth = texelFetch(i_Depth, pos, 0).r;
	//	imageStore(o_Depth, ivec2(pos), vec4(depth, depth, 0.0f, 0.0f));
	//	return;
	//}

	//float depth1 = textureOffset(i_Depth, (vec2(pos) + vec2(0.5f)) / u_PushConstant.u_ImageSize, ivec2(-1,  0)).x; // left
	//float depth2 = textureOffset(i_Depth, (vec2(pos) + vec2(0.5f)) / u_PushConstant.u_ImageSize, ivec2( 1,  0)).x; // right
	//float depth3 = textureOffset(i_Depth, (vec2(pos) + vec2(0.5f)) / u_PushConstant.u_ImageSize, ivec2( 0,  1)).x; // up
	//float depth4 = textureOffset(i_Depth, (vec2(pos) + vec2(0.5f)) / u_PushConstant.u_ImageSize, ivec2( 0, -1)).x; // down

	//float depthMin = min(min(depth1, depth2), min(depth3, depth4));
	//float depthMax = max(max(depth1, depth2), max(depth3, depth4));

	vec2 uv = ((vec2(pos)) / vec2(imageSize(o_Depth)));
	vec4 depth = textureGather(i_Depth, uv, 0);
	
	float depthMin = min(min(depth.x, depth.y), min(depth.z, depth.w));
	float depthMax = max(max(depth.x, depth.y), max(depth.z, depth.w));
#endif

	imageStore(o_Depth, ivec2(pos), vec4(depthMin, depthMax, 0.0, 0.0));
}