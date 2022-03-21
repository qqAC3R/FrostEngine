#type compute
#version 460
#extension GL_EXT_nonuniform_qualifier : enable

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(binding = 0) uniform sampler2D u_ViewPositionTex;
layout(binding = 1) uniform sampler2D u_DepthPyramid;
layout(binding = 2) uniform sampler2D u_NormalsTex;
layout(binding = 3) uniform sampler2D u_NoiseTex;

layout(binding = 4, rgba32f) uniform writeonly image2D o_AOTexture;

layout(push_constant) uniform PushConstant
{
	mat4 ViewMatrix;
	mat4 InvProjMatrix;
	vec4 ProjInfo;
	// Math::Vector4 projinfo	= { 2.0f / (width * proj._11), 2.0f / (height * proj._22), -1.0f / proj._11, -1.0f / proj._22 };

	vec4 ClipInfo;
	/*
	clipinfo.x = camera.GetNearPlane();
	clipinfo.y = camera.GetFarPlane();
	clipinfo.z = 0.5f * (app->GetClientHeight() / (2.0f * tanf(camera.GetFov() * 0.5f)));
	*/

	vec4 ScreenSize;

} u_PushConstant;

layout(binding = 5) buffer DebugBuffer
{
    float Value[];
} u_DebugBuffer;

#define PI				3.1415926535897932
#define TWO_PI			6.2831853071795864
#define HALF_PI			1.5707963267948966
#define ONE_OVER_PI		0.3183098861837906

#define NUM_STEPS		8
#define RADIUS			2.0		// in world space

#define NUM_MIP_LEVELS		5
#define PREFETCH_CACHE_SIZE	8

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
#if 1
	int mipLevel = clamp(int(floor(log2(currStep / PREFETCH_CACHE_SIZE))), 0, NUM_MIP_LEVELS - 1);
	
	vec2 baseSize = vec2(textureSize(u_DepthPyramid, 0));
	//vec2 mipCoord = vec2(uv) / u_PushConstant.ScreenSize.xy;
	vec2 mipCoord = (uv / baseSize);
	float depth = textureLod(u_DepthPyramid, mipCoord, mipLevel).r;
	
	
	
	vec4 ret = vec4(0.0f, 0.0f, 0.0f, depth);
	
	ret.xyz = CalcViewZ(depth, mipCoord);

	//ret.z = depth;


	//ret.z = u_PushConstant.ClipInfo.x + depth * (u_PushConstant.ClipInfo.y - u_PushConstant.ClipInfo.x);
	//ret.z = 1.0f - ret.z;
	//ret.xy = (uv * u_PushConstant.ProjInfo.xy + u_PushConstant.ProjInfo.zw) * ret.z;
	
	return ret;
#endif

	//vec2 texCoord = vec2(uv) / u_PushConstant.ScreenSize.xy;
	////return vec4(CalcViewZ(texture(u_DepthPyramid, texCoord).r, texCoord), 1.0f);
	//
	//return texture(u_ViewPositionTex, texCoord);
}

float Falloff(float dist2, float cosh)
{
#define FALLOFF_START2	0.16
#define FALLOFF_END2	4.0

	return 2.0 * clamp((dist2 - FALLOFF_START2) / (FALLOFF_END2 - FALLOFF_START2), 0.0, 1.0);
}


// https://aras-p.info/texts/CompactNormalStorage.html
vec3 DecodeNormals(vec2 enc)
{
    float scale = 1.7777f;
    vec3 nn = vec3(enc, 0.0f) * vec3(2 * scale, 2 * scale,0) + vec3(-scale, -scale,1);
    
	float g = 2.0 / dot(nn.xyz,nn.xyz);

    vec3 n;
    n.xy = g*nn.xy;
    n.z = g-1;

    return n;
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

  // Use saturate(x) instead of max(x,0.f) because that is faster on Kepler
  //return clamp(NdotV - control.NDotVBias,0,1) * clamp(Falloff(VdotV),0,1);
  return 0.0f;
}

