#type compute
#version 460 

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

#define SHADOW_MAP_CASCADE_COUNT 4

layout(binding = 0) uniform sampler2D u_ShadowDepthTexture;
layout(binding = 1) uniform sampler2D u_PositionTexture;
layout(binding = 2) uniform sampler2D u_ViewPositionTexture;
layout(binding = 5) uniform sampler2D u_NormalMap;
layout(binding = 3, rgba8) uniform writeonly image2D u_ShadowTextureOutput;

layout(push_constant) uniform PushConstant
{
	float CascadeDepthSplit[SHADOW_MAP_CASCADE_COUNT];
} u_PushConstant;

layout(binding = 4) uniform DirectionaLightData
{
	mat4 LightViewProjMatrix0;
	mat4 LightViewProjMatrix1;
	mat4 LightViewProjMatrix2;
	mat4 LightViewProjMatrix3;

	vec3 DirectionalLightDir;
	float DirectionLightSize;
	
	int FadeCascades;
	float CascadesFadeFactor;

	int CascadeDebug;

	int UsePCSS;
} u_DirLightData;

vec2 s_UV;

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

float SampleShadowMap(vec2 shadowCoords, uint cascadeIndex)
{
	vec2 coords = ComputeShadowCoord(shadowCoords.xy, cascadeIndex);
	float dist = texture(u_ShadowDepthTexture, coords).r;
	return dist;
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

uint GetCascadeIndex(vec2 uv)
{
	// Get cascade index for the current fragment's view position
	float viewPosZ = texture(u_ViewPositionTexture, uv).z;

	uint cascadeIndex = 1;
	for(uint i = 0; i < SHADOW_MAP_CASCADE_COUNT - 1; ++i) {
		if(viewPosZ < u_PushConstant.CascadeDepthSplit[i]) {	
			cascadeIndex = i + 1;
		}
	}
	return cascadeIndex;
}

// Fast octahedron normal vector decoding.
// https://jcgt.org/published/0003/02/01/
vec2 SignNotZero(vec2 v)
{
	return vec2((v.x >= 0.0) ? 1.0 : -1.0, (v.y >= 0.0) ? 1.0 : -1.0);
}
vec3 DecodeNormal(vec2 e)
{
	vec3 v = vec3(e.xy, 1.0 - abs(e.x) - abs(e.y));
	if (v.z < 0)
		v.xy = (1.0 - abs(v.yx)) * SignNotZero(v.xy);
	return normalize(v);
}

float GetShadowBias()
{
	const float MINIMUM_SHADOW_BIAS = 0.005;

	vec3 normal = DecodeNormal(texture(u_NormalMap, s_UV).xy);
	float bias = max(
		MINIMUM_SHADOW_BIAS * (1.0 - dot(normal, u_DirLightData.DirectionalLightDir)),
		MINIMUM_SHADOW_BIAS
	);
	
	return bias;
}

float HardShadows_SampleShadowTexture(vec4 shadowCoord, uint cascadeIndex)
{
	float shadow = 1.0;
	//float bias = GetShadowBias();
	float bias = 0.05;

	if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 )
	{
		float dist = SampleShadowMap(shadowCoord.xy * 0.5 + 0.5, cascadeIndex);

		if (shadowCoord.w > 0 && dist < shadowCoord.z - bias)
		{
			shadow = 0.3f;
		}
	}
	return shadow;
}


