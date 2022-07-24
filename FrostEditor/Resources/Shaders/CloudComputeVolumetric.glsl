//#type compute
#version 460

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(binding = 0) uniform sampler2D u_DepthBuffer;
layout(binding = 1) uniform sampler3D u_CloudNoiseTex;
layout(binding = 2) uniform sampler3D u_ErroderTex;
layout(binding = 3) uniform sampler2D u_SpatialBlueNoiseLUT;
layout(binding = 4) uniform writeonly image2D u_CloudTexture;

struct CloudVolumeParams
{
    vec3 Position;
    float CloudScale;

    vec3 Scale;
    float Density;
    
    vec3 Scattering;
    float PhaseFunction;

    float DensityOffset;
    float CloudAbsorption;
    float SunAbsorption;
    float Padding0;
};

layout(binding = 5) readonly buffer CloudVolumeData
{
	CloudVolumeParams Volumes[];
} u_CloudVolumeData;

layout(push_constant) uniform PushConstant
{
	mat4 InvViewProjMatrix;
	vec3 CameraPosition;

    float NearPlane;
    
    vec3 DirectionalLightDir;

    float FarPlane;
    float CloudsCount;

} u_PushConstant;

#define saturate(x) clamp(x, 0.0, 1.0)
#define PI 3.141592654

float numStepsLight = 8.0; // uniform
float lightAbsorptionTowardSun = 1.21; // uniform
float lightAbsorptionThroughCloud = 0.75; // Uniform
float darknessThreshold = 0.15; // Uniform


vec4 FastTricubicLookup(sampler3D tex, vec3 coord);

 // Returns (dstToBox, dstInsideBox). If ray misses box, dstInsideBox will be zero
vec2 RayBoxDst(vec3 boundsMin, vec3 boundsMax, vec3 rayOrigin, vec3 invRaydir) {
    // Adapted from: http://jcgt.org/published/0007/03/04/
    vec3 t0 = (boundsMin - rayOrigin) * invRaydir;
    vec3 t1 = (boundsMax - rayOrigin) * invRaydir;
    vec3 tmin = min(t0, t1);
    vec3 tmax = max(t0, t1);
                
    float dstA = max(max(tmin.x, tmin.y), tmin.z);
    float dstB = min(tmax.x, min(tmax.y, tmax.z));

    // CASE 1: ray intersects box from outside (0 <= dstA <= dstB)
    // dstA is dst to nearest intersection, dstB dst to far intersection

    // CASE 2: ray intersects box from inside (dstA < 0 < dstB)
    // dstA is the dst to intersection behind the ray, dstB is dst to forward intersection

    // CASE 3: ray misses box (dstA > dstB)

    float dstToBox = max(0, dstA);
    float dstInsideBox = max(0, dstB - dstToBox);
    return vec2(dstToBox, dstInsideBox);
}

// Decodes the worldspace position of the fragment from depth.
vec3 DecodePosition(vec2 texCoords, float depth)
{
	vec3 clipCoords = vec3(texCoords * 2.0 - 1.0, depth);
	
	vec4 temp = u_PushConstant.InvViewProjMatrix * vec4(clipCoords, 1.0);
	return temp.xyz / temp.w;
}

vec4 SampleBlueNoise(ivec2 coords)
{
	return texture(u_SpatialBlueNoiseLUT, (vec2(coords) + 0.5.xx) / vec2(textureSize(u_SpatialBlueNoiseLUT, 0).xy));
}

float LinearizeDepth(float d)
{
	return u_PushConstant.NearPlane * u_PushConstant.FarPlane / (u_PushConstant.FarPlane + d * (u_PushConstant.NearPlane - u_PushConstant.FarPlane));
}

float Remap(float originalValue, float originalMin, float originalMax, float newMin, float newMax)
{
	return newMin + (((originalValue - originalMin) / (originalMax - originalMin)) * (newMax - newMin));
}

