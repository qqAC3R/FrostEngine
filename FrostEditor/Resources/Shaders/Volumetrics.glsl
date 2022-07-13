#type compute
#version 460

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

// Constants
const float PI = 3.141592;
const float G_SCATTERING = 0.0f;

layout(binding = 0) uniform sampler2D u_PositionTex;
layout(binding = 1) uniform sampler2D u_ShadowDepthTexture;
layout(binding = 2, rgba8) uniform writeonly image2D u_VolumetricTex;

layout(push_constant) uniform PushConstant
{
	mat4 CameraViewMatrix;


	vec3 DirectionalLightDir;

	float A;

	vec3 CameraPosition;
	float B;

} u_PushConstant;

layout(binding = 3) uniform DirectionaLightData
{
	mat4 LightViewProjMatrix0;
	mat4 LightViewProjMatrix1;
	mat4 LightViewProjMatrix2;
	mat4 LightViewProjMatrix3;

	float CascadeDepthSplit[4];
} u_DirLightData;

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

bool HardShadows_SampleShadowTexture(vec4 shadowCoord, uint cascadeIndex)
{
	float bias = 0.005;

	if (shadowCoord.z > -1.0 && shadowCoord.z < 1.0 )
	{
		float dist = SampleShadowMap(shadowCoord.xy * 0.5 + 0.5, cascadeIndex);

		if (shadowCoord.w > 0 && dist < shadowCoord.z - bias)
		{
			return true;
		}
	}
	return false;
}

float ComputeScattering(float lightDotView)
{
	float result = 1.0f - G_SCATTERING * G_SCATTERING;
	result /= (4.0f * PI * pow(1.0f + G_SCATTERING * G_SCATTERING - (2.0f * G_SCATTERING) *  lightDotView, 1.5f));
	return result;
}


vec4 ditherPattern[4] = {
	{ 0.0f, 0.5f, 0.125f, 0.625f},
	{ 0.75f, 0.22f, 0.875f, 0.375f},
	{ 0.1875f, 0.6875f, 0.0625f, 0.5625},
	{ 0.9375f, 0.4375f, 0.8125f, 0.3125}
};


vec3 RayMarch(vec3 hitPointPos, vec3 cameraPosition, vec3 directionaLightDir, float stepCount)
{
	vec3 rayMarchVector = cameraPosition - hitPointPos;
	float rayMarchVectorLength = length(rayMarchVector);
	vec3 rayMarchVectorDir = rayMarchVector / rayMarchVectorLength;

	float stepLength = rayMarchVectorLength / stepCount;
	vec3 stepDir = rayMarchVectorDir * stepLength;

	vec3 currentPosition = hitPointPos;
	currentPosition += stepDir * ditherPattern[gl_GlobalInvocationID.x % 4].xyz;

	vec3 accumFog = vec3(0.0f);

	for(uint i = 0; i < stepCount - 1; i++)
	{
		// Sample the best cascade
		{
			vec4 viewSpacePos = u_PushConstant.CameraViewMatrix * vec4(currentPosition, 1.0f);
			uint cascadeIndex = GetCascadeIndex(viewSpacePos.z);
			cascadeIndex = 1;
			
			mat4 lightViewProjMat = GetCascadeMatrix(cascadeIndex);
			vec4 shadowPos = lightViewProjMat * vec4(currentPosition, 1.0);
			shadowPos /= shadowPos.w;

			bool isShadowed = HardShadows_SampleShadowTexture(shadowPos, cascadeIndex);
			
			if(!isShadowed)
			{
				accumFog += vec3(ComputeScattering(dot(rayMarchVectorDir, directionaLightDir)));
			}
		}

		currentPosition += stepDir;
	}

	accumFog /= stepCount;

	return clamp(accumFog, vec3(0.0f), vec3(0.5f));
}

// TODO: https://github.com/SanielX/Height-Fog/blob/c49cecffea7e5544fb903b4e45705da3d5d17a4d/Resources/HeightFogUsage.hlsl
vec3 ComputeFog( in vec3  rgb,       // original color of the pixel
               in float distance,  // camera to point distance
               in vec3  rayOrigin, // camera position
               in vec3  rayDir )   // camera to point vector
{
	float a = u_PushConstant.A;
	float b = u_PushConstant.B;

    float fogAmount = (a/b) * exp(-rayOrigin.y*b) * (1.0-exp( -distance*rayDir.y*b ))/rayDir.y;
    vec3  fogColor = vec3(0.1);
    return mix( rgb, fogColor, fogAmount );
}

void main()
{
	ivec2 globalInvocation = ivec2(gl_GlobalInvocationID.xy);

	vec3 worldPos = texelFetch(u_PositionTex, globalInvocation, 0).rgb;

	vec3 scatteringResult = RayMarch(
		worldPos,
		u_PushConstant.CameraPosition,
		u_PushConstant.DirectionalLightDir,
		10
	);


	vec3 viewSpacePos = vec3(u_PushConstant.CameraViewMatrix * vec4(worldPos, 1.0f));

	vec3 fog = ComputeFog(vec3(0.03f), length(u_PushConstant.CameraPosition - viewSpacePos), u_PushConstant.CameraPosition, normalize(u_PushConstant.CameraPosition - viewSpacePos));

	imageStore(u_VolumetricTex, globalInvocation, vec4((scatteringResult + fog) / 2.0 , 1.0f));
}