const vec2 PoissonDistribution[64] = vec2[](
	vec2(-0.884081, 0.124488),
	vec2(-0.714377, 0.027940),
	vec2(-0.747945, 0.227922),
	vec2(-0.939609, 0.243634),
	vec2(-0.985465, 0.045534),
	vec2(-0.861367, -0.136222),
	vec2(-0.881934, 0.396908),
	vec2(-0.466938, 0.014526),
	vec2(-0.558207, 0.212662),
	vec2(-0.578447, -0.095822),
	vec2(-0.740266, -0.095631),
	vec2(-0.751681, 0.472604),
	vec2(-0.553147, -0.243177),
	vec2(-0.674762, -0.330730),
	vec2(-0.402765, -0.122087),
	vec2(-0.319776, -0.312166),
	vec2(-0.413923, -0.439757),
	vec2(-0.979153, -0.201245),
	vec2(-0.865579, -0.288695),
	vec2(-0.243704, -0.186378),
	vec2(-0.294920, -0.055748),
	vec2(-0.604452, -0.544251),
	vec2(-0.418056, -0.587679),
	vec2(-0.549156, -0.415877),
	vec2(-0.238080, -0.611761),
	vec2(-0.267004, -0.459702),
	vec2(-0.100006, -0.229116),
	vec2(-0.101928, -0.380382),
	vec2(-0.681467, -0.700773),
	vec2(-0.763488, -0.543386),
	vec2(-0.549030, -0.750749),
	vec2(-0.809045, -0.408738),
	vec2(-0.388134, -0.773448),
	vec2(-0.429392, -0.894892),
	vec2(-0.131597, 0.065058),
	vec2(-0.275002, 0.102922),
	vec2(-0.106117, -0.068327),
	vec2(-0.294586, -0.891515),
	vec2(-0.629418, 0.379387),
	vec2(-0.407257, 0.339748),
	vec2(0.071650, -0.384284),
	vec2(0.022018, -0.263793),
	vec2(0.003879, -0.136073),
	vec2(-0.137533, -0.767844),
	vec2(-0.050874, -0.906068),
	vec2(0.114133, -0.070053),
	vec2(0.163314, -0.217231),
	vec2(-0.100262, -0.587992),
	vec2(-0.004942, 0.125368),
	vec2(0.035302, -0.619310),
	vec2(0.195646, -0.459022),
	vec2(0.303969, -0.346362),
	vec2(-0.678118, 0.685099),
	vec2(-0.628418, 0.507978),
	vec2(-0.508473, 0.458753),
	vec2(0.032134, -0.782030),
	vec2(0.122595, 0.280353),
	vec2(-0.043643, 0.312119),
	vec2(0.132993, 0.085170),
	vec2(-0.192106, 0.285848),
	vec2(0.183621, -0.713242),
	vec2(0.265220, -0.596716),
	vec2(-0.009628, -0.483058),
	vec2(-0.018516, 0.435703)
);

vec2 SamplePoisson(int index)
{
	return PoissonDistribution[index % 64];
}

float SearchRegionRadiusUV(float zWorld)
{
	const float light_zNear = 0.0;
	const float lightRadiusUV = 0.05;
	return lightRadiusUV * (zWorld - light_zNear) / zWorld;
}

float FindBlockerDistance_DirectionalLight(vec4 shadowCoords, uint cascadeIndex, float lightSize)
{
	float bias = GetShadowBias();
	//float bias = 0.05;

	int numBlockerSearchSamples = 32;
	int blockers = 0;
	float avgBlockerDistance = 0;

	float searchWidth = SearchRegionRadiusUV(shadowCoords.z);
	for (int i = 0; i < numBlockerSearchSamples; i++)
	{
		float z = SampleShadowMap((shadowCoords.xy * 0.5 + 0.5) + SamplePoisson(i) * searchWidth, cascadeIndex);

		if (z < (shadowCoords.z - bias))
		{
			blockers++;
			avgBlockerDistance += z;
		}
	}

	if (blockers > 0)
		return avgBlockerDistance / float(blockers);

	return -1;
}

float PCF_DirectionalLight(vec4 shadowCoords, uint cascadeIndex, float uvRadius)
{
	float bias = GetShadowBias();
	//float bias = 0.05;
	int numPCFSamples = 32;

	float sum = 0;
	for (int i = 0; i < numPCFSamples; i++)
	{
		vec2 offset = SamplePoisson(i) * uvRadius;
		float z = SampleShadowMap((shadowCoords.xy * 0.5 + 0.5) + offset, cascadeIndex);
		sum += step(shadowCoords.z - bias, z);
	}
	return sum / float(numPCFSamples);
}

float PCSS_SampleShadowTexture(vec4 shadowCoords, uint cascadeIndex, float lightSize)
{
	float blockerDistance = FindBlockerDistance_DirectionalLight(shadowCoords, cascadeIndex, lightSize);

	if(blockerDistance == -1) return 1.0f; // No occlusion

	float penumbraWidth = (shadowCoords.z - blockerDistance) / blockerDistance;
	float NEAR = 0.01; // Should this value be tweakable?
	float uvRadius = penumbraWidth * lightSize * NEAR / shadowCoords.z; // Do we need to divide by shadowCoords.z?

	uvRadius = min(uvRadius, 0.002f);
	return PCF_DirectionalLight(shadowCoords, cascadeIndex, uvRadius);// * ShadowFade;
}

