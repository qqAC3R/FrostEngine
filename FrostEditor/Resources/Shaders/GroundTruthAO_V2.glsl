#type compute
#version 460
#extension GL_EXT_nonuniform_qualifier : enable

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

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

} u_PushConstant;

#define PI				3.1415926535897932
#define TWO_PI			6.2831853071795864
#define HALF_PI			1.5707963267948966
#define ONE_OVER_PI		0.3183098861837906

#define NUM_DIRECTIONS	16
#define NUM_STEPS		8
#define RADIUS			2.0		// in world space

#define NUM_MIP_LEVELS		5
#define PREFETCH_CACHE_SIZE	8

// Unroll all loops for performance - this is important
#pragma optionNV(unroll all)


// https://aras-p.info/texts/CompactNormalStorage.html
vec3 DecodeNormals(vec2 enc)
{
    float scale = 1.7777f;
    vec3 nn = vec3(enc, 0.0f) * vec3(2 * scale, 2 * scale,0) + vec3(-scale, -scale,1);
    
	float g = 2.0 / dot(nn.xyz,nn.xyz);

    vec3 n;
    n.xy = g*nn.xy;
    n.z = g-1;

	n = clamp(n, vec3(-1.0f), vec3(1.0f));

    return n;
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
	int mipLevel = clamp(int(floor(log2(currStep / PREFETCH_CACHE_SIZE))), 0, NUM_MIP_LEVELS - 1);
	
	vec2 baseSize = vec2(textureSize(u_DepthPyramid, 0));
	vec2 mipCoord = (uv / baseSize);
	
	float depth = textureLod(u_DepthPyramid, mipCoord, mipLevel).r;

	vec4 ret = vec4(0.0f);
	ret.xyz = CalcViewZ(depth, mipCoord);

	return ret;
}

float Falloff(float dist2)
{
#define FALLOFF_START2	0.16
#define FALLOFF_END2	4.0

	return 2.0 * clamp((dist2 - FALLOFF_START2) / (FALLOFF_END2 - FALLOFF_START2), 0.0, 1.0);
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

	float radius = (RADIUS * u_PushConstant.AO_Data.x) / (1.0f - vpos.z);
	radius = max(NUM_STEPS, radius);
		
	float stepSize	= radius / NUM_STEPS;
	float phi		= noises.x * PI;
	float division	= noises.y * stepSize;
	float currStep	= 1.0 + division;

	//float dist, invdist, falloff, cosh;


	dir = vec3(cos(phi), sin(phi), 0.0);


	// calculate horizon angles
	for (int j = 0; j < NUM_STEPS; ++j)
	{
		offset = round(dir.xy * currStep);
			
		// h1
		{
			s = GetViewPosition(vec2(gl_GlobalInvocationID.xy) + offset, 0);
			ws = (s.xyz - vpos.xyz);

			ao += ComputeAO(vpos.xyz, vnorm.xyz, s.xyz);
		}


		// h2
		{
			s = GetViewPosition(vec2(gl_GlobalInvocationID.xy) - offset, 0);
			ws = (s.xyz - vpos.xyz);

			ao += ComputeAO(vpos.xyz, vnorm.xyz, s.xyz);
		}

		// increment
		currStep += stepSize;
	}

	float aoIntensity = 4.0f;

	ao *= aoIntensity / (NUM_STEPS * 2);
	ao = (1.0f - ao);

	return ao;
}

