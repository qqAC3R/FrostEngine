#type compute
#version 460
#extension GL_EXT_nonuniform_qualifier : enable

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(binding = 0) uniform sampler2D u_ViewPositionTex;
layout(binding = 1) uniform sampler2D u_DepthPyramid;
layout(binding = 2) uniform sampler2D u_NormalsTex;
layout(binding = 3) uniform sampler2D u_NoiseTex;

layout(binding = 4) uniform writeonly image2D o_AOTexture;

layout(push_constant) uniform PushConstant
{
	mat4 ViewMatrix;
	mat4 InvProjMatrix;

    // vec4:                x        ||     y,z   
	vec3 AO_Data; // ProjectionScale || ScreenSize
	int AO_Mode;

	int FrameIndex;
	float CameraFOV;

} u_PushConstant;

#define PI				3.1415926535897932
#define TWO_PI			6.2831853071795864
#define HALF_PI			1.5707963267948966
#define ONE_OVER_PI		0.3183098861837906

#define NUM_STEPS		8
#define RADIUS			2.0		// in world space

#define NUM_MIP_LEVELS		5
#define PREFETCH_CACHE_SIZE	8
#define saturate(x)             clamp(x, 0.0, 1.0)

// Unroll all loops for performance - this is important
#pragma optionNV(unroll all)


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

vec3 CalcViewZ(float depth, vec2 uv)
{
	vec4 clipSpacePosition = vec4(uv * 2 - 1.0f, depth, 1.0);
	vec4 viewSpacePosition = u_PushConstant.InvProjMatrix * clipSpacePosition;

	// Perspective division
    viewSpacePosition /= viewSpacePosition.w;

	return vec3(viewSpacePosition);
}

vec4 GetViewPosition(vec2 uv, float currStep)
{
	int mipLevel = clamp(int(floor(log2(currStep / PREFETCH_CACHE_SIZE))), 1, NUM_MIP_LEVELS - 1);
	
	vec2 baseSize = vec2(textureSize(u_DepthPyramid, 1));
	vec2 mipCoord = (uv / baseSize);
	
	float depth = textureLod(u_DepthPyramid, mipCoord, mipLevel).r;

	vec4 ret = vec4(0.0f);
	ret.xyz = CalcViewZ(depth, mipCoord);

	return ret;
}

vec4 GetViewPositionWithUV(vec2 uv, float currStep)
{
	int mipLevel = clamp(int(floor(log2(currStep / PREFETCH_CACHE_SIZE))), 1, NUM_MIP_LEVELS - 1);
	
	vec2 baseSize = vec2(textureSize(u_DepthPyramid, 1));
	
	float depth = textureLod(u_DepthPyramid, uv, mipLevel).r;

	vec4 ret = vec4(0.0f);
	ret.xyz = CalcViewZ(depth, uv);

	return ret;
}

// max absolute error 9.0x10^-3
// Eberly's polynomial degree 1 - respect bounds
// 4 VGPR, 12 FR (8 FR, 1 QR), 1 scalar
// input [-1, 1] and output [0, PI]
#define kPI 3.141592653589793
float acosFast(float inX) 
{
    float x = abs(inX);
    float res = -0.156583f * x + (0.5 * kPI);
    res *= sqrt(1.0f - x);
    return (inX >= 0) ? res : kPI - res;
}

// Relative error : ~3.4% over full
// Precise format : ~small float
// 2 ALU
float rsqrtFast(float x)
{
	int i = floatBitsToInt(x);
	i = 0x5f3759df - (i >> 1);
	return intBitsToFloat (i);
}
uvec2 remap8x8(uint lane) // gl_LocalInvocationIndex in 8x8 threadgroup.
{
    return uvec2(
        (((lane >> 2) & 0x0007) & 0xFFFE) | lane & 0x0001,
        ((lane >> 1) & 0x0003) | (((lane >> 3) & 0x0007) & 0xFFFC)
    );
}

float Falloff(float dist2)
{
#define FALLOFF_START2	0.16
#define FALLOFF_END2	4.0
	return 2.0 * clamp((dist2 - FALLOFF_START2) / (FALLOFF_END2 - FALLOFF_START2), 0.0, 1.0);
}


float interleavedGradientNoise(vec2 iPos)
{
	return fract(52.9829189f * fract((iPos.x * 0.06711056) + (iPos.y * 0.00583715)));
}

const float kRots[6] = { 60.0f, 300.0f, 180.0f, 240.0f, 120.0f, 0.0f };
const float kOffsets[4] = { 0.1f, 0.6f, 0.35f, 0.85f };