float SampleCloudDensity(vec3 rayPos, vec3 boxMin, vec3 boxMax, const CloudVolumeParams params, vec3 noise)
{
    /*
    vec3 boxSize = boxMax - boxMin;

    vec4 low_frequency_noise = FastTricubicLookup(u_CloudNoiseTex, rayPos * 0.01 * cloudScale);
    float lowFreqFBM = dot(low_frequency_noise.gba, vec3(0.625, 0.25, 0.125));
    float base_cloud = remap(low_frequency_noise.r, (lowFreqFBM - 1.0), 1.0, 0.0 , 1.0);
    
    float cloud_coverage = precipation;
    float base_cloud_with_coverage = remap(base_cloud, cloud_coverage, 1.0, 0.0, 1.0);
    base_cloud_with_coverage *= cloud_coverage;

    return clamp(base_cloud_with_coverage, 0.0, 1.0);


    // Calculate falloff at along x/z edges of the cloud container
    const float containerEdgeFadeDst = 50;
    float dstFromEdgeX = min(containerEdgeFadeDst, min(rayPos.x - boxMin.x, boxMax.x - rayPos.x));
    float dstFromEdgeZ = min(containerEdgeFadeDst, min(rayPos.z - boxMin.z, boxMax.z - rayPos.z));
    float edgeWeight = min(dstFromEdgeZ,dstFromEdgeX)/containerEdgeFadeDst;

    // Calculate height gradient
    float gMin = .2;
    float gMax = .7;
    float heightPercent = (rayPos.y - boxMin.y) / boxSize.y;
    float heightGradient = saturate(remap(heightPercent, 0.0, gMin, 0.0, 1.0))
                         * saturate(remap(heightPercent, 1.0, gMax, 0.0, 1.0));
    heightGradient *= edgeWeight;



    base_cloud_with_coverage *= heightGradient;

    if (base_cloud_with_coverage > 0) 
    {

        // Sample detail noise
        vec3 detailSamplePos = rayPos * 0.01 * cloudScale;
        vec4 detailNoise = FastTricubicLookup(u_ErroderTex, detailSamplePos);
    
        float highFreqFBM = dot(detailNoise.rgb, vec3(0.625, 0.25, 0.125));//(erodeCloudNoise.r * 0.625) + (erodeCloudNoise.g * 0.25) + (erodeCloudNoise.b * 0.125);
        float highFreqNoiseModifier = mix(highFreqFBM, 1.0 - highFreqFBM, clamp(heightGradient * 10.0, 0.0, 1.0));

        base_cloud_with_coverage = base_cloud_with_coverage - highFreqNoiseModifier * (1.0 - base_cloud_with_coverage);

        base_cloud_with_coverage = remap(base_cloud_with_coverage*2.0, highFreqNoiseModifier * 0.2, 1.0, 0.0, 1.0);
    }

    //return base_cloud_with_coverage;
    return clamp(base_cloud_with_coverage, 0.0, 1.0);

    */






    // Compute the coords for sampling (with noise)
    vec3 cloudUWV = rayPos * 0.001 * params.CloudScale;
    cloudUWV += (2.0 * noise - 1.0.xxx) / vec3(textureSize(u_CloudNoiseTex, 0).xyz);

    vec3 boxSize = boxMax - boxMin;

    // Calculate falloff at along x/z edges of the cloud container
    const float containerEdgeFadeDst = 1;
    float dstFromEdgeX = min(containerEdgeFadeDst, min(rayPos.x - boxMin.x, boxMax.x - rayPos.x));
    float dstFromEdgeZ = min(containerEdgeFadeDst, min(rayPos.z - boxMin.z, boxMax.z - rayPos.z));
    float edgeWeight = min(dstFromEdgeZ,dstFromEdgeX)/containerEdgeFadeDst;
                
    // Calculate height gradient
    float gMin = .2;
    float gMax = .7;
    float heightPercent = (rayPos.y - boxMin.y) / boxSize.y;
    float heightGradient = saturate(Remap(heightPercent, 0.0, gMin, 0.0, 1.0))
                         * saturate(Remap(heightPercent, 1.0, gMax, 0.0, 1.0));
    heightGradient *= edgeWeight;

    // Params
    vec4 shapeNoiseWeights = vec4(1, 0.48, 0.15, 0);
    float densityOffset = -params.DensityOffset; // Uniform

    // Calculate base shape density
    // As described here https://www.diva-portal.org/smash/get/diva2:1223894/FULLTEXT01.pdf
    vec4 low_frequency_noise = FastTricubicLookup(u_CloudNoiseTex, cloudUWV);
    float lowFreqFBM = dot(low_frequency_noise.gba, vec3(0.625, 0.25, 0.125));
    vec3 shapeNoise = vec3(Remap(low_frequency_noise.r, (lowFreqFBM - 1.0), 1.0, 0.0 , 1.0));
    
    vec3 normalizedShapeWeights = vec3(1.0) / dot(shapeNoiseWeights.rgb, vec3(1.0));
	float shapeFBM = dot(shapeNoise.rgb, normalizedShapeWeights) * heightGradient;
    float baseShapeDensity = shapeFBM + densityOffset * 0.1;

    if (baseShapeDensity > 0) 
    {
        vec3 detailUWV = rayPos * 0.001 * params.Scale;
        detailUWV  += (2.0 * noise - 1.0.xxx) / vec3(textureSize(u_ErroderTex, 0).xyz);

        // Sample detail noise
        vec4 detailNoise = FastTricubicLookup(u_ErroderTex, detailUWV);
    
        vec3 detailWeights = vec3(1.0, 0.5, 0.5);
        float detailNoiseWeight = 1.0;
    
        vec3 normalizedDetailWeights = detailWeights / dot(detailWeights, vec3(1.0));
        float detailFBM = dot(detailNoise.xyz, normalizedDetailWeights);
    
        // Subtract detail noise from base shape (weighted by inverse density so that edges get eroded more than centre)
        float oneMinusShape = 1 - shapeFBM;
        float detailErodeWeight = oneMinusShape * oneMinusShape * oneMinusShape;
        float cloudDensity = baseShapeDensity - (1.0 - detailFBM) * detailErodeWeight * detailNoiseWeight;
    
        float densityMultiplier = params.Density;
        return cloudDensity * densityMultiplier * 0.1;
    }
    return 0.0;
}

