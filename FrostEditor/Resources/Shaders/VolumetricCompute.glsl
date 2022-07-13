/*
	Sources:
		https://github.com/nuclearkevin/Strontium
		https://www.ea.com/frostbite/news/physically-based-unified-volumetric-rendering-in-frostbite
*/
#type compute
#version 460

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

#define PI 3.141592654
#define EPSILON 1e-6

layout(binding = 0) uniform sampler3D u_ScatExtinctionFroxel;
layout(binding = 1) uniform sampler3D u_EmissionPhaseFroxel;
layout(binding = 2) uniform sampler2D u_DepthTexture;
layout(binding = 3) uniform sampler2D u_BlueNoiseLUT;
layout(binding = 4) uniform sampler2D u_ShadowDepthTexture;
layout(binding = 5) uniform writeonly image2D u_VolumetricTex;

layout(binding = 6) uniform DirectionaLightData
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

	float FogVolumesCount;

	vec3 DirectionalLightDir;
} u_PushConstant;


struct ScatteringParams
{
	vec3 Emission;
	vec3 Scattering;
	float Phase;
	float Extinction;
};

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

bool HardShadows_SampleShadowTexture(vec4 shadowCoord, uint cascadeIndex)
{
	float bias = 0.005;

	if (shadowCoord.z > -1.0 && shadowCoord.z < 1.0 )
	{
		float dist = SampleShadowMap(shadowCoord.xy * 0.5 + 0.5, cascadeIndex);

		if (shadowCoord.w > 0 && dist < shadowCoord.z - bias)
		{
			return false;
		}
	}
	return true;
}
// ------------------------ LUTS --------------------------
vec4 FastTricubicLookup(sampler3D tex, vec3 coord);

vec4 SampleBlueNoise(ivec2 coords)
{
	return texture(u_BlueNoiseLUT, (vec2(coords) + 0.5.xx) / vec2(textureSize(u_BlueNoiseLUT, 0).xy));
}
// ------------------- VOLUMETRICS -----------------------
ScatteringParams GetScatteringParamsAtPos(vec3 position, float maxT, vec2 uv, vec3 noise)
{
	ScatteringParams outParams;

	float z = length(position - u_PushConstant.CameraPosition);
	z /= maxT;
	z = pow(z, 1.0 / 2.0);

	vec3 volumeUVW = vec3(uv, z);

	// Offset the coords with some noise
	volumeUVW += (2.0 * noise - vec3(1.0)) / textureSize(u_ScatExtinctionFroxel, 0).xyz;

	// Get the values
	vec4 mieScattering_extinction = FastTricubicLookup(u_ScatExtinctionFroxel, volumeUVW);
	vec4 emissionPhase = FastTricubicLookup(u_EmissionPhaseFroxel, volumeUVW);

	outParams.Scattering = mieScattering_extinction.rgb;
	outParams.Extinction = mieScattering_extinction.a;
	outParams.Emission = emissionPhase.rgb;
	outParams.Phase = emissionPhase.a;

	return outParams;
}

// Decodes the worldspace position of the fragment from depth.
vec3 DecodePosition(ivec2 coords)
{
	vec2 texCoords = (vec2(coords) + vec2(0.5f)) / vec2(imageSize(u_VolumetricTex).xy);

	float depth = textureLod(u_DepthTexture, texCoords, 1).r;
	vec3 clipCoords = vec3(texCoords * 2.0 - 1.0, depth);
	
	vec4 temp = u_PushConstant.InvViewProjMatrix * vec4(clipCoords, 1.0);
	return temp.xyz / temp.w;
}


float GetMiePhase(float cosTheta, float g)
{
	const float scale = 3.0 / (8.0 * PI);
	
	float num = (1.0 - g * g) * (1.0 + cosTheta * cosTheta);
	float denom = (2.0 + g * g) * pow((1.0 + g * g - 2.0 * g * cosTheta), 1.5);
	
	return scale * num / denom;
}

