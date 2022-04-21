#type compute
#version 460

#define PI 3.141592654
#define PLANET_RADIUS_OFFSET 0.01
#define NUM_STEPS 32

layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;

layout(binding = 0) writeonly uniform image3D u_AerialLUT;
layout(binding = 1) uniform sampler2D u_TransmittanceLUT;
layout(binding = 2) uniform sampler2D u_MultiScatterLUT;
layout(binding = 4) uniform sampler3D u_AerialLUT_Sampler;

// Camera specific uniforms.
layout(binding = 3) uniform CameraBlock
{
	mat4 ViewMatrix;
	mat4 ProjMatrix;
	mat4 InvViewProjMatrix;
	vec4 CamPosition;
	vec4 NearFarPlane; // Near plane (x), far plane (y). z and w are unused.
} u_CameraSpecs;

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

} m_SkyParams;

struct ScatteringParams
{
	vec4 RayleighScattering; //  Rayleigh scattering base (x, y, z) and height falloff (w).
	vec4 RayleighAbsorption; //  Rayleigh absorption base (x, y, z) and height falloff (w).
	vec4 MieScattering; //  Mie scattering base (x, y, z) and height falloff (w).
	vec4 MieAbsorption; //  Mie absorption base (x, y, z) and height falloff (w).
	vec4 OzoneAbsorption; //  Ozone absorption base (x, y, z) and scale (w).
};


// Helper functions.
float ArcCos(float x)
{
	return acos(clamp(x, -1.0, 1.0));
}

float RayIntersectSphere(vec3 rayOrigin, vec3 rayDirection, float radius)
{
	float b = dot(rayOrigin, rayDirection);
	float c = dot(rayOrigin, rayOrigin) - radius * radius;
	if (c > 0.0f && b > 0.0)
	  return -1.0;
	
	float discr = b * b - c;
	if (discr < 0.0)
	  return -1.0;
	
	// Special case: inside sphere, use far discriminant
	if (discr > b * b)
	  return (-b + sqrt(discr));
	
	return -b - sqrt(discr);
}

vec3 GetValFromLUT(sampler2D u_LUT,
				   vec3 pos,
				   vec3 sunDir,
				   float groundRadius,
				   float atmosphereRadius)
{
	float height = length(pos);
	vec3 up = pos / height;

	float sunCosZenithAngle = dot(sunDir, up);

	float u = clamp(0.5f + 0.5f * sunCosZenithAngle, 0.0f, 1.0f);
	float v = max(0.0f, min(1.0f, (height - groundRadius) / (atmosphereRadius - groundRadius)));

	return texture(u_LUT, vec2(u, v)).rgb;
}


float GetMiePhase(float cosTheta)
{
	const float g = 0.8;
	const float scale = 3.0 / (8.0 * PI);
	
	float num = (1.0 - g * g) * (1.0 + cosTheta * cosTheta);
	float denom = (2.0 + g * g) * pow((1.0 + g * g - 2.0 * g * cosTheta), 1.5);
	
	return scale * num / denom;
}

float GetRayleighPhase(float cosTheta)
{
	const float k = 3.0 / (16.0 * PI);
	return k * (1.0 + cosTheta * cosTheta);
}

vec3 ComputeRayleighScattering(vec3 pos, ScatteringParams params, float groundRadius)
{
	float altitude = (length(pos) - groundRadius) * 1000.0f; // convert into KM
	float rayleighDensity = exp(-altitude / params.RayleighScattering.w);

	return params.RayleighScattering.rgb * rayleighDensity;
}

vec3 ComputeMieScattering(vec3 pos, ScatteringParams params, float groundRadius)
{
	float altitude = (length(pos) - groundRadius) * 1000.0f; // convert into KM
	float mieDensity = exp(-altitude / params.MieScattering.w);

	return params.MieScattering.rgb * mieDensity;
}

vec3 ComputeExtinction(vec3 pos, ScatteringParams params, float groundRadius)
{
	float altitude = (length(pos) - groundRadius) * 1000.0; // transform distance into KM

	// Calculate the density for rayLeigh and mie scattering
	float rayLeighDensity = exp(-altitude / params.RayleighScattering.w);
	float mieDensity = exp(-altitude / params.MieScattering.w);

	vec3 rayleighScattering = params.RayleighScattering.rgb * rayLeighDensity;
	vec3 rayleighAbsorption = params.RayleighAbsorption.rgb * rayLeighDensity;

	vec3 mieScattering = params.MieScattering.rgb * mieDensity;
	vec3 mieAbsorption = params.MieAbsorption.rgb * mieDensity;

	vec3 ozoneAbsorption = params.OzoneAbsorption.w * params.OzoneAbsorption.rgb * max(0.0f, 1.0f - abs(altitude - 25.0f) / 15.0f);

	return rayleighScattering + vec3(rayleighAbsorption + mieScattering + mieAbsorption) + ozoneAbsorption;
}