float LightMarch(vec3 rayPos, vec3 worldPos, vec3 boxMin, vec3 boxMax, const CloudVolumeParams params)
{
    vec3 dirToLight = (u_PushConstant.DirectionalLightDir);
    float dstInsideBox = RayBoxDst(boxMin, boxMax, rayPos, 1.0 / dirToLight).y;

    float stepSize = dstInsideBox/numStepsLight;
    float totalDensity = 0;

    for (int step = 0; step < numStepsLight; step ++)
    {
        rayPos += dirToLight * stepSize;
        float cloudDensity = SampleCloudDensity(rayPos, boxMin, boxMax, params, vec3(0.0));
        totalDensity += max(0, cloudDensity * stepSize);
    }

    float transmittance = exp(-totalDensity * params.SunAbsorption);
    return darknessThreshold + transmittance * (1-darknessThreshold);
}

// Henyey-Greenstein
float HG(float a, float g)
{
    float g2 = g*g;
    return (1-g2) / (4*3.1415*pow(1+g2-2*g*(a), 1.5));
}

float Phase(float cosTheta, float phaseValue)
{
    /*
        material.SetVector ("phaseParams", new Vector4 (forwardScattering, backScattering, baseBrightness, phaseFactor));

        public float forwardScattering = .83f;
        [Range (0, 1)]
        public float backScattering = .3f;
        [Range (0, 1)]
        public float baseBrightness = .8f;
        [Range (0, 1)]
        public float phaseFactor = .15f;
    */
    
    vec4 phaseParams = { 0.83f, 0.3f, 0.8f, 0.15f };
    float blend = 0.5;
    float hgBlend = HG(cosTheta,phaseParams.x) * (1-blend) + HG(cosTheta,-phaseParams.y) * blend;
    return phaseParams.z + hgBlend * phaseValue;
}

