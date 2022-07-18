#type compute
#version 460

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

#define PI 3.141592654

layout(binding = 0) uniform sampler3D u_ScatExtinctionFroxel;
layout(binding = 1) uniform sampler3D u_EmissionPhaseFroxel;
layout(binding = 2) uniform sampler2D u_ShadowDepthTexture;
layout(binding = 3) uniform sampler2D u_TemporalBlueNoiseLUT;

layout(binding = 4, rgba16f) restrict writeonly uniform image3D u_ScatExtinctionFroxel_Output;

layout(binding = 5) uniform DirectionaLightData
{
	mat4 LightViewProjMatrix0;
	mat4 LightViewProjMatrix1;
	mat4 LightViewProjMatrix2;
	mat4 LightViewProjMatrix3;

	float CascadeDepthSplit[4];
} u_DirLightData;


layout(push_constant) uniform PushConstant
{
	mat4 InvViewProjMatrix;
	vec3 CameraPosition;

	float Time;

	vec3 DirectionalLightDir;
} u_PushConstant;


// ---------------------- CASCADED SHADOW MAPS ------------------------
vec2 ComputeShadowCoord(vec2 coord, uint cascadeIndex)
{
	switch(cascadeIndex)
	{
	case 1:
		return coord * vec2(0.5f);
	case 2:
		return vec2(coord.x * 0.5f + 0.5f, coord.y * 0.5f);
	case 3:
		return vec2(coord.x * 0.5f, coord.y * 0.5f + 0.5f);
	case 4:
		return coord * vec2(0.5f) + vec2(0.5f);
	}
}

uint GetCascadeIndex(float viewPosZ)
{
#define SHADOW_MAP_CASCADE_COUNT 4

	uint cascadeIndex = 1;
	for(uint i = 0; i < SHADOW_MAP_CASCADE_COUNT - 1; ++i) 
	{
		if(viewPosZ < u_DirLightData.CascadeDepthSplit[i])
		{	
			cascadeIndex = i + 1;
		}
	}
	return cascadeIndex;
}

mat4 GetCascadeMatrix(uint cascadeIndex)
{
	switch(cascadeIndex)
	{
		case 1: return u_DirLightData.LightViewProjMatrix0;
		case 2: return u_DirLightData.LightViewProjMatrix1;
		case 3: return u_DirLightData.LightViewProjMatrix2;
		case 4: return u_DirLightData.LightViewProjMatrix3;
	}
}

float SampleShadowMap(vec2 shadowCoords, uint cascadeIndex)
{
	vec2 coords = ComputeShadowCoord(shadowCoords.xy, cascadeIndex);
	float dist = texture(u_ShadowDepthTexture, coords).r;
	return dist;
}

float SampleShadowTexture(vec4 shadowCoord, uint cascadeIndex)
{
	float bias = 0.005;

	if (shadowCoord.z > -1.0 && shadowCoord.z < 1.0 )
	{
		float dist = SampleShadowMap(shadowCoord.xy * 0.5 + 0.5, cascadeIndex);

		if (shadowCoord.w > 0 && dist < shadowCoord.z - bias)
		{
			return 0.1f;
		}
	}
	return 1.0f;
}
// ---------------------------------------------------------

float GetMiePhase(float cosTheta, float g)
{
	const float scale = 3.0 / (8.0 * PI);
	
	float num = (1.0 - g * g) * (1.0 + cosTheta * cosTheta);
	float denom = (2.0 + g * g) * pow((1.0 + g * g - 2.0 * g * cosTheta), 1.5);
	
	return scale * num / denom;
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
	ivec3 numFroxels = ivec3(imageSize(u_ScatExtinctionFroxel_Output).xyz);

	if(any(greaterThanEqual(invoke, numFroxels)))
		return;

	vec3 noise = (SampleNoise(invoke.xy).rgb * 2.0 - vec3(1.0)) / vec3(numFroxels);
	vec2 uv = (vec2(invoke.xy) + vec2(0.5)) / vec2(numFroxels.xy) + noise.xy;

	// Compute the world space position of the froxel's texel
	vec4 worldSpaceMax = u_PushConstant.InvViewProjMatrix * vec4(uv * 2.0 - vec2(1.0), 1.0, 1.0);
	worldSpaceMax /= worldSpaceMax.w;
	vec3 direction = worldSpaceMax.xyz - u_PushConstant.CameraPosition;
	float w = (float(invoke.z) + 0.5) / float(numFroxels.z) + noise.z;
	vec3 worldSpacePosition = u_PushConstant.CameraPosition + direction * w * w;

	// Sampling the froxels
	vec4 scattExtinction = texelFetch(u_ScatExtinctionFroxel, invoke, 0);
	vec4 emissionPhase = texelFetch(u_EmissionPhaseFroxel, invoke, 0);

	// Compute the light integral
	float phaseFunction = GetMiePhase(
		dot(normalize(direction), u_PushConstant.DirectionalLightDir),
		emissionPhase.w
	);

	vec3 mieScattering = scattExtinction.xyz;
	vec3 extinction = vec3(scattExtinction.w);
	vec3 voxelAlbedo = mieScattering / extinction;

	// TODO: Add point light contribution
	uint cascadeIndex = GetCascadeIndex(worldSpacePosition.z);
	mat4 cascadeMat = GetCascadeMatrix(cascadeIndex);
	vec4 shadowCoord = cascadeMat * vec4(worldSpacePosition, 1.0f);
	float visibility = SampleShadowTexture(shadowCoord, cascadeIndex);

	vec3 light = max(voxelAlbedo * phaseFunction * visibility * 5.0f, vec3(0.0f)) + emissionPhase.rgb;

	imageStore(u_ScatExtinctionFroxel_Output, invoke, vec4(light, scattExtinction.w));
}