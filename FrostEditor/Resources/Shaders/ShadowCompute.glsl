#type compute
#version 460 

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

#pragma optionNV(unroll all)

layout(binding = 0) uniform sampler2D u_ShadowDepthTexture;
layout(binding = 1) uniform sampler2D u_DepthBuffer;
layout(binding = 2) uniform sampler2D u_ViewPositionTexture;
layout(binding = 3) uniform sampler2D u_NormalTexture;
layout(binding = 4, rgba8) uniform restrict writeonly image2D u_ShadowTextureOutput;

layout(push_constant) uniform PushConstant
{
	mat4 InvViewProjection;
	vec4 CascadeDepthSplit;
	float NearCameraClip;
	float FarCameraClip;
} u_PushConstant;

layout(binding = 5) uniform DirectionaLightData
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

#define SHADOW_MAP_CASCADE_COUNT 4
vec2 s_UV;
float GLOBAL_BIAS = 0.002;
float delta = 0.0;
const float PI = 3.141592;


vec2 SamplePoisson(int index);

mat4 GetCascadeMatrix(uint cascadeIndex);
uint GetCascadeIndex(float viewPosZ);

vec2 ComputeShadowCoord(vec2 coord, uint cascadeIndex);
float SampleShadowMap(vec2 shadowCoords, uint cascadeIndex);
vec4 SampleShadowMap_Gather(vec2 shadowCoords, uint cascadeIndex);
float AverageBlockDepth(vec4 projCoords, uint cascadeIndex, float w_light);

float HardShadows_SampleShadowTexture(vec4 shadowCoord, uint cascadeIndex)
{
	float shadow = 1.0;
	float bias = GLOBAL_BIAS;

	if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 )
	{
		float dist = SampleShadowMap(shadowCoord.xy, cascadeIndex);

		if (shadowCoord.w >  0 && dist < shadowCoord.z - bias)
		{
			shadow = 0.0;
		}
	}
	return shadow;
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




// Interleaved gradient noise from:
// http://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare
float InterleavedGradientNoise(vec2 uvs)
{
  const vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
  return fract(magic.z * fract(dot(uvs, magic.xy)));
}

// Create a rotation matrix with the noise function above.
mat2 RandomRotation(vec2 uvs)
{
	float theta = 2.0 * PI * InterleavedGradientNoise(uvs);
	float sinTheta = sin(theta);
	float cosTheta = cos(theta);
	return mat2(cosTheta, sinTheta, -sinTheta, cosTheta);
}

float SearchRegionRadiusUV(float zWorld)
{
	const float light_zNear = 0.0;
	const float lightRadiusUV = 0.05;
	return lightRadiusUV * (zWorld - light_zNear) / zWorld;
}

int GetRequiredPCFSamplesForCascade(uint cascadeIndex)
{
	switch(cascadeIndex)
	{
		case 1: return 8;
		case 2: return 6;
		case 3: return 4;
		case 4: return 4;
	}
	return 0;
}

int GetRequiredBlockerSamplesForCascade(uint cascadeIndex)
{
	switch(cascadeIndex)
	{
		case 1: return 48;
		case 2: return 24;
		case 3: return 10;
		case 4: return 6;
	}
	return 0;
}

// Depth Aware Contact harden pcf. See GDC2021: "Shadows of Cold War" for tech detail.
// Use cache occluder dist to fit one curve similar to tonemapper, to get some effect like pcss.
// can reduce tiny acne natively.
float contactHardenPCFKernal(
    const float occluders, 
    const float occluderDistSum, 
    const float compareDepth,
    const uint shadowSampleCount)
{
    // Normalize occluder dist.
    float occluderAvgDist = occluderDistSum / occluders;

//#if SHADOW_DEPTH_GATHER
    float w = 1.0 / float(4 * shadowSampleCount); // We gather 4 pixels.
//#else
//    float w = 1.0f / (1 * shadowSampleCount); 
//#endif
    
    float pcfWeight = clamp(occluderAvgDist / compareDepth, 0.0, 1.0);
    
    // Normalize occluders.
    float percentageOccluded = clamp(occluders * w, 0.0, 1.0);

    // S curve fit.
    percentageOccluded = 2.0 * percentageOccluded - 1.0;
    float occludedSign = sign(percentageOccluded);
    percentageOccluded = 1.0 - (occludedSign * percentageOccluded);
    percentageOccluded = mix(percentageOccluded * percentageOccluded * percentageOccluded, percentageOccluded, pcfWeight);
    percentageOccluded = 1.0 - percentageOccluded;
    percentageOccluded *= occludedSign;
    percentageOccluded = 0.5 * percentageOccluded + 0.5;

#define SHADOW_COLOR 1

    return percentageOccluded * SHADOW_COLOR;
}