vec4 RayMarchScattering(vec3 pos,
						vec3 rayDir,
						vec3 sunDir,
						float tMax,
                        ScatteringParams params,
						float groundRadius,
                        float atmoRadius)
{
	float cosTheta = dot(rayDir, sunDir);

	float miePhaseValue = GetMiePhase(cosTheta);
	float rayleighPhaseValue = GetRayleighPhase(-cosTheta);

	vec3 lum = vec3(0.0);
	vec3 transmittance = vec3(1.0);
	float t = 0.0;

	for (uint i = 0; i < NUM_STEPS; i++)
	{
		float newT = ((float(i) + 0.3) / float(NUM_STEPS)) * tMax;
		float dt = newT - t;
		t = newT;

		vec3 newPos = pos + t * rayDir;

		vec3 rayleighScattering = ComputeRayleighScattering(newPos, params, groundRadius);
		vec3 mieScattering = ComputeMieScattering(newPos, params, groundRadius);
		vec3 extinction = ComputeExtinction(newPos, params, groundRadius);

		vec3 sampleTransmittance = exp(-dt * extinction);

		// Sample LUTS
		vec3 sunTransmittance = GetValFromLUT(u_TransmittanceLUT, newPos, sunDir, groundRadius, atmoRadius);
		vec3 psiMS = GetValFromLUT(u_MultiScatterLUT, newPos, sunDir, groundRadius, atmoRadius);


		vec3 rayleighInScattering = rayleighScattering * (rayleighPhaseValue * sunTransmittance + psiMS);
		vec3 mieInScattering = mieScattering * (miePhaseValue * sunTransmittance + psiMS);
		vec3 inScattering = (rayleighInScattering + mieInScattering);

		// Integrated scattering within path segment.
		vec3 scatteringIntegral = (inScattering - inScattering * sampleTransmittance) / extinction;

		lum += scatteringIntegral * transmittance;

		transmittance *= sampleTransmittance;

	}

	return vec4(lum, 1.0 - dot(transmittance, vec3(1.0 / 3.0)));
}

void main()
{
	ivec3 globalInvocation = ivec3(gl_GlobalInvocationID.xyz);

	vec3 texSize = vec3(imageSize(u_AerialLUT).xyz);
	vec3 texelSize = vec3(1.0f) / texSize;

	ScatteringParams params;
	params.RayleighScattering = m_SkyParams.RayleighScattering;
	params.RayleighAbsorption = m_SkyParams.RayleighAbsorption;
	params.MieScattering = m_SkyParams.MieScattering;
	params.MieAbsorption = m_SkyParams.MieAbsorption;
	params.OzoneAbsorption = m_SkyParams.OzoneAbsorption;

	float groundRadius = m_SkyParams.PlanetAbledo_Radius.w;
	float atmosphereRadius = m_SkyParams.AtmosphereRadius;

	vec3 sunDir = normalize(-m_SkyParams.SunDirection_Intensity.xyz);
	vec3 viewPos = vec3(m_SkyParams.ViewPos_SunSize.xyz);

	// Compute the center of the voxel in worldspace (relative to the planet).
	vec2 pixelPos = vec2(globalInvocation.xy) + vec2(0.5f);
	vec3 clipSpace = vec3(vec2(2.0) * (pixelPos * texelSize.xy) - vec2(1.0), 0);
	vec4 hPos = u_CameraSpecs.InvViewProjMatrix * vec4(clipSpace, 1.0);

	vec3 camPos = (u_CameraSpecs.CamPosition.xyz * 1e-6) + viewPos;
	vec3 worldDir = normalize(hPos.xyz);

	float slice = (float(globalInvocation.z) + 0.5f) * texelSize.z;
	slice *= slice;
	slice *= texSize.z;

	float planeDelta = (u_CameraSpecs.NearFarPlane.y - u_CameraSpecs.NearFarPlane.x) * texelSize.z * 1e-6;

	float tMax = planeDelta * slice;
	vec3 newWorldPos = camPos + (tMax * worldDir);
	float voxelHeight = length(newWorldPos);

	if(voxelHeight <= groundRadius + PLANET_RADIUS_OFFSET)
	{
		float offset = groundRadius + PLANET_RADIUS_OFFSET + 0.001f;
		newWorldPos = normalize(newWorldPos) * offset;
		tMax = length(newWorldPos - camPos);
	}

	vec4 result = RayMarchScattering(
		camPos,
		worldDir,
		sunDir,
		tMax,
		params,
		groundRadius,
		atmosphereRadius
	);

	result.rgb /= result.a;

	imageStore(u_AerialLUT, globalInvocation, result);

}