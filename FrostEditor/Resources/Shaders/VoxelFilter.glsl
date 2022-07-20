#type compute
#version 460

layout (local_size_x = 8, local_size_y = 8, local_size_z = 8) in;

layout(binding = 0, rgba8) uniform writeonly image3D o_VoxelTexture;
layout(binding = 1) uniform sampler3D u_VoxelTexture;
layout(binding = 2) uniform sampler2D u_ShadowDepthTexture;

layout(push_constant) uniform Constants
{
	mat4 CameraViewMatrix;

	vec4 CameraPosition_SampleMipLevel;
	
	float ProjectionExtents;
	float VoxelScale;

} u_PushConstant;

layout(binding = 3) uniform DirectionaLightData
{
	mat4 LightViewProjMatrix0;
	mat4 LightViewProjMatrix1;
	mat4 LightViewProjMatrix2;
	mat4 LightViewProjMatrix3;

	float CascadeDepthSplit[4];

	float DirectionalLightIntensity;

} u_DirLightData;

const ivec3 anisoOffsets[] = ivec3[8]
(
	ivec3(1, 1, 1),
	ivec3(1, 1, 0),
	ivec3(1, 0, 1),
	ivec3(1, 0, 0),
	ivec3(0, 1, 1),
	ivec3(0, 1, 0),
	ivec3(0, 0, 1),
	ivec3(0, 0, 0)
);


void FetchTexels(ivec3 pos, int dir6, inout vec4 val[8]) 
{
	for(int i = 0; i < 8; i++)
	{
		val[i] = texelFetch(u_VoxelTexture, pos + anisoOffsets[i], 0);
	}
}

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

float SampleGather_ShadowMap(vec2 shadowCoords, uint cascadeIndex)
{
	vec2 coords = ComputeShadowCoord(shadowCoords.xy, cascadeIndex);
	vec4 dist = textureGather(u_ShadowDepthTexture, coords, 0);
	return (dist.x + dist.y + dist.z + dist.w) / 4.0f;
}

float HardShadows_SampleShadowTexture(vec4 shadowCoord, uint cascadeIndex)
{
	float shadow = u_DirLightData.DirectionalLightIntensity;
	float bias = 0.01;

	if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 )
	{
		float dist = SampleShadowMap(shadowCoord.xy * 0.5 + 0.5, cascadeIndex);
		float gatherPixels = SampleGather_ShadowMap(shadowCoord.xy * 0.5 + 0.5, cascadeIndex);

		dist = (dist + gatherPixels) / 2.0f;

		if (shadowCoord.w > 0 && dist < shadowCoord.z - bias)
		{
			shadow = 0.3f;
		}
	}
	return shadow;
}


// Interesting
// https://github.com/Hanlin-Zhou/OpenGL-VXGI-Engine/blob/master/shader/MipMap.comp

void main()
{
	ivec3 writePos = ivec3(gl_GlobalInvocationID);
	ivec3 sourcePos = writePos * 2;

	if(u_PushConstant.CameraPosition_SampleMipLevel.w == 0)
	{
		vec4 val = texelFetch(u_VoxelTexture, writePos, 0);

		if(val.r != 0.0f || val.g != 0.0f || val.b != 0.0f)
		{
			vec3 cameraPos = u_PushConstant.CameraPosition_SampleMipLevel.xyz;
			float aabbExtents = u_PushConstant.ProjectionExtents;
			vec3 voxelPosition = vec3(cameraPos.x - aabbExtents, cameraPos.y - aabbExtents, cameraPos.z - aabbExtents);
			
			voxelPosition.y += (float(writePos.y) * u_PushConstant.VoxelScale);
			voxelPosition.x += (float(writePos.x) * u_PushConstant.VoxelScale);
			voxelPosition.z += (float(writePos.z) * u_PushConstant.VoxelScale);
			
			
			vec4 viewSpacePos = u_PushConstant.CameraViewMatrix * vec4(voxelPosition, 1.0);
			uint cascadeIndex = GetCascadeIndex(viewSpacePos.z);

			mat4 lightViewProjMat = GetCascadeMatrix(cascadeIndex);
			vec4 shadowPos = lightViewProjMat * vec4(voxelPosition, 1.0);
			shadowPos /= shadowPos.w;

			//vec3 shadowFactor;
			//switch(cascadeIndex)
			//{
			//	case 1: shadowFactor = vec3(1.0f, 0.25f, 0.25f); break;
			//	case 2: shadowFactor = vec3(0.25f, 1.0f, 0.25f); break;
			//	case 3: shadowFactor = vec3(0.25f, 0.25f, 1.0f); break;
			//	case 4: shadowFactor = vec3(1.0f, 1.0f, 0.25f);  break;
			//}

			float shadowFactor = HardShadows_SampleShadowTexture(shadowPos, cascadeIndex);
			
			val.rgb *= shadowFactor;
			//val.rgb = voxelPosition;
			
			val.a = 1.0f;
		}


		imageStore(o_VoxelTexture, writePos, val);

		return;
	}

	vec4 values[8];
	FetchTexels(sourcePos, 0, values);

	imageStore(o_VoxelTexture, writePos, 
		(
		values[0] + values[4] +
		values[1] + values[5] +
		values[2] + values[6] +
		values[3] + values[7]
		) / 8.0f
	);

	//imageStore(o_VoxelTexture, writePos, 
	//	(
	//	values[0] + values[4] * (1 - values[0].a) + 
	//	values[1] + values[5] * (1 - values[1].a) +
	//	values[2] + values[6] * (1 - values[2].a) +
	//	values[3] + values[7] * (1 - values[3].a)
	//	) * 0.25f
	//);


}