// Auto bias by cacsade and NoL, some magic number here.
float AutoBias(float NoL, float biasMul)
{
    return 1e-3f + (1.0f - NoL) * biasMul * 2e-3f;
}

float NewPCF(vec4 shadowCoords, uint cascadeIndex, float lightSize)
{
	// TODO: Add DPCF
	// https://github.com/google/filament/blob/main/shaders/src/shadowing.fs#L257
	// https://github.com/google/filament/blob/main/shaders/src/shadowing.fs#L257


	float bias = GLOBAL_BIAS;
	
	float compareDepth = shadowCoords.z + bias;

	int numPCFSamples = 64;
	//int requiredPCFSamples = GetRequiredPCFSamplesForCascade(cascadeIndex);
	int requiredPCFSamples = 12;
	int additionFactor = numPCFSamples / requiredPCFSamples;

	mat2 rot = RandomRotation(vec2(gl_GlobalInvocationID.xy));
	vec2 texelSize = 1.0 / (textureSize(u_ShadowDepthTexture, 0) / 2.0);

	float occluders = 0.0;
	float occluderDistSum = 0.0;

	//const float NEAR = 0.001;
	//float uvRadius = lightSize * NEAR * 2.0;

	//float blockerDistance = AverageBlockDepth(shadowCoords, cascadeIndex, lightSize);
	float receiverDepth = shadowCoords.z;
	float penumbraWidth = pow(0.5, float(cascadeIndex));
	//float penumbraWidth = 0.1;
	const float NEAR = 0.01;
	float uvRadius = penumbraWidth * lightSize * NEAR / receiverDepth;



	float sum = 0.0;
	for (int i = 0; i < numPCFSamples; i += additionFactor)
	{
		vec2 offset;
		offset = (rot) * SamplePoisson(i) * uvRadius;

		
		//float z = SampleShadowMap((shadowCoords.xy) + offset, cascadeIndex);
		//{
		//	float dist = z - compareDepth;
		//	float occluder = step(0.0, dist); // reverse z.
		//
		//	// Collect occluders.
		//	occluders += occluder;
		//	occluderDistSum += dist * occluder;
		//}

		vec4 depths = SampleShadowMap_Gather((shadowCoords.xy) + offset, cascadeIndex);
		//for (uint j = 0; j < 4; j++)
		//{
		//	float dist = depths[j] - compareDepth;
		//	float occluder = step(0.0, dist); // reverse z.
		//
		//	// Collect occluders.
		//	occluders += occluder;
		//	occluderDistSum += dist * occluder;
		//}

		float z = (depths.x + depths.y + depths.z + depths.w) / 4.0;

		sum += step(shadowCoords.z - bias, z);

	}
	return sum / float(requiredPCFSamples);
	//return contactHardenPCFKernal(occluders, occluderDistSum, compareDepth, requiredPCFSamples);
}