vec3 GetRandomVector(uvec2 iPos)
{
	iPos.y = 16384 - iPos.y;

	//float noise	= texelFetch(u_NoiseTex, iPos % 4, 0).r;

    float temporalAngle = kRots[u_PushConstant.FrameIndex % 6] * (kPI / 360.0);
	float temporalCos = cos(temporalAngle);
	float temporalSin = sin(temporalAngle);

	float gradientNoise = interleavedGradientNoise(vec2(iPos));

    vec2 randomTexVec = vec2(0);
	randomTexVec.x = cos(gradientNoise * kPI);
	randomTexVec.y = sin(gradientNoise * kPI);

	float scaleOffset = (1.0 / 4.0) * ((iPos.y - iPos.x) & 3);

    vec3 randomVec = vec3(0);
	randomVec.x = dot(randomTexVec.xy, vec2(temporalCos, -temporalSin));
    randomVec.y = dot(randomTexVec.xy, vec2(temporalSin,  temporalCos));
	randomVec.z = fract(scaleOffset + kOffsets[(u_PushConstant.FrameIndex / 6) % 4] * 0.25);

	return randomVec;
}

float ComputeGTAO(vec3 viewSpacePos, vec3 viewSpaceNormal, vec3 viewDir)
{
	ivec2 outputSize = imageSize(o_AOTexture);
	const vec2 texelSize = 1.0 / vec2(outputSize);

	ivec2 loc = ivec2(gl_GlobalInvocationID.xy);
	vec2 uv = (vec2(loc) + 0.5.xx) / vec2(outputSize);


	uvec2 groupThreadId = remap8x8(gl_LocalInvocationIndex);
    uvec2 dispatchId = groupThreadId + gl_WorkGroupID.xy * 8;
    ivec2 workPos = ivec2(dispatchId);

	// GTAO Settings
	const uint kGTAOSliceNum = 2;
    const uint kGTAOStepNum = 8;
    
	const float kGTAORadius = 3.5;
    const float kGTAOThickness = 2.0;

	const float kGTAOMaxPixelScreenRadius = 21.0f; // Max offset 256 pixel.


	// Compute screen space step lenght, using simple similar triangles math.
    // Same tech detail from HBAO.
	float pixelRadius = (u_PushConstant.AO_Data.x * kGTAORadius) / (-viewSpacePos.z);
	

	pixelRadius = min(pixelRadius, float(kGTAOMaxPixelScreenRadius)); // Max pixel search radius is 256 pixel, avoid large search step when view is close.
    pixelRadius = max(pixelRadius, float(kGTAOStepNum)); // At least step one pixel.
    
	const float stepRadius = pixelRadius / (float(kGTAOStepNum) + 1.0); // Divide by steps + 1 so that the farthest samples are not fully attenuated
	const float attenFactor = 1.0 / max(kGTAORadius * kGTAORadius, 0.001);



	float occlusion = 0.0;
    vec3 bounceColor = vec3(0.0);
    {
		vec3 randomAndOffset = GetRandomVector(ivec2(gl_GlobalInvocationID.xy));

		vec2 sliceDir = randomAndOffset.xy;
        float offset = randomAndOffset.z;

		// Unreal magic code, rotate by step sin/cos.
        float stepAngle = kPI / float(kGTAOSliceNum);
        float sinDeltaAngle	= sin(stepAngle);
	    float cosDeltaAngle	= cos(stepAngle);

		for(uint i = 0; i < kGTAOSliceNum; i++) 
        {
			vec2 sliceDirection = sliceDir * texelSize; // slice direction in texel uint.
			
			// Horizontal search best angle for this direction.
            vec2 bestAng = vec2(-1.0, -1.0);
            
			for(int j = 1; j < kGTAOStepNum + 1; j++)
            {
				float fi = float(j);
                
				// stepRadius > 1.0, and jitterStep is range [0, 1]
                vec2 uvOffset = sliceDirection * max(stepRadius * (fi + offset), fi + 1.0);


				// build two conversely sample direction.
                uvOffset.y *= -1.0;
                vec4 uv2 = uv.xyxy + vec4(uvOffset.xy, -uvOffset.xy);

				// Use hzb to imporve L2 cache hit rate.
                float mipLevel = 0.0;
                if(j == 2)
                    mipLevel ++;
                if(j > 3)
                    mipLevel += 2;

				vec3 h1 = GetViewPositionWithUV(uv2.xy, mipLevel).xyz - viewSpacePos; // H1 is positive direction.
                vec3 h2 = GetViewPositionWithUV(uv2.zw, mipLevel).xyz - viewSpacePos; // H2 is negative direction.



				float h1LenSq = dot(h1, h1);
				float falloffH1 = saturate(h1LenSq * attenFactor);
				if(falloffH1 < 1.0)
                {
					//float angH1 = dot(h1, viewDir) * rsqrtFast(h1LenSq + 0.0001);
                    //angH1 = mix(angH1, bestAng.x, falloffH1);
                    //bestAng.x = (angH1 > bestAng.x) ? angH1 : mix(angH1, bestAng.x, kGTAOThickness);

					// Length of the vector
					float dist = length(h1);
					float invdist = 1.0 / dist;

					// Compute sample's cosine
					float cosh = invdist  * dot(h1, viewDir);
					float falloff = Falloff(dist);

					bestAng.x = max(bestAng.x, cosh - falloff);
				}

				float h2LenSq = dot(h2, h2);
                float falloffH2 = saturate(h2LenSq * attenFactor);
                if(falloffH2 < 1.0)
                {
                    //float angH2 = dot(h2, viewDir) * rsqrtFast(h2LenSq + 0.0001);
                    //angH2 = mix(angH2, bestAng.y, falloffH2);
                    //bestAng.y = (angH2 > bestAng.y) ? angH2 : mix(angH2, bestAng.y, kGTAOThickness);


					// Length of the vector
					float dist = length(h2);
					float invdist = 1.0 / dist;

					// Compute sample's cosine
					float cosh = invdist  * dot(h2, viewDir);
					float falloff = Falloff(dist);

					bestAng.y = max(bestAng.y, cosh - falloff);
                }
			}

			bestAng.x = acosFast(clamp(bestAng.x, -1.0, 1.0));
            bestAng.y = acosFast(clamp(bestAng.y, -1.0, 1.0));
            

			// Compute inner integral.
            {
                // Given the angles found in the search plane we need to project the View Space Normal onto the plane defined by the search axis and the View Direction and perform the inner integrate
                vec3 planeNormal = normalize(cross(vec3(sliceDir, 0.0), viewDir));
                vec3 perp = cross(viewDir, planeNormal);
                vec3 projNormal = viewSpaceNormal - planeNormal * dot(viewSpaceNormal, planeNormal);

                float lenProjNormal = length(projNormal) + 0.000001;
                float recipMag = 1.0 / lenProjNormal;

                float cosAng = dot(projNormal, perp) * recipMag;	
	            float gamma = acosFast(cosAng) - 0.5 * kPI;				
	            float cosGamma = dot(projNormal, viewDir) * recipMag;
	            float sinGamma = cosAng * (-2.0);					

	            // clamp to normal hemisphere 
	            bestAng.x = gamma + max(-bestAng.x - gamma, -(0.5 * kPI));
	            bestAng.y = gamma + min( bestAng.y - gamma,  (0.5 * kPI));

                // See Activision GTAO paper: https://www.activision.com/cdn/research/s2016_pbs_activision_occlusion.pptx
                // Integrate arcCos weight.
	            float ao = ((lenProjNormal) * 0.25 * 
					((bestAng.x * sinGamma + cosGamma - cos(2.0 * bestAng.x - gamma)) +
				  	 (bestAng.y * sinGamma + cosGamma - cos(2.0 * bestAng.y - gamma))));

                occlusion += ao;
            }

			// Unreal magic code, rotate by sin/cos step.
            // Rotate for the next angle
            vec2 tempScreenDir = sliceDir.xy;
            sliceDir.x = (tempScreenDir.x * cosDeltaAngle) + (tempScreenDir.y * -sinDeltaAngle);
            sliceDir.y = (tempScreenDir.x * sinDeltaAngle) + (tempScreenDir.y *  cosDeltaAngle);
            offset = fract(offset + 0.617);

		}


		occlusion = occlusion / float(kGTAOSliceNum);
        occlusion *= 2.0 / kPI;

	}

	
	return occlusion;
}