float ComputeGTAO(vec3 vpos, vec3 vnorm, vec3 vdir)
{
	ivec2 loc = ivec2(gl_GlobalInvocationID.xy);

	// reinvert normals
	vnorm.x *= -1.0f;

	float ao = 0.0f;

	vec4 s;
	vec3 dir, ws;

	vec2 noises	= texelFetch(u_NoiseTex, loc % 4, 0).rg;
	vec2 offset;
	vec2 horizons = vec2(-1.0, -1.0);

	float radius = (RADIUS * u_PushConstant.AO_Data.x) / (1.0f - vpos.z);
	radius = max(NUM_STEPS, radius);
		
	float stepSize	= radius / NUM_STEPS;
	float phi		= noises.x * PI;
	float division	= noises.y * stepSize;
	float currStep	= 1.0 + division;

	float dist, invdist, falloff, cosh;


	dir = vec3(cos(phi), sin(phi), 0.0);
	horizons = vec2(-1.0);


	// calculate horizon angles
	for (int j = 0; j < NUM_STEPS; ++j)
	{
		offset = round(dir.xy * currStep);
			
		// h1
		{
			s = GetViewPosition(vec2(gl_GlobalInvocationID.xy) + offset, currStep);
			ws = (s.xyz - vpos.xyz);

			// Length of the vector
			dist = length(ws);
			invdist = 1 / dist;

			// Compute sample's cosine
			cosh = invdist  * dot(ws, vdir);

			falloff = Falloff(dist);
			horizons.x = max(horizons.x, cosh - falloff);
		}


		// h2
		{
			s = GetViewPosition(vec2(gl_GlobalInvocationID.xy) - offset, currStep);
			ws = (s.xyz - vpos.xyz);

			// Length of the vector
			dist = length(ws);
			invdist = 1 / dist;

			// Compute sample's cosine
			cosh = invdist * dot(ws, vdir);

			falloff = Falloff(dist);
			horizons.y = max(horizons.y, cosh - falloff);
		}

		// increment
		currStep += stepSize;
	}

	horizons = acos(horizons);
		

	// calculate gamma
	vec3 bitangent	= normalize(cross(vdir, dir));
	vec3 tangent	= cross(bitangent, vdir);
	vec3 nx			= vnorm - bitangent * dot(vnorm, bitangent);

	float nnx		= length(nx);
	float invnnx	= 1.0 / (nnx + 1e-6);			// to avoid division with zero
	float cosxi		= dot(nx, tangent) * invnnx;	// xi = gamma + HALF_PI
	float gamma		= acos(cosxi) - HALF_PI;
	float cosgamma	= dot(nx, vdir) * invnnx;
	float singamma2	= -2.0 * cosxi;					// cos(x + HALF_PI) = -sin(x)

	// clamp to normal hemisphere
	horizons.y = gamma + max(-horizons.y - gamma, -HALF_PI);
	horizons.x = gamma + min( horizons.x - gamma,  HALF_PI);

	// Riemann integral is additive
	ao += nnx * 0.25 * (
		(horizons.y * singamma2 + cosgamma - cos(2.0 * horizons.y - gamma)) +
		(horizons.x * singamma2 + cosgamma - cos(2.0 * horizons.x - gamma)));
			

	return ao;
}

#define HBAO_MODE 0
#define GTAO_MODE 1

float ComputeAO(vec3 vpos, vec3 vnorm, vec3 vdir, int aoMode)
{
	float ao = 0.0f;
	
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
	vec2 s_UV = vec2(loc) / u_PushConstant.AO_Data.yz;
	//vec4 vpos = GetViewPosition(loc, 0.0);
	vec4 vpos = texelFetch(u_ViewPositionTex, loc, 0);
	
	//if (vpos.w == 1.0) {
	//	imageStore(o_AOTexture, ivec2(gl_GlobalInvocationID.xy), vec4(vec3(1.0f), 1.0f));
	//	return;
	//}

	if(vpos.x == 0.0f && vpos.y == 0.0f && vpos.z == 0.0f)
	{
		imageStore(o_AOTexture, ivec2(gl_GlobalInvocationID.xy), vec4(vec3(1.0f), 1.0f));
		return;
	}
		

	vec3 world_norm = DecodeNormals(texelFetch(u_NormalsTex, loc, 0).rg);
	vec3 vnorm = transpose(inverse(mat3(u_PushConstant.ViewMatrix))) * world_norm;
	vec3 vdir = normalize(-vpos.xyz);

	float ao = ComputeAO(vpos.xyz, vnorm.xyz, vdir.xyz, u_PushConstant.AO_Mode);

	ao = clamp(ao, 0.0f, 1.0f);

	imageStore(o_AOTexture, ivec2(gl_GlobalInvocationID.xy), vec4(vec3(ao), 1.0f));
}