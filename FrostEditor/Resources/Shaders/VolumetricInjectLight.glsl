#type compute
#version 460
#extension GL_EXT_scalar_block_layout : enable

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

#define PI 3.141592654

const float LtcLutTextureSize = 64.0; // ltc_texture size
const float LtcLutTextureScale = (LtcLutTextureSize - 1.0) / LtcLutTextureSize;
const float LtcLutTextureBias  = 0.5 / LtcLutTextureSize;


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
	
	vec3 MieScattering;
	float Density;
	float Absorption;
	float Phase;
	float Intensity;
} u_DirLightData;


struct PointLight
{
    vec3 Radiance;
	float Intensity;
    float Radius;
    float Falloff;
    vec3 Position;
};

struct RectangularLight
{
	vec3 Radiance;
	float Intensity;
	vec4 Vertex0; // W component stores the TwoSided bool
	vec4 Vertex1; // W component stores the Radius for it to be culled
	vec4 Vertex2;
	vec4 Vertex3;
};

// Point lights
layout(binding = 6, scalar) readonly buffer PointLightData {
	PointLight PointLights[];
} u_PointLightData;

layout(binding = 7) readonly buffer VisiblePointLightData {
	int Indices[];
} u_VisiblePointLightData;

// Rectangular lights
layout(binding = 8) uniform sampler2D u_LTC2Lut;

layout(binding = 9) readonly buffer RectangularLightData {
	RectangularLight RectLights[];
} u_RectLightData;

layout(binding = 10) readonly buffer VisibleRectLightData {
	int Indices[];
} u_VisibleRectLightData;


layout(push_constant) uniform PushConstant
{
	mat4 InvViewProjMatrix;
	vec3 CameraPosition;

	float Time;

	vec3 DirectionalLightDir;

	float LightCullingWorkgroupX;

	vec2 ViewportSize;
	float PointLightCount;
	float RectangularLightCount;
} u_PushConstant;


// --------------------- Tiled Point Light Culling --------------------------
int GetPointLightBufferIndex(int i, vec2 uv)
{
	vec2 coord = vec2(u_PushConstant.ViewportSize * uv);
    ivec2 tileID = ivec2(coord) / ivec2(16, 16); //Current Fragment position / Tile count
    uint index = tileID.y * uint(u_PushConstant.LightCullingWorkgroupX) + tileID.x;

    uint offset = index * 1024;
    return u_VisiblePointLightData.Indices[offset + i];
}
// --------------------- Tiled Rectangular Light Culling --------------------------
int GetRectangularLightBufferIndex(int i, vec2 uv)
{
	vec2 coord = vec2(u_PushConstant.ViewportSize * uv);
    ivec2 tileID = ivec2(coord) / ivec2(16, 16); //Current Fragment position / Tile count
    uint index = tileID.y * uint(u_PushConstant.LightCullingWorkgroupX) + tileID.x;

    uint offset = index * 1024;
    return u_VisibleRectLightData.Indices[offset + i];
}

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
			cascadeIndex = i + 2;
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

vec3 GetPointLightContribution(PointLight pointLight, vec3 worldPos, vec3 viewVector, float phase)
{
	float Ld          = length(pointLight.Position - worldPos); //  Light distance
	float attenuation = clamp(1.0 - (Ld * Ld) / (pointLight.Radius * pointLight.Radius), 0.0, 1.0);
	attenuation      *= mix(attenuation, 1.0, pointLight.Falloff);

	vec3 lightDir = normalize(pointLight.Position - worldPos);

	float phaseFunction = GetMiePhase(
		dot(normalize(viewVector), lightDir),
		phase
	);

	return pointLight.Intensity * pointLight.Radiance * phaseFunction * attenuation;
}