void main()
{
	ivec2 loc = ivec2(gl_GlobalInvocationID.xy);
	vec2 s_UV = vec2(loc) / u_PushConstant.ScreenSize.xy;
	//vec4 vpos = texelFetch(u_ViewPositionTex, loc, 0);
	vec4 vpos = GetViewPosition(loc, 1.0);
	
	if (vpos.w == 1.0) {
		imageStore(o_AOTexture, ivec2(gl_GlobalInvocationID.xy), vec4(vec3(1.0f), 1.0f));
		return;
	}

	//vec4 vpos = vec4(CalcViewZ(texture(u_DepthPyramid, s_UV).r, s_UV), 1.0f);


	vec4 s;
	vec3 world_norm = DecodeNormals(texelFetch(u_NormalsTex, loc, 0).rg);
	vec3 vnorm = transpose(inverse(mat3(u_PushConstant.ViewMatrix))) * world_norm;
	vec3 vdir = normalize(-vpos.xyz);

	vec3 dir;
	vec3 ws;

	// calculation uses left handed system
	//vnorm.z = -vnorm.z;

	vec2 noises	= texelFetch(u_NoiseTex, loc % 4, 0).rg;
	vec2 offset;
	vec2 horizons = vec2(-1.0, -1.0);

	float radius = (RADIUS * u_PushConstant.ClipInfo.z) / (1.0f - vpos.z);
	radius = max(NUM_STEPS, radius);
		
	float stepSize	= radius / NUM_STEPS;
	float phi		= noises.x * PI;
	float ao		= 0.0;
	float division	= noises.y * stepSize;
	float currStep	= 1.0 + division;

	float dist, invdist, falloff, cosh;

	//for (int k = 0; k < NUM_DIRECTIONS; ++k)
	{

		dir = vec3(cos(phi), sin(phi), 0.0);
		horizons = vec2(-1.0);


		// calculate horizon angles
		for (int j = 0; j < NUM_STEPS; ++j)
		{
#if 1
			offset = round(dir.xy * currStep);

			// h1
			s = GetViewPosition(vec2(gl_GlobalInvocationID.xy) + offset, 0);
			ws = s.xyz - vpos.xyz;

			// Length of the vector
			//dist = dot(ws, ws);
			//invdist = inversesqrt(dist);
			dist = length(ws);
			invdist = 1/dist;
			cosh = invdist * dot(ws, vdir);

			falloff = Falloff(dist, cosh);
			horizons.x = max(horizons.x, cosh - falloff);


			// h2
			s = GetViewPosition(vec2(gl_GlobalInvocationID.xy) - offset, 0);
			ws = s.xyz - vpos.xyz;

			// Length of the vector
			//dist = dot(ws, ws);
			//invdist = inversesqrt(dist);
			dist = length(ws);
			invdist = 1/dist;
			cosh = invdist * dot(ws, vdir);

			falloff = Falloff(dist, cosh);
			horizons.y = max(horizons.y, cosh - falloff);

			// increment
			currStep += stepSize;
#endif
		}

		horizons = acos(horizons);
	



		// calculate gamma
		vec3 bitangent	= normalize(cross(dir, vdir));
		vec3 tangent	= cross(vdir, bitangent);
		vec3 nx			= vnorm - bitangent * dot(vnorm, bitangent);

		float nnx		= length(nx);
		float invnnx	= 1.0 / (nnx + 1e-6);			// to avoid division with zero
		float cosxi		= dot(nx, tangent) * invnnx;	// xi = gamma + HALF_PI
		float gamma		= acos(cosxi) - HALF_PI;
		float cosgamma	= dot(nx, vdir) * invnnx;
		float singamma2	= -2.0 * cosxi;					// cos(x + HALF_PI) = -sin(x)

		// clamp to normal hemisphere
		horizons.x = gamma + max(-horizons.x - gamma, -HALF_PI);
		horizons.y = gamma + min(horizons.y - gamma, HALF_PI);

		

		// Riemann integral is additive
		ao += nnx * 0.25 * (
			(horizons.x * singamma2 + cosgamma - cos(2.0 * horizons.x - gamma)) +
			(horizons.y * singamma2 + cosgamma - cos(2.0 * horizons.y - gamma)));
	}

	imageStore(o_AOTexture, ivec2(gl_GlobalInvocationID.xy), vec4(vec3(ao), 1.0f));
	
}