void main()
{
	ivec2 globalInvocation = ivec2(gl_GlobalInvocationID.xy);
	ivec2 imgSize = imageSize(u_ShadowTextureOutput).xy;
	vec2 uv = (vec2(globalInvocation) + 0.5.xx) / vec2(imgSize);
	s_UV = uv;

	if (any(greaterThanEqual(globalInvocation, imgSize)))
		return;

	uint cascadeIndex = GetCascadeIndex(uv);
	vec3 position = texture(u_PositionTexture, uv).rgb;

	float lightSize = u_DirLightData.DirectionLightSize;

	// Depth compare for shadowing
	vec4 shadowCoord = (GetCascadeMatrix(cascadeIndex)) * vec4(position, 1.0);
	shadowCoord /= shadowCoord.w;

	float shadowFactor = 0.0f;
	if(u_DirLightData.FadeCascades == 1)
	{
		float viewPosZ = texture(u_ViewPositionTexture, uv).z;
		float cascadeTransitionFade = u_DirLightData.CascadesFadeFactor;

		//float c0 = smoothstep(u_PushConstant.CascadeDepthSplit[0] + cascadeTransitionFade * 0.5f, u_PushConstant.CascadeDepthSplit[0] - cascadeTransitionFade * 0.5f, viewPosZ);
		float c1 = smoothstep(u_PushConstant.CascadeDepthSplit[1] + cascadeTransitionFade * 0.5f, u_PushConstant.CascadeDepthSplit[1] - cascadeTransitionFade * 0.5f, viewPosZ);
		float c2 = smoothstep(u_PushConstant.CascadeDepthSplit[2] + cascadeTransitionFade * 0.5f, u_PushConstant.CascadeDepthSplit[2] - cascadeTransitionFade * 0.5f, viewPosZ);

		//if (c0 > 0.0 && c0 < 1.0)
		//{
		//	vec4 shadowCoord0 = (GetCascadeMatrix(0)) * vec4(position, 1.0); shadowCoord0 /= shadowCoord.w;
		//	vec4 shadowCoord1 = (GetCascadeMatrix(1)) * vec4(position, 1.0); shadowCoord1 /= shadowCoord.w;
		//
		//	float shadowFactor0 = PCSS_SampleShadowTexture(shadowCoord0, 0, lightSize);
		//	float shadowFactor1 = PCSS_SampleShadowTexture(shadowCoord1, 1, lightSize);
		//
		//	shadowFactor = mix(shadowFactor0, shadowFactor1, c0);
		//}
		if(c1 > 0.0 && c1 < 1.0)
		{
			vec4 shadowCoord1 = (GetCascadeMatrix(1)) * vec4(position, 1.0); shadowCoord1 /= shadowCoord.w;
			vec4 shadowCoord2 = (GetCascadeMatrix(2)) * vec4(position, 1.0); shadowCoord2 /= shadowCoord.w;

			float shadowFactor1 = u_DirLightData.UsePCSS == 1 ? PCSS_SampleShadowTexture(shadowCoord1, 1, lightSize) : HardShadows_SampleShadowTexture(shadowCoord1, 1);
			float shadowFactor2 = u_DirLightData.UsePCSS == 1 ? PCSS_SampleShadowTexture(shadowCoord2, 2, lightSize) : HardShadows_SampleShadowTexture(shadowCoord2, 2);

			shadowFactor = mix(shadowFactor1, shadowFactor2, c1);
		}
		else if (c2 > 0.0 && c2 < 1.0)
		{
			vec4 shadowCoord2 = (GetCascadeMatrix(2)) * vec4(position, 1.0); shadowCoord2 /= shadowCoord.w;
			vec4 shadowCoord3 = (GetCascadeMatrix(3)) * vec4(position, 1.0); shadowCoord3 /= shadowCoord.w;

			float shadowFactor2 = u_DirLightData.UsePCSS == 1 ? PCSS_SampleShadowTexture(shadowCoord2, 2, lightSize) : HardShadows_SampleShadowTexture(shadowCoord2, 2);
			float shadowFactor3 = u_DirLightData.UsePCSS == 1 ? PCSS_SampleShadowTexture(shadowCoord3, 3, lightSize) : HardShadows_SampleShadowTexture(shadowCoord3, 3);

			shadowFactor = mix(shadowFactor2, shadowFactor3, c2);
		}
		else
		{
			shadowFactor = u_DirLightData.UsePCSS == 1 ? PCSS_SampleShadowTexture(shadowCoord, cascadeIndex, lightSize) : HardShadows_SampleShadowTexture(shadowCoord, cascadeIndex);
		}
	}
	else
	{
		shadowFactor = u_DirLightData.UsePCSS == 1 ? PCSS_SampleShadowTexture(shadowCoord, cascadeIndex, lightSize) : HardShadows_SampleShadowTexture(shadowCoord, cascadeIndex);
	}

	

	vec3 shadow = vec3(clamp(shadowFactor, 0.3f, 1.0f));


	if(u_DirLightData.CascadeDebug == 1)
	{
		switch(cascadeIndex)
		{
			case 1: shadow *= vec3(1.0f, 0.25f, 0.25f); break;
			case 2: shadow *= vec3(0.25f, 1.0f, 0.25f); break;
			case 3: shadow *= vec3(0.25f, 0.25f, 1.0f); break;
			case 4: shadow *= vec3(1.0f, 1.0f, 0.25f);  break;
		}
	}

	imageStore(u_ShadowTextureOutput, globalInvocation, vec4(vec3(shadow), 1.0f));
}