float Falloff2(float DistanceSquare)
{
	// 1 scalar mad instruction
	float R = 2.0f;
	float R2 = R*R;
	float negInvR2 = -1.0f / R2;
	return DistanceSquare * negInvR2 + 1.0;
}
//----------------------------------------------------------------------------------
// P = view-space position at the kernel center
// N = view-space normal at the kernel center
// S = view-space position of the current sample
//----------------------------------------------------------------------------------
float ComputeAO(vec3 P, vec3 N, vec3 S)
{
	vec3 V = S - P;
	float VdotV = dot(V, V);
	float NdotV = dot(N, V) * 1.0/sqrt(VdotV);

	float NDotVBias = 0.1f;

	

	// Use saturate(x) instead of max(x,0.f) because that is faster on Kepler
	return clamp(NdotV - NDotVBias, 0, 1) * clamp(Falloff2(VdotV),0,1);
}

float ComputeHBAO(vec3 vpos, vec3 vnorm, vec3 vdir)
{
	ivec2 loc = ivec2(gl_GlobalInvocationID.xy);

	float ao = 0.0f;

	vec4 s;
	vec3 dir, ws;

	vec2 noises	= texelFetch(u_NoiseTex, loc % 4, 0).rg;
	vec2 offset;

	float radius = (RADIUS * u_PushConstant.AO_Data.x) / (1.0 - vpos.z);
	radius = max(float(NUM_STEPS), radius);
		
	float stepSize	= radius / NUM_STEPS;
	float phi		= noises.x * PI;
	float division	= noises.y * stepSize;
	float currStep	= 1.0 + division;


	float cosh, falloff, dist, invdist;
	vec2 horizons = vec2(-1.0, -1.0);

	dir = vec3(cos(phi), sin(phi), 0.0);


	// calculate horizon angles
	for (int j = 0; j < NUM_STEPS; ++j)
	{
		offset = round(dir.xy * currStep);
			
		// h1
		{
			s = GetViewPosition(vec2(gl_GlobalInvocationID.xy) + offset, currStep);
			ao += ComputeAO(vpos.xyz, vnorm.xyz, s.xyz);


			ws = (s.xyz - vpos.xyz);

			// Length of the vector
			dist = length(ws);
			invdist = 1.0 / dist;

			// Compute sample's cosine
			cosh = invdist  * dot(ws, vdir);
			falloff = Falloff(dist);

			horizons.x = max(horizons.x, cosh - falloff);
		}


		// h2
		{
			s = GetViewPosition(vec2(gl_GlobalInvocationID.xy) - offset, currStep);
			ao += ComputeAO(vpos.xyz, vnorm.xyz, s.xyz);
			
			ws = (s.xyz - vpos.xyz);

			// Length of the vector
			dist = length(ws);
			invdist = 1.0 / dist;

			// Compute sample's cosine
			cosh = invdist  * dot(ws, vdir);
			falloff = Falloff(dist);

			horizons.y = max(horizons.y, cosh - falloff);
		}

		// increment
		currStep += stepSize;
	}


	float aoIntensity = 2.0;
	ao *= aoIntensity / float(NUM_STEPS);
	ao = (1.0 - ao);

	return ao;
}

