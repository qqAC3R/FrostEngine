#type compute
#version 460

#define TWO_PI 6.283185308
#define PI 3.141592654
#define PI_OVER_TWO 1.570796327

layout(local_size_x = 8, local_size_y = 8) in;

layout(binding = 0) uniform sampler2D u_SkyViewLUT;
layout(binding = 1, rgba16f) writeonly uniform imageCube u_IrradianceMap;

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

// Importance sample a cosine lobe.
vec3 ImportanceSample(uint i, uint N, vec3 normal)
{
	vec2 xi = Hammersley(i, N);

	float phi = TWO_PI * xi.x;
	float theta = ArcCos(sqrt(xi.y));
	
	float sinTheta = sin(theta);
	vec3 world = vec3(sinTheta * cos(phi), sinTheta * sin(phi), cos(theta));

	vec3 up = abs(normal.z) < 0.999f ? vec3(0.0f, 0.0f, 1.0f) : vec3(1.0f, 0.0f, 0.0f);
	vec3 tangent = normalize(cross(up, normal));
	vec3 bitangent = cross(normal, tangent);

	return normalize(tangent * world.x + bitangent * world.y + normal * world.z);
}

vec3 SampleSkyViewLUT(sampler2D SkyViewLUT, vec3 viewPos, vec3 viewDir, vec3 sunDir, float groundRadius)
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

	return texture(SkyViewLUT, vec2(u, v)).rgb;
}

void main()
{
	vec2 cubemapSize = vec2(imageSize(u_IrradianceMap).xy);
	vec3 worldPos = CubeToWorld(ivec3(gl_GlobalInvocationID), cubemapSize);

	vec3 sunPos = normalize(u_PushConstant.SunDirection_Intensity.xyz);
	vec3 viewPos = u_PushConstant.ViewPos_SunSize.xyz;

	float groundRadius = u_PushConstant.PlanetAbledo_Radius.w;

	vec3 normal = normalize(worldPos);
	normal.xyz *= -1.0f;

	vec3 irradiance = vec3(0.0f);
	const uint numSamples = 512;

	for(uint i = 0; i < numSamples; i++)
	{
		vec3 sampleDir = ImportanceSample(i, numSamples, normal);
		irradiance += SampleSkyViewLUT(u_SkyViewLUT, viewPos, sampleDir, sunPos, groundRadius);
	}

	irradiance = PI * irradiance * (1.0f / float(numSamples)) * u_PushConstant.SunDirection_Intensity.w;
	imageStore(u_IrradianceMap, ivec3(gl_GlobalInvocationID), vec4(irradiance, 1.0f));
}