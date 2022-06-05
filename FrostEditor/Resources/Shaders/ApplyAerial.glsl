#type compute
#version 460

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(binding = 0) uniform sampler3D u_AerialLUT;
layout(binding = 1) uniform sampler2D u_DepthBuffer;
layout(binding = 2) uniform sampler2D u_ViewPos;
layout(binding = 3) uniform writeonly image2D o_Image;

// Camera specific uniforms.
layout(binding = 4) uniform CameraBlock
{
	mat4 ViewMatrix;
	mat4 ProjMatrix;
	mat4 InvViewProjMatrix;
	vec4 CamPosition;
	vec4 NearFarPlane; // Near plane (x), far plane (y). z and w are unused.
} u_CameraSpecs;

vec4 TricubicLookup(sampler3D tex, vec3 coord);

float FogFactorExp2( const float dist, const float density)
{
	const float LOG2 = -1.442695;
	float d = density * dist;
	return 1.0 - clamp(exp2(d * d * LOG2), 0.0, 1.0);
}

void main()
{
	ivec2 globalInvocation = ivec2(gl_GlobalInvocationID.xy);

	vec2 dTexCoords = gl_GlobalInvocationID.xy / textureSize(u_DepthBuffer, 0).xy;
	vec3 aerialPersTexSize = vec3(textureSize(u_AerialLUT, 0).xyz);

	// Compute aerial perspective position using the view frustum near/far planes.
	float planeDelta = (u_CameraSpecs.NearFarPlane.y - u_CameraSpecs.NearFarPlane.x) * 1e-6 / aerialPersTexSize.z;
	float slice = texture(u_DepthBuffer, dTexCoords).r / planeDelta;

	// Fade the slice out if it's close to the near plane.
	float weight = 1.0;
	if (slice < 0.5)
	{
		weight = clamp(2.0 * slice, 0.0, 1.0);
		slice = 0.5;
	}
	float w = sqrt(slice);

	// Spatial AA as described in [Patry2021]:
	// http://advances.realtimerendering.com/s2021/jpatry_advances2021/index.html
	vec4 aerialVoxel = TricubicLookup(u_AerialLUT, vec3(dTexCoords.xy, w));

	// Density AA as described in [Patry2021]:
	// http://advances.realtimerendering.com/s2021/jpatry_advances2021/index.html
	aerialVoxel.rgb *= aerialVoxel.a;
	aerialVoxel.rgb *= weight;

	//vec3 viewPos = texelFetch(u_ViewPos, globalInvocation, 0).rgb;
	//vec3 rayDir = viewPos - u_CameraSpecs.CamPosition.xyz;
	//float cameraToSampleDistance = length(rayDir);
	//
	//float fogAmount = FogFactorExp2(cameraToSampleDistance, 0.05f);
	//
	//vec3 color = mix(aerialVoxel.rgb, vec3(1.0f), fogAmount);

	//imageStore(o_Image, globalInvocation, vec4(aerialVoxel.rgb, 1.0f));
}


/*
 * The filter below follows this license:
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
*/

vec4 TricubicLookup(sampler3D tex, vec3 coord)
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
  vec3 h0 = mult * ((w1 / g0) - 0.5 + index);
  vec3 h1 = mult * ((w3 / g1) + 1.5 + index);

  // fetch the eight linear interpolations
  // weighting and fetching is interleaved for performance and stability reasons
  vec4 tex000 = texture(tex, h0).rgba;
  vec4 tex100 = texture(tex, vec3(h1.x, h0.y, h0.z)).rgba;
  tex000 = mix(tex100, tex000, g0.x);  //weigh along the x-direction
  vec4 tex010 = texture(tex, vec3(h0.x, h1.y, h0.z)).rgba;
  vec4 tex110 = texture(tex, vec3(h1.x, h1.y, h0.z)).rgba;
  tex010 = mix(tex110, tex010, g0.x);  //weigh along the x-direction
  tex000 = mix(tex010, tex000, g0.y);  //weigh along the y-direction
  vec4 tex001 = texture(tex, vec3(h0.x, h0.y, h1.z)).rgba;
  vec4 tex101 = texture(tex, vec3(h1.x, h0.y, h1.z)).rgba;
  tex001 = mix(tex101, tex001, g0.x);  //weigh along the x-direction
  vec4 tex011 = texture(tex, vec3(h0.x, h1.y, h1.z)).rgba;
  vec4 tex111 = texture(tex, h1).rgba;
  tex011 = mix(tex111, tex011, g0.x);  //weigh along the x-direction
  tex001 = mix(tex011, tex001, g0.y);  //weigh along the y-direction

  return mix(tex001, tex000, g0.z);  //weigh along the z-direction
}