// Poisson disk generated with 'poisson-disk-generator' tool from
// https://github.com/corporateshark/poisson-disk-generator by Sergey Kosarevsky
/*const*/ mediump vec2 poissonDisk[64] = vec2[]( // don't use 'const' b/c of OSX GL compiler bug
    vec2(0.511749, 0.547686), vec2(0.58929, 0.257224), vec2(0.165018, 0.57663), vec2(0.407692, 0.742285),
    vec2(0.707012, 0.646523), vec2(0.31463, 0.466825), vec2(0.801257, 0.485186), vec2(0.418136, 0.146517),
    vec2(0.579889, 0.0368284), vec2(0.79801, 0.140114), vec2(-0.0413185, 0.371455), vec2(-0.0529108, 0.627352),
    vec2(0.0821375, 0.882071), vec2(0.17308, 0.301207), vec2(-0.120452, 0.867216), vec2(0.371096, 0.916454),
    vec2(-0.178381, 0.146101), vec2(-0.276489, 0.550525), vec2(0.12542, 0.126643), vec2(-0.296654, 0.286879),
    vec2(0.261744, -0.00604975), vec2(-0.213417, 0.715776), vec2(0.425684, -0.153211), vec2(-0.480054, 0.321357),
    vec2(-0.0717878, -0.0250567), vec2(-0.328775, -0.169666), vec2(-0.394923, 0.130802), vec2(-0.553681, -0.176777),
    vec2(-0.722615, 0.120616), vec2(-0.693065, 0.309017), vec2(0.603193, 0.791471), vec2(-0.0754941, -0.297988),
    vec2(0.109303, -0.156472), vec2(0.260605, -0.280111), vec2(0.129731, -0.487954), vec2(-0.537315, 0.520494),
    vec2(-0.42758, 0.800607), vec2(0.77309, -0.0728102), vec2(0.908777, 0.328356), vec2(0.985341, 0.0759158),
    vec2(0.947536, -0.11837), vec2(-0.103315, -0.610747), vec2(0.337171, -0.584), vec2(0.210919, -0.720055),
    vec2(0.41894, -0.36769), vec2(-0.254228, -0.49368), vec2(-0.428562, -0.404037), vec2(-0.831732, -0.189615),
    vec2(-0.922642, 0.0888026), vec2(-0.865914, 0.427795), vec2(0.706117, -0.311662), vec2(0.545465, -0.520942),
    vec2(-0.695738, 0.664492), vec2(0.389421, -0.899007), vec2(0.48842, -0.708054), vec2(0.760298, -0.62735),
    vec2(-0.390788, -0.707388), vec2(-0.591046, -0.686721), vec2(-0.769903, -0.413775), vec2(-0.604457, -0.502571),
    vec2(-0.557234, 0.00451362), vec2(0.147572, -0.924353), vec2(-0.0662488, -0.892081), vec2(0.863832, -0.407206)
);

void blockerSearchAndFilter(vec4 shadowPosition, uint cascadeIndex, const mat2 randomRotation, vec2 filterRadii, float shadowBulbRadius, const uint tapCount, out float occludedCount, out float z_occSum)
{
    occludedCount = 0.0;
    z_occSum = 0.0;

    for (uint i = 0; i < tapCount; i++)
	{
        vec2 duv = randomRotation * (poissonDisk[i] * filterRadii);
		vec2 uv = shadowPosition.xy;
        vec2 tc = uv + duv;

        //float z_occ = SampleShadowMap(tc, cascadeIndex);
        vec4 z_occs = SampleShadowMap_Gather(tc, cascadeIndex);
		float z_occ = (z_occs.x + z_occs.y + z_occs.z + z_occs.w) / 4.0;
		float z_rec = shadowPosition.z;

        // note: z_occ and z_rec are not necessarily linear here, comparing them is always okay for
        // the regular PCF, but the "distance" is meaningless unless they are actually linear
        // (e.g.: for the directional light).
        // Either way, if we assume that all the samples are close to each other we can take their
        // average regardless, and the average depth value of the occluders
        // becomes: z_occSum / occludedCount.

        // receiver plane depth bias
        float z_bias = 0.003;
		float dz = z_rec - z_occ;
        float occluded = step(z_bias, dz);
        occludedCount += occluded;
        z_occSum += z_occ * occluded;
    }
}

// ----------------------------------------------------------------------------
float AverageBlockDepth(vec4 projCoords, uint cascadeIndex, float w_light)
{
    float blockerSum = 0.0;
    int blockerCount = 0;
    
    vec2 texelSize = 1.0 / (textureSize(u_ShadowDepthTexture, 0) / 2.0);
    //vec2 texelSize = 1.0 / (textureSize(u_ShadowDepthTexture, 0));
    
    float currentDepth = projCoords.z;
    
    float search_range = w_light * (currentDepth - 0.05) / currentDepth;
    if(search_range <= 0.0)
        return -1.0;

    //int range = int(search_range);
    
    int window = 3;
    for(int i = -window; i < window; i++)
	{
        for(int j = -window; j < window; j++)
		{
            vec2 shift = vec2(float(i) * 1.0 * search_range / float(window), float(j) * 1.0 * search_range / float(window));
            
			float sampleDepth = SampleShadowMap(projCoords.xy + shift * texelSize, uint(cascadeIndex));

            if (sampleDepth < currentDepth)
			{
                blockerSum += sampleDepth;
                blockerCount++;
            }
        }
    }
    
    if(blockerCount > 0)
        return blockerSum / float(blockerCount);
    else
        return -1.0; //--> not in shadow~~~~
}

