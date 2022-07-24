#type compute
#version 460

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(binding = 0) uniform sampler3D u_LightExtinctionFroxel;

layout(binding = 1, rgba16f) restrict writeonly uniform image3D u_FinalLightFroxel;

// Unroll all loops for performance - this is important
#pragma optionNV(unroll all)

layout(push_constant) uniform PushConstant
{
	mat4 InvViewProjMatrix;
	vec3 CameraPosition;

	float Padding0;

	vec3 DirectionalLightDir;
} u_PushConstant;

void main()
{
	ivec2 invoke = ivec2(gl_GlobalInvocationID.xy);
	ivec3 numFroxels = ivec3(imageSize(u_FinalLightFroxel).xyz);

	if (any(greaterThanEqual(invoke, numFroxels.xy)))
		return;

	vec2 uvs = (vec2(invoke.xy) + vec2(0.5)) / vec2(numFroxels.xy);

	vec4 worldSpaceMax = u_PushConstant.InvViewProjMatrix * vec4(uvs * 2.0 - vec2(1.0), 1.0, 1.0);
	worldSpaceMax /= worldSpaceMax.w;
	vec3 direction = worldSpaceMax.xyz - u_PushConstant.CameraPosition;

	// Loop over the z dimension and integrate the in-scattered lighting and transmittance.
	float transmittance = 1.0;
	vec3 luminance = vec3(0.0);

	// For loop temporal variables to not be allocated every time
	vec4 lightExtinction;
	float w;
	vec3 worldSpacePosition;
	vec3 previousPosition = u_PushConstant.CameraPosition;
	float dt;
	float sampleTransmittance;
	vec3 scatteringIntegral;

	for (uint i = 0; i < numFroxels.z; i++)
	{
		lightExtinction = texelFetch(u_LightExtinctionFroxel, ivec3(invoke, i), 0);

		if(lightExtinction.w < 1e-4)
		{
			w = (float(i) + 0.5) / float(numFroxels.z);
			worldSpacePosition = u_PushConstant.CameraPosition + normalize(direction) * length(direction) * w * w;
			previousPosition = worldSpacePosition;

			luminance += lightExtinction.xyz;
			imageStore(u_FinalLightFroxel, ivec3(invoke, i), vec4(luminance, transmittance));
			continue;
		}

		w = (float(i) + 0.5) / float(numFroxels.z);
		worldSpacePosition = u_PushConstant.CameraPosition + direction * w * w;

		dt = length(worldSpacePosition - previousPosition);
		sampleTransmittance = exp(-dt * lightExtinction.w);
		scatteringIntegral = (lightExtinction.rgb - lightExtinction.rgb * sampleTransmittance) / lightExtinction.w;

		luminance += max(scatteringIntegral * transmittance, 0.0);
		transmittance *= min(sampleTransmittance, 1.0);

		previousPosition = worldSpacePosition;

		imageStore(u_FinalLightFroxel, ivec3(invoke, i), vec4(luminance, transmittance));
	}
}