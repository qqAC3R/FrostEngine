#type compute
#version 460

/*
	Sources:
		https://github.com/nuclearkevin/Strontium
		https://www.ea.com/frostbite/news/physically-based-unified-volumetric-rendering-in-frostbite
*/

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(binding = 0, rgba16f) restrict writeonly uniform image3D u_ScatExtinctionFroxel;
layout(binding = 1, rgba16f) restrict writeonly uniform image3D u_EmissionPhaseFroxel;
layout(binding = 3) uniform sampler2D u_TemporalBlueNoiseLUT;

struct FogVolumeParams
{
	mat4 InvTransformMatrix; // Inverse model-space transform matrix.
	vec4 MieScatteringPhase; // Mie scattering (x, y, z) and phase value (w).
	vec4 EmissionAbsorption; // Emission (x, y, z) and absorption (w).
};

layout(binding = 4) readonly buffer FogVolumeData
{
	FogVolumeParams Volumes[];
} u_FogVolumeData;

layout(push_constant) uniform PushConstant
{
	mat4 InvViewProjMatrix;
	vec3 CameraPosition;

	float FogVolumesCount;
	float Time;
} u_PushConstant;

bool PointInOBB(vec3 point, FogVolumeParams volume)
{
	vec3 transformPoint = vec3(volume.InvTransformMatrix * vec4(point, 1.0f));

	bool xAxis = abs(transformPoint.x) <= 1.0f;
	bool yAxis = abs(transformPoint.y) <= 1.0f;
	bool zAxis = abs(transformPoint.z) <= 1.0f;

	return xAxis && yAxis && zAxis;
}

// Sample a dithering function.
vec4 SampleNoise(ivec2 coords)
{
	vec4 temporal = fract((vec4(u_PushConstant.Time) + vec4(0.0, 1.0, 2.0, 3.0)) * 0.61803399);
	vec2 uv = (vec2(coords) + 0.5.xx) / vec2(textureSize(u_TemporalBlueNoiseLUT, 0).xy);
	return fract(texture(u_TemporalBlueNoiseLUT, uv) + temporal);
}

void main()
{
	ivec3 invoke = ivec3(gl_GlobalInvocationID.xyz);
	ivec3 numFroxels = ivec3(imageSize(u_ScatExtinctionFroxel).xyz);

	if (any(greaterThanEqual(invoke, numFroxels)))
		return;

	// Sample temporal blue noise
	vec3 noise = (2.0 * SampleNoise(invoke.xy).rgb - vec3(1.0)) / vec3(numFroxels) * 0.35f;

	// Center UV of the current texel
	vec2 centerUV;
	centerUV = (vec2(invoke.xy) + vec2(0.5f)) / vec2(numFroxels.xy) + noise.xy;


	// Computing the center of the current texel (in world space)
	vec3 worldSpacePosCenter;
	vec4 worldSpaceMax;
	vec3 direction;
	float w;

	// Getting the max value of the current UV (the point on the far plane on the frustum)
	worldSpaceMax = u_PushConstant.InvViewProjMatrix * vec4(2.0f * centerUV - vec2(1.0f), 1.0f, 1.0f);
	worldSpaceMax /= worldSpaceMax.w;

	// Computing the world space position of the texel's center
	direction = worldSpaceMax.xyz - u_PushConstant.CameraPosition;

	w = (float(invoke.z) + 0.5f) / float(numFroxels.z) + noise.z;
	worldSpacePosCenter = u_PushConstant.CameraPosition + direction * w * w;

	// Compute the contribuition of all volumes for this texel
	vec4 scatExtParams = vec4(0.0f);
	vec4 emissionPhaseParams = vec4(0.0f);

	float numVolumesInPixel = 0.0;
	float extinction;
	FogVolumeParams volumeParams;
	for(uint i = 0; i < u_PushConstant.FogVolumesCount; i++)
	{
		volumeParams = u_FogVolumeData.Volumes[i];

		if(PointInOBB(worldSpacePosCenter, volumeParams))
		{
			extinction = dot(volumeParams.MieScatteringPhase.xyz, vec3(1.0 / 3.0)) + volumeParams.EmissionAbsorption.w;

			scatExtParams       += vec4(volumeParams.MieScatteringPhase.xyz, extinction);
			emissionPhaseParams += vec4(volumeParams.EmissionAbsorption.xyz, volumeParams.MieScatteringPhase.w);

			numVolumesInPixel += 1.0f;
		}
	}

	// Average phase.
	emissionPhaseParams.w /= max(numVolumesInPixel, 1.0f);

	// Storing the results
	imageStore(u_ScatExtinctionFroxel, invoke, scatExtParams);
	imageStore(u_EmissionPhaseFroxel,  invoke, emissionPhaseParams);
}