vec4 ComputeVolumetrics()
{
	const uint numSteps = 48;
	ivec2 coords = ivec2(gl_GlobalInvocationID.xy);

	// The end point of the raymarch
	//vec3 endPos = texelFetch(u_PositionTexture, coords, 0).rgb;
	vec4 noise = SampleBlueNoise(coords);

	vec3 endPos = DecodePosition(coords);

	// Computing the ray length and step length from the end point to the camera
	float rayLength = length(endPos - u_PushConstant.CameraPosition) - EPSILON;
	float stepLength = rayLength / max(float(numSteps) - 1.0, 1.0); 
	
	// Getting the normalized ray direction
	vec3 rayDir = (endPos - u_PushConstant.CameraPosition) / rayLength;
	//vec3 rayDir = (u_PushConstant.CameraPosition - endPos) / rayLength;

	// Start position of the ray march
	vec3 startPos = u_PushConstant.CameraPosition + rayDir * stepLength * noise.w;

	float startEndLength = length(endPos - startPos) - EPSILON;
	stepLength = startEndLength / max(float(numSteps) - 1.0, 1.0);

	vec2 uv = (vec2(coords) + vec2(0.5f)) / vec2(imageSize(u_VolumetricTex).xy);
	vec4 worldSpaceMax = u_PushConstant.InvViewProjMatrix * vec4(2.0 * uv - vec2(1.0), 1.0f, 1.0f);
	worldSpaceMax /= worldSpaceMax.w;

	float distanceToFarPlane = length(worldSpaceMax.xyz - u_PushConstant.CameraPosition);

	vec3 luminance = vec3(0.0f);
	vec3 transmittance = vec3(1.0f);
	float currentSteps = 0.0f;
	for(uint i = 0; i < numSteps; i++)
	{
		vec3 samplePos = startPos + currentSteps * rayDir;

		uint cascadeIndex = GetCascadeIndex(samplePos.z);
		mat4 cascadeMat = GetCascadeMatrix(cascadeIndex);
		vec4 shadowCoord = cascadeMat * vec4(samplePos, 1.0f);
		bool visibility = HardShadows_SampleShadowTexture(shadowCoord, cascadeIndex);

		float shadowContribution = 1.0f;
		if(!visibility)
		{
			shadowContribution = 0.1f;
		}

		ScatteringParams params = GetScatteringParamsAtPos(samplePos, distanceToFarPlane, uv, noise.rgb);

		float phaseFunction = GetMiePhase(dot(rayDir, u_PushConstant.DirectionalLightDir), params.Phase);
		vec3 mieScattering = params.Scattering;
		vec3 extinction = vec3(params.Extinction);
		vec3 voxelAlbedo = mieScattering / extinction;

		
		vec3 lScattering = voxelAlbedo * phaseFunction * shadowContribution;// * visibility
		vec3 sampleTransmittance = exp(-stepLength * extinction);
		vec3 scatteringIntegral = (lScattering - lScattering * sampleTransmittance) / extinction;

		luminance += max(scatteringIntegral * transmittance, 0.0f);
		luminance += params.Emission * transmittance;
		transmittance *= min(sampleTransmittance, 1.0f);

		//if(visibility)
		//{
		//	transmittance *= min(sampleTransmittance, 1.0f);
		//}



		currentSteps += stepLength;
	}

	//luminance *= u_lightColourIntensity.xyz * u_lightColourIntensity.w;
	luminance *= vec3(1.0f) * 5.0f;

	return vec4(luminance, dot(vec3(1.0 / 3.0), transmittance));
}
// -------------------------------------------------------

void main()
{
	ivec2 invoke = ivec2(gl_GlobalInvocationID.xy);
	ivec2 outImageSize = ivec2(imageSize(u_VolumetricTex).xy);

	// Quit early for threads that aren't in bounds of the screen.
	if (any(greaterThanEqual(invoke, imageSize(u_VolumetricTex).xy)))
		return;

	vec4 result = ComputeVolumetrics();

	imageStore(u_VolumetricTex, invoke, result);
}

