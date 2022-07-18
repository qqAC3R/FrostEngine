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

layout(binding = 0) uniform sampler3D u_FinalGatherFroxel;
layout(binding = 1) uniform sampler2D u_DepthTexture;
layout(binding = 2) uniform sampler2D u_SpatialBlueNoiseLUT;
layout(binding = 3) uniform writeonly image2D u_VolumetricTex;

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

// ------------------------ LUTS --------------------------
vec4 FastTricubicLookup(sampler3D tex, vec3 coord);

vec4 SampleBlueNoise(ivec2 coords)
{
	return texture(u_SpatialBlueNoiseLUT, (vec2(coords) + 0.5.xx) / vec2(textureSize(u_SpatialBlueNoiseLUT, 0).xy));
}
// ------------------- VOLUMETRICS -----------------------
vec4 GetValueFromFroxel(vec3 position, float maxT, vec2 uv, vec3 noise)
{
	float z = length(position - u_PushConstant.CameraPosition);
	z /= maxT;
	z = pow(z, 1.0 / 2.0);

	vec3 volumeUVW = vec3(uv, z);

	// Offset the coords with some noise
	volumeUVW += (2.0 * noise - vec3(1.0)) / textureSize(u_FinalGatherFroxel, 0).xyz;

	// Get the value
	vec4 result = FastTricubicLookup(u_FinalGatherFroxel, volumeUVW);
	return result;
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

void main()
{
	ivec2 invoke = ivec2(gl_GlobalInvocationID.xy);
	ivec2 outImageSize = ivec2(imageSize(u_VolumetricTex).xy);

	// Quit early for threads that aren't in bounds of the screen.
	if (any(greaterThanEqual(invoke, imageSize(u_VolumetricTex).xy)))
		return;

	ivec2 coords = invoke;

	vec4 noise = SampleBlueNoise(coords);
	vec3 worlSpacePos = DecodePosition(coords);

	vec2 uv = (vec2(coords) + vec2(0.5f)) / vec2(imageSize(u_VolumetricTex).xy);
	vec4 worldSpaceMax = u_PushConstant.InvViewProjMatrix * vec4(2.0 * uv - vec2(1.0), 1.0f, 1.0f);
	worldSpaceMax /= worldSpaceMax.w;

	float distanceToFarPlane = length(worldSpaceMax.xyz - u_PushConstant.CameraPosition);


	vec4 result = GetValueFromFroxel(worlSpacePos, distanceToFarPlane, uv, noise.rgb);

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