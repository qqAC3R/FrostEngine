#type compute
#version 460

#define TWO_PI 6.283185308
#define PI 3.141592654
#define PI_OVER_TWO 1.570796327

layout(local_size_x = 8, local_size_y = 8) in;

layout(binding = 0) uniform sampler2D u_SkyViewLUT;
layout(binding = 1, rgba16f) writeonly uniform imageCube u_PrefilteredMap;

layout(push_constant) uniform PushConstant
{
	vec4 RayleighScattering;
	vec4 RayleighAbsorption;
	vec4 MieScattering;
	vec4 MieAbsorption;

	vec4 OzoneAbsorption;
	vec4 PlanetAbledo_Radius;

	vec4 SunDirection_Intensity;
	
	vec4 ViewPos_SunSize;
	
	float AtmosphereRadius;

	// Used for generating the irradiance map
	float Roughness;
	int NrSamples;

} u_PushConstant;

vec3 CubeToWorld(ivec3 cubeCoord, vec2 cubeSize)
{
	vec2 texCoord = vec2(cubeCoord.xy) / cubeSize;
	texCoord = texCoord  * 2.0 - 1.0; // Swap to -1 -> +1
	switch(cubeCoord.z)
	{
		case 0: return vec3(1.0, -texCoord.yx); // CUBE_MAP_POS_X
		case 1: return vec3(-1.0, -texCoord.y, texCoord.x); // CUBE_MAP_NEG_X
		case 2: return vec3(texCoord.x, 1.0, texCoord.y); // CUBE_MAP_POS_Y
		case 3: return vec3(texCoord.x, -1.0, -texCoord.y); // CUBE_MAP_NEG_Y
		case 4: return vec3(texCoord.x, -texCoord.y, 1.0); // CUBE_MAP_POS_Z
		case 5: return vec3(-texCoord.xy, -1.0); // CUBE_MAP_NEG_Z
	}
	return vec3(0.0);
}

float ArcCos(float x)
{
	return acos(clamp(x, -1.0, 1.0));
}

// Hammersley sequence, generates a low discrepancy pseudorandom number.
vec2 Hammersley(uint i, uint N)
{
	float fbits;
	uint bits = i;
	
	bits  = (bits << 16u) | (bits >> 16u);
	bits  = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits  = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits  = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits  = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	fbits = float(bits) * 2.3283064365386963e-10;
	
	return vec2(float(i) / float(N), fbits);
}

// Importance sampling of the Smith-Schlick-Beckmann geometry function.
vec3 SSB_Importance(vec2 Xi, vec3 N, float roughness)
{
	float a = roughness * roughness;

	float phi = 2.0 * PI * Xi.x;
	float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
	float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

	// from spherical coordinates to cartesian coordinates - halfway vector
	vec3 H;
	H.x = cos(phi) * sinTheta;
	H.y = sin(phi) * sinTheta;
	H.z = cosTheta;

	// from tangent-space H vector to world-space sample vector
	vec3 up        = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
	vec3 tangent   = normalize(cross(up, N));
	vec3 bitangent = cross(N, tangent);

	vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
	return normalize(sampleVec);
}

// Smith-Schlick-Beckmann geometry function.
float SSB_Geometry(vec3 N, vec3 H, float roughness)
{
	float a      = roughness * roughness;
	float a2     = a * a;
	float NdotH  = max(dot(N, H), 0.0);
	float NdotH2 = NdotH * NdotH;
	
	float nom   = a2;
	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom       = PI * denom * denom;
	
	return nom / denom;
}

vec3 SampleSkyViewLUT(sampler2D SkyViewLUT, vec3 viewPos, vec3 viewDir, vec3 sunDir, float groundRadius, float mipLevel)
{
	float height = length(viewPos);
	vec3 up = viewPos / height;

	float horizonAngle = ArcCos(sqrt(height * height - groundRadius * groundRadius) / height);
	float altitudeAngle = horizonAngle - acos(dot(viewDir, up));

	vec3 right = cross(sunDir, up);
	vec3 forward = cross(up, right);

	vec3 projectedDir = normalize(viewDir - up * (dot(viewDir, up)));
	float sinTheta = dot(projectedDir, right);
	float cosTheta = dot(projectedDir, forward);
	float azimuthAngle = atan(sinTheta, cosTheta) + PI;

	float u = azimuthAngle / (TWO_PI);
	float v = 0.5f + 0.5f * sign(altitudeAngle) * sqrt(abs(altitudeAngle) / PI_OVER_TWO);

	return textureLod(SkyViewLUT, vec2(u, v), mipLevel).rgb;
}


void main()
{
	const uint sampleCount = u_PushConstant.NrSamples;
	vec2 mipSize = vec2(imageSize(u_PrefilteredMap).xy);

	ivec3 cubeCoord = ivec3(gl_GlobalInvocationID);

	vec3 sunPos = normalize(u_PushConstant.SunDirection_Intensity.xyz);
	vec3 viewPos = u_PushConstant.ViewPos_SunSize.xyz;
	float groundRadius = u_PushConstant.PlanetAbledo_Radius.w;

	vec3 worldPos = CubeToWorld(cubeCoord, mipSize);
	vec3 N = normalize(worldPos);

	vec3 prefilteredColor = vec3(0.0f);
	float totalWeight = 0.0f;

	for(uint i = 0; i < sampleCount; i++)
	{
		vec2 Xi = Hammersley(i, sampleCount);
		vec3 H = SSB_Importance(Xi, N, u_PushConstant.Roughness);
		vec3 L  = normalize(2.0 * dot(N, H) * H - N);
		
		float NdotL = max(dot(N, L), 0.0f);
		if(NdotL > 0.0f)
		{
			float D = SSB_Geometry(N, H, u_PushConstant.Roughness);
			float NdotH = max(dot(N, H), 0.0f);
			float HdotV = max(dot(H, N), 0.0f);

			float pdf = D * NdotH / (4.0f * HdotV + 0.0001);

			float saTexel = 4.0f * PI / (6.0f * 256.0f * 128.0f);
			float saSample = 1.0 / (float(sampleCount) * pdf + 0.0001);

			float mipLevel = u_PushConstant.Roughness == 0.0 ? 0.0 : 0.5 * log2(saSample / saTexel);

			vec3 skySample = SampleSkyViewLUT(u_SkyViewLUT, viewPos, L, sunPos, groundRadius, mipLevel);

			prefilteredColor += u_PushConstant.SunDirection_Intensity.w * skySample * NdotL;
			totalWeight += NdotL;

		}
	}

	prefilteredColor = prefilteredColor / totalWeight;
	imageStore(u_PrefilteredMap, cubeCoord, vec4(prefilteredColor, 1.0));

}