void main()
{
    ivec2 invoke = ivec2(gl_GlobalInvocationID.xy);

    vec2 texCoords = (vec2(invoke) + vec2(0.5f)) / vec2(textureSize(u_DepthBuffer, 0).xy / 2.0);
    float depth = textureLod(u_DepthBuffer, texCoords, 0).r;


    vec3 worldPos = DecodePosition(texCoords, depth);
    vec3 viewVector = normalize(worldPos - u_PushConstant.CameraPosition);
    float linearDepth = depth * length(worldPos - u_PushConstant.CameraPosition);


    // https://github.com/SebLague/Clouds/blob/fcc997c40d36c7bedf95a294cd2136b8c5127009/Assets/Scripts/Clouds/Shaders/Clouds.shader#L281
    // https://github.com/SebLague/Clouds/blob/fcc997c40d36c7bedf95a294cd2136b8c5127009/Assets/Scripts/Clouds/CloudMaster.cs
    // https://youtu.be/4QOcCGI6xOU?t=383

    vec4 noise = SampleBlueNoise(invoke);


    const float numSteps = 12.0;
    float transmittance = 1.0f;
    float lightEnergy = 0.0f;
    vec3 scatteringIntegral = vec3(0.0);

    CloudVolumeParams cloudParams;
    for(uint i = 0; i < u_PushConstant.CloudsCount; i++)
    {
        cloudParams = u_CloudVolumeData.Volumes[i];

        vec3 boxPosition = cloudParams.Position;
        vec3 localScale = cloudParams.Scale;

	    vec3 boundsMin = boxPosition - localScale / 2.0;
	    vec3 boundsMax = boxPosition + localScale / 2.0;

        vec2 rayToContainerInfo = RayBoxDst(boundsMin, boundsMax, u_PushConstant.CameraPosition, 1.0 / viewVector);
        float dstToBox = rayToContainerInfo.x;
        float dstInsideBox = rayToContainerInfo.y;


        vec3 rayOrigin = u_PushConstant.CameraPosition;
        vec3 rayDir = viewVector;
    
        float dstTravelled = 0.0;
        
        float stepSize = dstInsideBox / numSteps;
        float dstLimit = min(linearDepth - dstToBox, dstInsideBox);

        // Phase function makes clouds brighter around sun
        float cosAngle = dot(rayDir, u_PushConstant.DirectionalLightDir);
        float phaseValue = Phase(cosAngle, cloudParams.PhaseFunction);

	    //float phaseValue = GetMiePhase(
		//    dot(normalize(rayDir), u_PushConstant.DirectionalLightDir),
		//    cloudParams.PhaseFunction
	    //);

        vec3 scatteringValue = cloudParams.Scattering;

        while(dstTravelled < dstLimit)
        {
            vec3 rayPos = rayOrigin + rayDir * (dstToBox + dstTravelled);
            
            float density = SampleCloudDensity(rayPos, boundsMin, boundsMax, cloudParams, noise.rgb);

            if(density > 0.0)
            {
                float lightTransmittance = LightMarch(rayPos, worldPos, boundsMin, boundsMax, cloudParams);
                
                scatteringIntegral += scatteringValue * density * stepSize * transmittance * lightTransmittance * phaseValue;

                transmittance *= min(exp(-density * stepSize * cloudParams.CloudAbsorption), 1.0);

                // Exit early if T is close to zero as further samples won't affect the result much
                if(transmittance < 0.01) break;
            }

            dstTravelled += stepSize;
        }
    
    }

    imageStore(u_CloudTexture, invoke, vec4(vec3(scatteringIntegral), transmittance));
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




// OLD CODE
/*
vec4 low_frequency_noise = textureLod(u_CloudNoiseTex, rayPos * 0.01 * cloudScale, lod);
float lowFreqFBM = dot(low_frequency_noise.gba, vec3(0.625, 0.25, 0.125));
float base_cloud = remap(low_frequency_noise.r, -(1.0 - lowFreqFBM), 1.0, 0.0 , 1.0);
    
// Densitry for height (TODO) L:268
//base_cloud *= 5.0;


float cloud_coverage = 2.0f;
float base_cloud_with_coverage = remap(base_cloud, cloud_coverage, 1.0, 0.0, 1.0);
base_cloud_with_coverage *= cloud_coverage;

if(expensive)
{
    vec3 erodeCloudNoise = textureLod(u_ErroderTex, rayPos, lod).rgb;
    float highFreqFBM = dot(erodeCloudNoise.rgb, vec3(0.625, 0.25, 0.125));

    float heightFraction = 1.0;
    float highFreqNoiseModifier = mix(highFreqFBM, 1.0 - highFreqFBM, clamp(heightFraction * 10.0, 0.0, 1.0));

    base_cloud_with_coverage = base_cloud_with_coverage - highFreqNoiseModifier * (1.0 - base_cloud_with_coverage);

    base_cloud_with_coverage = remap(base_cloud_with_coverage*2.0, highFreqNoiseModifier * 0.2, 1.0, 0.0, 1.0);
}

return low_frequency_noise.r;
return clamp(base_cloud_with_coverage, 0.0, 1.0);

*/