// The code below follows this license.
/*--------------------------------------------------------------------------*\
Copyright (c) 2008-2009, Danny Ruijters. All rights reserved.
http://www.dannyruijters.nl/cubicinterpolation/
This file is part of CUDA Cubic B-Spline Interpolation (CI).
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
*  Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
*  Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
*  Neither the name of the copyright holders nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
The views and conclusions contained in the software and documentation are
those of the authors and should not be interpreted as representing official
policies, either expressed or implied.
When using this code in a scientific project, please cite one or all of the
following papers:
*  Daniel Ruijters and Philippe Thévenaz,
   GPU Prefilter for Accurate Cubic B-Spline Interpolation,
   The Computer Journal, vol. 55, no. 1, pp. 15-20, January 2012.
   http://dannyruijters.nl/docs/cudaPrefilter3.pdf
*  Daniel Ruijters, Bart M. ter Haar Romeny, and Paul Suetens,
   Efficient GPU-Based Texture Interpolation using Uniform B-Splines,
   Journal of Graphics Tools, vol. 13, no. 4, pp. 61-69, 2008.
\*--------------------------------------------------------------------------*/

vec4 FastTricubicLookup(sampler3D tex, vec3 coord)
{
  // shift the coordinate from [0,1] to [-0.5, nrOfVoxels-0.5]
  vec3 nrOfVoxels = vec3(textureSize(tex, 0));
  vec3 coord_grid = coord * nrOfVoxels - 0.5;
  vec3 index = floor(coord_grid);
  vec3 fraction = coord_grid - index;
  vec3 one_frac = 1.0 - fraction;

  vec3 w0 = 1.0/6.0 * one_frac*one_frac*one_frac;
  vec3 w1 = 2.0/3.0 - 0.5 * fraction*fraction*(2.0-fraction);
  vec3 w2 = 2.0/3.0 - 0.5 * one_frac*one_frac*(2.0-one_frac);
  vec3 w3 = 1.0/6.0 * fraction*fraction*fraction;

  vec3 g0 = w0 + w1;
  vec3 g1 = w2 + w3;
  vec3 mult = 1.0 / nrOfVoxels;
  vec3 h0 = mult * ((w1 / g0) - 0.5 + index);  //h0 = w1/g0 - 1, move from [-0.5, nrOfVoxels-0.5] to [0,1]
  vec3 h1 = mult * ((w3 / g1) + 1.5 + index);  //h1 = w3/g1 + 1, move from [-0.5, nrOfVoxels-0.5] to [0,1]

  // fetch the eight linear interpolations
  // weighting and fetching is interleaved for performance and stability reasons
  vec4 tex000 = texture(tex, h0);
  vec4 tex100 = texture(tex, vec3(h1.x, h0.y, h0.z));
  tex000 = mix(tex100, tex000, g0.x);  //weigh along the x-direction
  vec4 tex010 = texture(tex, vec3(h0.x, h1.y, h0.z));
  vec4 tex110 = texture(tex, vec3(h1.x, h1.y, h0.z));
  tex010 = mix(tex110, tex010, g0.x);  //weigh along the x-direction
  tex000 = mix(tex010, tex000, g0.y);  //weigh along the y-direction
  vec4 tex001 = texture(tex, vec3(h0.x, h0.y, h1.z));
  vec4 tex101 = texture(tex, vec3(h1.x, h0.y, h1.z));
  tex001 = mix(tex101, tex001, g0.x);  //weigh along the x-direction
  vec4 tex011 = texture(tex, vec3(h0.x, h1.y, h1.z));
  vec4 tex111 = texture(tex, h1);
  tex011 = mix(tex111, tex011, g0.x);  //weigh along the x-direction
  tex001 = mix(tex011, tex001, g0.y);  //weigh along the y-direction

  return mix(tex001, tex000, g0.z);  //weigh along the z-direction
}