float FindBlockerDistance(vec4 shadowCoords, uint cascadeIndex, float lightSize)
{
	float bias = GLOBAL_BIAS;

	int numBlockerSearchSamples = GetRequiredBlockerSamplesForCascade(cascadeIndex);
	int blockers = 0;
	float avgBlockerDistance = 0.0;

	float searchWidth = SearchRegionRadiusUV(shadowCoords.z);
	for (int i = 0; i < numBlockerSearchSamples; i++)
	{
		float z = SampleShadowMap((shadowCoords.xy) + SamplePoisson(i) * searchWidth, cascadeIndex);

		if (z < (shadowCoords.z - bias))
		{
			blockers++;
			avgBlockerDistance += z;
		}
	}

	if (blockers > 0)
		return avgBlockerDistance / float(blockers);

	return -1.0;
}

float PCFDirectionalLight(vec4 shadowCoords, uint cascadeIndex, float uvRadius, float blockerDistance)
{
	float bias = GLOBAL_BIAS;
	//int numPCFSamples = u_DirLightData.UsePCSS == 1 ? 64 : GetRequiredPCFSamplesForCascade(cascadeIndex);
	int numPCFSamples = 64;
	int requiredPCFSamples = GetRequiredPCFSamplesForCascade(cascadeIndex);
	int additionFactor = numPCFSamples / requiredPCFSamples;

	mat2 rot = RandomRotation(vec2(gl_GlobalInvocationID.xy));
	vec2 texelSize = 1.0 / (textureSize(u_ShadowDepthTexture, 0) / 2.0);

	float sum = 0.0;
	for (int i = 0; i < numPCFSamples; i += additionFactor)
	{
		vec2 offset;
		//if(cascadeIndex <= 2)
			offset = (rot * blockerDistance) * SamplePoisson(i) * uvRadius;
		//else
		//	offset = SamplePoisson(i) * uvRadius;

		
		float z = SampleShadowMap((shadowCoords.xy) + offset, cascadeIndex);
		//vec4 depths = SampleShadowMap_Gather((shadowCoords.xy) + offset, cascadeIndex);
		//for (uint j = 0; j < 4; j++)
		//{
		//	float dist = depths[j] - compareDepth;
		//	float occluder = step(0.0, dist); // reverse z.
		//
		//	// Collect occluders.
		//	occluders += occluder;
		//	occluderDistSum += dist * occluder;
		//}

		sum += step(shadowCoords.z - bias, z);
	}
	return sum / float(requiredPCFSamples);
}

float PCSS_SampleShadowTexture(vec4 shadowCoords, uint cascadeIndex, float lightSize, float customDepth)
{
	if(cascadeIndex >= 3) return PCFDirectionalLight(shadowCoords, cascadeIndex, 0.0001, 0.5);

	vec2 texelSize = vec2(1.0) / (vec2(textureSize(u_ShadowDepthTexture, 0).xy) / 2.0);

	// Custom function (https://www.desmos.com/calculator/6wqdtlndfm)
	lightSize = -3.0 * exp(-1.1 * lightSize) + 3.0;

	//float blockerDistance = FindBlockerDistance(shadowCoords, cascadeIndex, lightSize);
	//float blockerDistance = AverageBlockDepth(shadowCoords, cascadeIndex, lightSize);

	float occludedCount, occlusionSum;
	//float blockerDistance = AverageBlockDepth(shadowCoords, cascadeIndex, lightSize, x1, x2);

	mat2 rot = RandomRotation(vec2(gl_GlobalInvocationID.xy));
	blockerSearchAndFilter(
		shadowCoords, cascadeIndex, rot, texelSize * lightSize, lightSize, 12u, occludedCount, occlusionSum
	);
	float blockerDistance = occlusionSum / occludedCount;
	
	if(blockerDistance == 0.0) return 0.0; // No occlusion

	float receiverDepth = shadowCoords.z;
	float penumbraWidth = (receiverDepth - blockerDistance) / blockerDistance;
	const float NEAR = 0.01;
	float uvRadius = penumbraWidth * lightSize * NEAR / receiverDepth;

	return PCFDirectionalLight(shadowCoords, cascadeIndex, uvRadius, blockerDistance);
}