#define HBAO_MODE 0
#define GTAO_MODE 1

float ComputeAO(vec3 vpos, vec3 vnorm, vec3 vdir, int aoMode)
{
	float ao = 0.0f;
	
	//ao = ComputeHBAO(vpos, vnorm, vdir);
	switch(aoMode)
	{
		case HBAO_MODE: ao = ComputeHBAO(vpos, vnorm, vdir); break;
		case GTAO_MODE: ao = ComputeGTAO(vpos, vnorm, vdir); break;
	}

	return ao;
}

void main()
{
	ivec2 loc = ivec2(gl_GlobalInvocationID.xy);
	ivec2 imgSize = imageSize(o_AOTexture).xy;
	vec2 s_UV = (vec2(loc) + 0.5.xx) / vec2(imgSize);
	vec4 vpos = texture(u_ViewPositionTex, s_UV);

	if(vpos.x == 0.0f && vpos.y == 0.0f && vpos.z == 0.0f)
	{
		imageStore(o_AOTexture, ivec2(gl_GlobalInvocationID.xy), vec4(vec3(1.0f), 1.0f));
		return;
	}

	vec3 world_norm = normalize(DecodeNormal(texture(u_NormalsTex, s_UV).rg));
	vec3 vnorm = normalize(transpose(inverse(mat3(u_PushConstant.ViewMatrix))) * world_norm);
	vec3 vdir = normalize(-vpos.xyz);

	float ao = ComputeAO(vpos.xyz, vnorm.xyz, vdir.xyz, u_PushConstant.AO_Mode);
	ao = clamp(ao, 0.0f, 1.0f);

	imageStore(o_AOTexture, ivec2(gl_GlobalInvocationID.xy), vec4(vec3(ao), 1.0f));
}