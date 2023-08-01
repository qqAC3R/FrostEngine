/*
	Sources:
		https://github.com/nuclearkevin/Strontium
		https://www.ea.com/frostbite/news/physically-based-unified-volumetric-rendering-in-frostbite
		https://www.youtube.com/watch?v=2XXS5UyNjjU
*/
#type compute
#version 460

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

// Unroll all loops for performance - this is important
#pragma optionNV(unroll all)

layout(binding = 0) uniform sampler3D u_CurrentVolumetricFroxel;
layout(binding = 1) uniform sampler3D u_PreviousVolumetricFroxel;

layout(binding = 2) writeonly uniform image3D u_ResolveVolumetricFroxel; // The same froxel as `u_CurrentVolumetricFroxel`, we don't more because we are only reading and writting at one texel per wrap (gpu core)

layout(push_constant) uniform PushConstant
{
	mat4 InvViewProjMatrix;
	mat4 PreviousViewProjMatrix;
	mat4 PreviousInvViewProjMatrix;

	vec3 CameraPosition;
	float Padding0;
	vec3 PreviousCameraPosition;

} u_PushConstant;

bool InFrustum(vec3 ndc)
{
	bool xAxis = ndc.x >= -1.0 && ndc.x <= 1.0;
	bool yAxis = ndc.y >= -1.0 && ndc.y <= 1.0;
	bool zAxis = ndc.z >= -1.0 && ndc.z <= 1.0;
	
	return xAxis && yAxis && zAxis;
}

vec4 SpatialFilter(vec3 coord)
{
	vec3 texel = 1.0.xxx / vec3(textureSize(u_CurrentVolumetricFroxel, 0).xyz);
	
	float weight;
	vec3 offset;
	vec4 accum = 0.0.xxxx;
	float totalWeight = 0.0;
	for (int i = -1; i <= 1; i++)
	{
		for (int j = -1; j <= 1; j++)
		{
			for (int k = -1; k <= 1; k++)
			{
				offset = texel * vec3(i, j, k);
				weight = exp(-2.29 * dot(coord - offset, coord - offset));
				totalWeight += weight;
				accum += weight * texture(u_CurrentVolumetricFroxel, coord + offset);
			}
		}
	}
	
	return max(accum / totalWeight, 0.0.xxxx);
}

void main()
{
	ivec3 invoke = ivec3(gl_GlobalInvocationID.xyz);
	ivec3 numFroxels = ivec3(imageSize(u_ResolveVolumetricFroxel).xyz);

	if (any(greaterThanEqual(invoke, numFroxels)))
		return;

	vec3 texel = vec3(1.0) / vec3(numFroxels.xyz);
	vec2 uv = (vec2(invoke.xy) + vec2(0.5)) * texel.xy;
	float w = (float(invoke.z) + 0.5) * texel.z;

	vec3 uvw = vec3(uv, w);

	// Box blur + Computing the min/max of the neighboring pixels for clamping the history buffer texel color
	//vec4 currentResult = SpatialFilter(uvw);
	vec4 currentResult = vec4(0.0);
	vec4 pixelMinClamp = vec4(1.0f);
	vec4 pixelMaxClamp = vec4(0.0f);
	vec4 neighbouringTexels[8];
	neighbouringTexels[0] = texture(u_CurrentVolumetricFroxel, uvw + vec3(-0.5, -0.5, -0.5) * texel);
	neighbouringTexels[1] = texture(u_CurrentVolumetricFroxel, uvw + vec3( 0.5, -0.5, -0.5) * texel);
	neighbouringTexels[2] = texture(u_CurrentVolumetricFroxel, uvw + vec3(-0.5,  0.5, -0.5) * texel);
	neighbouringTexels[3] = texture(u_CurrentVolumetricFroxel, uvw + vec3( 0.5,  0.5, -0.5) * texel);
	neighbouringTexels[4] = texture(u_CurrentVolumetricFroxel, uvw + vec3(-0.5, -0.5,  0.5) * texel);
	neighbouringTexels[5] = texture(u_CurrentVolumetricFroxel, uvw + vec3( 0.5, -0.5,  0.5) * texel);
	neighbouringTexels[6] = texture(u_CurrentVolumetricFroxel, uvw + vec3(-0.5,  0.5,  0.5) * texel);
	neighbouringTexels[7] = texture(u_CurrentVolumetricFroxel, uvw + vec3( 0.5,  0.5,  0.5) * texel);

	for(uint i = 0; i < 8; i++)
	{
		currentResult += neighbouringTexels[i];

		pixelMinClamp = min(pixelMinClamp, neighbouringTexels[i]);
		pixelMaxClamp = max(pixelMaxClamp, neighbouringTexels[i]);
	}
	currentResult /= 8.0;

	// Temporal AA
	// Get current texel's world space position for it to be converted into
	// the previous frame's position for sampling and mixing
	vec4 worldSpaceMax = u_PushConstant.InvViewProjMatrix * vec4(uv * 2.0 - vec2(1.0), 1.0, 1.0);
	worldSpaceMax /= worldSpaceMax.w;
	vec3 direction = worldSpaceMax.xyz - u_PushConstant.CameraPosition;
	vec3 worldSpacePosition = u_PushConstant.CameraPosition + direction * w * w;

	// Project the current world space position into the previous frame
	worldSpaceMax = u_PushConstant.PreviousViewProjMatrix * vec4(worldSpacePosition, 1.0);
	vec3 previousPosNDC = (worldSpaceMax.xyz / worldSpaceMax.w);
	vec2 previousUV = 0.5 * previousPosNDC.xy + 0.5.xx;

	// Also, compute the z component
	worldSpaceMax = u_PushConstant.PreviousInvViewProjMatrix * vec4(previousUV * 2.0 - vec2(1.0), 1.0, 1.0);;
	vec3 previousWorldSpaceMax = (worldSpaceMax.xyz / worldSpaceMax.w);
	float previousFarPlaneLength = length(previousWorldSpaceMax - u_PushConstant.PreviousCameraPosition);
	float previousZ = length(worldSpacePosition - u_PushConstant.PreviousCameraPosition);
	previousZ /= previousFarPlaneLength;
	previousZ = pow(previousZ, 1.0 / 2.0);


	vec4 lastFrameResult = texture(u_PreviousVolumetricFroxel, vec3(previousUV, previousZ));

	//// Sampling the history buffer
	//vec4 lastFrameResult = texture(u_PreviousVolumetricFroxel, vec3(previousUV, previousZ));
	//
	// Mixing it up a bit more for the edges to not flicker so much
	lastFrameResult = mix(clamp(lastFrameResult, pixelMinClamp, pixelMaxClamp), lastFrameResult, 0.9);

	// Compute the final result
	vec4 finalResult;
	if(InFrustum(previousPosNDC))
		finalResult = mix(lastFrameResult, currentResult, 0.02);
	else
		finalResult = currentResult;
		
	imageStore(u_ResolveVolumetricFroxel, invoke, finalResult);
}