vec3 ComputeWorldPos(float depth)
{
	ivec2 invoke = ivec2(gl_GlobalInvocationID.xy);
	vec2 coords = (vec2(invoke) + 0.5.xx) / vec2(imageSize(u_ShadowTextureOutput).xy);

	vec4 clipSpacePosition = vec4(coords * 2.0 - 1.0, depth, 1.0);
	vec4 viewSpacePosition = u_PushConstant.InvViewProjection * clipSpacePosition;
	
    viewSpacePosition /= viewSpacePosition.w; // Perspective division
	return vec3(viewSpacePosition);
}



void main()
{
	ivec2 globalInvocation = ivec2(gl_GlobalInvocationID.xy);
	ivec2 imgSize = imageSize(u_ShadowTextureOutput).xy;

	if (any(greaterThanEqual(globalInvocation, imgSize)))
		return;

	float viewPosZ = texelFetch(u_ViewPositionTexture, globalInvocation, 0).z;
	uint cascadeIndex = GetCascadeIndex(viewPosZ);

	float depth = texelFetch(u_DepthBuffer, globalInvocation, 0).r; 
	vec3 position = ComputeWorldPos(depth);
	float lightSize = u_DirLightData.DirectionLightSize;

	
	// Depth compare for shadowing
	vec4 shadowCoord = ((GetCascadeMatrix(cascadeIndex))) * vec4(position, 1.0);
	shadowCoord /= shadowCoord.w;

	shadowCoord.xy = shadowCoord.xy * 0.5 + 0.5;

	float shadowFactor = 0.0f;
	/*
	if(u_DirLightData.FadeCascades == 1)
	{
		
		float cascadeTransitionFade = u_DirLightData.CascadesFadeFactor;

		float c1 = smoothstep(u_PushConstant.CascadeDepthSplit[1] + cascadeTransitionFade * 0.5f, u_PushConstant.CascadeDepthSplit[1] - cascadeTransitionFade * 0.5f, viewPosZ);
		float c2 = smoothstep(u_PushConstant.CascadeDepthSplit[2] + cascadeTransitionFade * 0.5f, u_PushConstant.CascadeDepthSplit[2] - cascadeTransitionFade * 0.5f, viewPosZ);

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
	*/
	shadowFactor = u_DirLightData.UsePCSS == 1 ? PCSS_SampleShadowTexture(shadowCoord, cascadeIndex, lightSize, 1.0) : HardShadows_SampleShadowTexture(shadowCoord, cascadeIndex);
	//shadowFactor = u_DirLightData.UsePCSS == 1 ? NewPCF(shadowCoord, cascadeIndex, lightSize) : HardShadows_SampleShadowTexture(shadowCoord, cascadeIndex);

	
	vec3 shadow = vec3(clamp(shadowFactor, 0.0f, 1.0f));

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

	imageStore(u_ShadowTextureOutput, globalInvocation, vec4(shadow, 1.0f));
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

vec2 ComputeShadowCoord(vec2 coord, uint cascadeIndex)
{
	coord = clamp(coord, 0.0f, 1.0f);
	switch(cascadeIndex)
	{
	case 1:
		return coord * vec2(0.5f);
	case 2:
		return vec2(coord.x * 0.5f + 0.5f + delta, coord.y * 0.5f);
	case 3:
		return vec2(coord.x * 0.5f, coord.y * 0.5f + 0.5f + delta);
	case 4:
		return coord * vec2(0.5f) + vec2(0.5f + delta);
	}
}

float SampleShadowMap(vec2 shadowCoords, uint cascadeIndex)
{
	vec2 coords = ComputeShadowCoord(shadowCoords.xy, cascadeIndex);
	float dist = texture(u_ShadowDepthTexture, coords).r;
	return dist;
}

vec4 SampleShadowMap_Gather(vec2 shadowCoords, uint cascadeIndex)
{
	vec2 coords = ComputeShadowCoord(shadowCoords.xy, cascadeIndex);
	vec4 depths = textureGather(u_ShadowDepthTexture, coords, 0);
	return depths;
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

uint GetCascadeIndex(float viewPosZ)
{
	uint cascadeIndex = 1;
	for(uint i = 0; i < SHADOW_MAP_CASCADE_COUNT - 1; ++i)
	{
		if(viewPosZ < u_PushConstant.CascadeDepthSplit[i])
		{	
			cascadeIndex = i + 2;
		}
	}
	return cascadeIndex;
}