// Vector form without project to the plane (dot with the normal)
// Use for proxy sphere clipping
vec3 IntegrateEdgeVec(vec3 v1, vec3 v2)
{
    // Using built-in acos() function will result flaws
    // Using fitting result for calculating acos()
    float x = dot(v1, v2);
    float y = abs(x);

    float a = 0.8543985 + (0.4965155 + 0.0145206*y)*y;
    float b = 3.4175940 + (4.1616724 + y)*y;
    float v = a / b;

    float theta_sintheta = (x > 0.0) ? v : 0.5*inversesqrt(max(1.0 - x*x, 1e-7)) - v;

    return cross(v1, v2)*theta_sintheta;
}
vec3 EvaluateLTC(mat3 invM, RectangularLight rectLight, vec3 worldPos, vec3 viewVector, vec3 averageLightDir)
{
	bool twoSided = bool(rectLight.Vertex0.w > 0.0);

	vec3 T1, T2;
	T1 = normalize(viewVector - averageLightDir * dot(viewVector, averageLightDir));
	T2 = cross(averageLightDir, T1);

	invM = invM * transpose(mat3(T1, T2, averageLightDir));

	// polygon (allocate 4 vertices for clipping)
	vec3 L[4];
	// transform polygon from LTC back to origin D0 (cosine weighted)
	L[0] = invM * (rectLight.Vertex0.xyz - worldPos);
	L[1] = invM * (rectLight.Vertex1.xyz - worldPos);
	L[2] = invM * (rectLight.Vertex2.xyz - worldPos);
	L[3] = invM * (rectLight.Vertex3.xyz - worldPos);

	// cos weighted space
    L[0] = normalize(L[0]);
    L[1] = normalize(L[1]);
    L[2] = normalize(L[2]);
    L[3] = normalize(L[3]);

	// integrate
    vec3 integral = vec3(0.0);
    integral += IntegrateEdgeVec(L[0], L[1]);
    integral += IntegrateEdgeVec(L[1], L[2]);
    integral += IntegrateEdgeVec(L[2], L[3]);
    integral += IntegrateEdgeVec(L[3], L[0]);

	float formFactor = length(integral);

	// use tabulated horizon-clipped sphere
    // check if the shading point is behind the light
    vec3 dir = rectLight.Vertex0.xyz - worldPos; // LTC space
    vec3 lightNormal = cross(rectLight.Vertex1.xyz - rectLight.Vertex0.xyz, rectLight.Vertex3.xyz - rectLight.Vertex0.xyz);
    bool behind = (dot(dir, lightNormal) < 0.0);
	float z = integral.z / formFactor * mix(1.0, -1.0, float(behind));

	vec2 uv = vec2(z * 0.5 + 0.5, formFactor); // range [0, 1]
    uv = uv * LtcLutTextureScale + vec2(LtcLutTextureBias);

	// Fetch the form factor for horizon clipping
    float scale = texture(u_LTC2Lut, uv).w;

	float sum = formFactor * scale;
	if(!behind && rectLight.Vertex0.w == 0.0)
		sum = 0.0;

	// Outgoing radiance (solid angle) for the entire polygon
    return vec3(sum);
	
}
vec3 ComputeRectangularLightContribution(RectangularLight rectLight, vec3 centerRectLightPos, vec3 worldPos, vec3 viewVector, float phase)
{
	// Attenuation
	float rectLightRadius = rectLight.Vertex1.w;
	float falloff = 3.0;

	float Ld          = length(centerRectLightPos - worldPos); //  Light distance
	float attenuation = clamp(1.0 - (Ld * Ld) / (rectLightRadius * rectLightRadius), 0.0, 1.0);
	attenuation      *= mix(attenuation, 1.0, falloff);

	if(attenuation == 0.0)
		return vec3(0.0);
	
	vec3 avgLightDir = normalize(centerRectLightPos - worldPos);

	// Need to multiply by \pi to account for all incident light as evaluateLTC divides by \pi implicitly.
	vec3 radiance = PI * EvaluateLTC(mat3(1.0), rectLight, worldPos, viewVector, avgLightDir);
	
	float phaseFunction = GetMiePhase(
		dot(normalize(viewVector), avgLightDir),
		phase
	);

	return rectLight.Radiance * rectLight.Intensity * radiance * attenuation;
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
	vec2 uv = (vec2(invoke.xy) + vec2(0.5)) / vec2(numFroxels.xy);// + noise.xy;

	// Compute the world space position of the froxel's texel
	vec4 worldSpaceMax = u_PushConstant.InvViewProjMatrix * vec4(uv * 2.0 - vec2(1.0), 1.0, 1.0);
	worldSpaceMax /= worldSpaceMax.w;
	vec3 direction = worldSpaceMax.xyz - u_PushConstant.CameraPosition;
	float w = (float(invoke.z) + 0.5) / float(numFroxels.z) + noise.z;
	vec3 worldSpacePosition = u_PushConstant.CameraPosition + direction * w * w;

	// Sampling the froxels
	vec4 scattExtinction = texelFetch(u_ScatExtinctionFroxel, invoke, 0);
	vec4 emissionPhase = texelFetch(u_EmissionPhaseFroxel, invoke, 0);


	// Computing Cascaded Shadow Map Contribution
	uint cascadeIndex = GetCascadeIndex(worldSpacePosition.z);
	mat4 cascadeMat = GetCascadeMatrix(cascadeIndex);
	vec4 shadowCoord = cascadeMat * vec4(worldSpacePosition, 1.0f);
	float visibility = SampleShadowTexture(shadowCoord, cascadeIndex);

	
	// Compute the point light factor for the point light contribution (all point lights color into a singe froxel)
	vec3 pointLightFactor = vec3(0.0);
	if(scattExtinction.w > 0.0f)
	{
		for(int i = 0; i < u_PushConstant.PointLightCount; i++)
		{
			int lightIndex = GetPointLightBufferIndex(int(i), uv);
			if (lightIndex == -1)
				break;

			const PointLight pointLight = u_PointLightData.PointLights[lightIndex];

			if(length(pointLight.Position - worldSpacePosition) <= pointLight.Radius)
			{
				pointLightFactor += GetPointLightContribution(
					pointLight,
					worldSpacePosition,
					direction,
					emissionPhase.w
				);
			}
		}
	}

	vec3 rectangularLightFactor = vec3(0.0);
	if(scattExtinction.w > 0.0f)
	{
		for(int i = 0; i < u_PushConstant.RectangularLightCount; i++)
		{
			int lightIndex = GetRectangularLightBufferIndex(int(i), uv);
			if (lightIndex == -1)
				break;

			const RectangularLight rectLight = u_RectLightData.RectLights[lightIndex];

			vec3 centerRectLightPos = ( rectLight.Vertex0.xyz +
										rectLight.Vertex1.xyz +
										rectLight.Vertex2.xyz +
										rectLight.Vertex3.xyz ) / 4.0;

			if(length(centerRectLightPos - worldSpacePosition) <= rectLight.Vertex1.w/*Radius*/)
			{
				rectangularLightFactor += ComputeRectangularLightContribution(
					rectLight,
					centerRectLightPos,
					worldSpacePosition,
					direction,
					emissionPhase.w
				);
			}
		}
	}

	// Compute the light integral for the fog volumes
	float phaseFunction = GetMiePhase(
		dot(normalize(direction), u_PushConstant.DirectionalLightDir),
		emissionPhase.w
	);
	vec3 mieScattering = scattExtinction.xyz;
	vec3 extinction = vec3(scattExtinction.w);
	vec3 voxelAlbedo = mieScattering / extinction;
	vec3 fogVolumesContribution = max(voxelAlbedo * phaseFunction * visibility * 1.0f * u_DirLightData.Intensity, vec3(0.0)) + emissionPhase.rgb;

	// Compute the light integral for the point lights
	vec3 pointLightContribution = max(pointLightFactor * max(visibility, 0.1), vec3(0.0));
	vec3 rectLightContribution = max(rectangularLightFactor * max(visibility, 0.1), vec3(0.0));




	// Compute the light integral for the directional light
	vec3 dirLightContribution = vec3(0.0f);
	vec3 dirLightExtinction = vec3(0.0f);
	if(u_DirLightData.Density > 0.0f)
	{
		vec3 dirLightMieScattering = u_DirLightData.MieScattering * pow(u_DirLightData.Density, 3);
		dirLightExtinction = vec3(dot(dirLightMieScattering, vec3(1.0 / 3.0)) + (u_DirLightData.Absorption * u_DirLightData.Density));

		vec3 dirLightVoxelAlbedo = dirLightMieScattering / dirLightExtinction;
		
		float dirLightphaseFunction = GetMiePhase(dot(normalize(direction), u_PushConstant.DirectionalLightDir), u_DirLightData.Phase);
		
		vec3 dirLightFinalContribution = dirLightVoxelAlbedo * dirLightphaseFunction;

		dirLightContribution = max(dirLightVoxelAlbedo * (visibility - 0.1f), vec3(0.0f));
	}


	vec3 finalResult = (rectLightContribution + pointLightContribution + fogVolumesContribution + dirLightContribution);

	imageStore(u_ScatExtinctionFroxel_Output, invoke, vec4(finalResult, scattExtinction.w + dirLightExtinction.x));
}