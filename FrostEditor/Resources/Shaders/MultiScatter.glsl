#type compute
#version 460

#define PI 3.141592654

#define MULTISCATTER_STEPS 20
#define SQURT_SAMPLES 8

layout(local_size_x = 1, local_size_y = 1, local_size_z = 64) in;

layout(binding = 0, rgba16f) writeonly uniform image2D u_MultiScatterLUT;

layout(binding = 1) uniform sampler2D u_TransmittanceLUT;

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

vec3 GetSphericalDirection(float theta, float phi)
{
	float cosPhi = cos(phi);
	float sinPhi = sin(phi);
	float cosTheta = cos(theta);
	float sinTheta = sin(theta);
	return vec3(sinPhi * sinTheta, cosPhi, sinPhi * cosTheta);
}

struct ScatteringResult
{
	vec3 Luminance;
	vec3 LuminanceFactor;
};

vec3 GetValFromTransmittanceLUT(sampler2D Transmittance_LUT,
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

	return texture(Transmittance_LUT, vec2(u, v)).rgb;
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

ScatteringResult ComputeSingleScattering(vec3 pos,
										 vec3 rayDir,
										 vec3 sunDir,
										 ScatteringParams params,
										 float groundRadius,
										 float atmosphereRadius,
										 vec3 groundAlbedo)
{
	float atmosphereDist = RayIntersectSphere(pos, rayDir, atmosphereRadius);
	float groundDist = RayIntersectSphere(pos, rayDir, groundRadius);
	float tMax = groundDist > 0.0f ? groundDist : atmosphereDist;

	float cosTheta = dot(rayDir, sunDir);

	float miePhaseValue = GetMiePhase(cosTheta);
	float rayleighPhaseValue = GetRayleighPhase(-cosTheta);

	vec3 lum = vec3(0.0f);
	vec3 lumFactor = vec3(0.0f);
	vec3 transmittance = vec3(1.0f);
	float t = 0.0f;
	for(uint stepI = 0; stepI < MULTISCATTER_STEPS; stepI++)
	{
		float newT = ((float(stepI) + 0.3) / float(MULTISCATTER_STEPS)) * tMax;
		float dt = newT - t;
		t = newT;

		vec3 newPos = pos + t * rayDir;

		vec3 rayleighScattering = ComputeRayleighScattering(newPos, params, groundRadius);
		vec3 mieScattering = ComputeMieScattering(newPos, params, groundRadius);
		vec3 extinction = ComputeExtinction(newPos, params, groundRadius);

		vec3 sampleTransmittance = exp(-dt * extinction);

		vec3 scatteringNoPhase = rayleighScattering + mieScattering;
		vec3 scatteringF = (scatteringNoPhase - scatteringNoPhase * sampleTransmittance) / extinction;

		lumFactor += transmittance * scatteringF;

		vec3 sunTransmittance = GetValFromTransmittanceLUT(u_TransmittanceLUT, newPos, sunDir, groundRadius, atmosphereRadius);

		vec3 rayleighInScattering = rayleighScattering * rayleighPhaseValue;
		vec3 mieInScattering = mieScattering * miePhaseValue;
		vec3 inScattering = (rayleighInScattering + mieInScattering) * sunTransmittance;

		// Integrated scattering within path segment.
		vec3 scatteringIntegral = (inScattering - inScattering * sampleTransmittance) / extinction;

		lum += scatteringIntegral * transmittance;
		transmittance *= sampleTransmittance;
	}


	// Bounced lighting from the ground using simple Lambertian diffuse.
	if(groundDist > 0.0f)
	{
		vec3 hitPos = pos + groundDist * rayDir;
		
		if(dot(pos, sunDir) > 0.0f)
		{
			const vec3 normal = normalize(hitPos);
			const float NdotL = clamp(dot(normal, sunDir), 0.0f, 1.0f);

			hitPos = normal * groundRadius;

			vec3 sunTransmittance = GetValFromTransmittanceLUT(u_TransmittanceLUT, hitPos, sunDir, groundRadius, atmosphereRadius);

			lum += transmittance * groundAlbedo * sunTransmittance * NdotL / PI;
		}
	}


	ScatteringResult results;
	results.Luminance = lum;
	results.LuminanceFactor = lumFactor;

	return results;
}

// Using parallel sums to compute the scattering integral.
shared vec3 Factor_Ms[64];
shared vec3 Luminance_Ms[64];

void main()
{
	ivec3 globalInvocation = ivec3(gl_GlobalInvocationID.xyz);

	vec2 texSize = vec2(imageSize(u_MultiScatterLUT).xy);
	vec2 texelSize = vec2(1.0f) / texSize;

	ScatteringParams params;
	params.RayleighScattering = m_SkyParams.RayleighScattering;
	params.RayleighAbsorption = m_SkyParams.RayleighAbsorption;
	params.MieScattering = m_SkyParams.MieScattering;
	params.MieAbsorption = m_SkyParams.MieAbsorption;
	params.OzoneAbsorption = m_SkyParams.OzoneAbsorption;

	//float groundRadius = m_SkyParams.PlanetAbledo_Radius.w;
	//float atmosphereRadius = m_SkyParams.SunDir_AtmRadius.w;
	//vec3 groundAlbedo = m_SkyParams.PlanetAbledo_Radius.rgb;

	float groundRadius = m_SkyParams.PlanetAbledo_Radius.w;
	float atmosphereRadius = m_SkyParams.AtmosphereRadius;
	vec3 groundAlbedo = m_SkyParams.PlanetAbledo_Radius.rgb;


	vec2 uv = (vec2(globalInvocation) + vec2(0.5f)) * texelSize;
	float sunCosTheta = 2.0f * uv.x - 1.0f;
	float sunTheta = ArcCos(sunCosTheta);
	float height = mix(groundRadius, atmosphereRadius, uv.y);

	vec3 pos = vec3(0.0, height, 0.0);
	vec3 sunDir = normalize(vec3(0.0, sunCosTheta, -sin(sunTheta)));

	float sqrtSample = float(SQURT_SAMPLES);
	float i = 0.5 + float(globalInvocation.z / SQURT_SAMPLES);
	float j = 0.5 + float(globalInvocation.z - float((globalInvocation.z / SQURT_SAMPLES) * SQURT_SAMPLES));
	float phi = ArcCos(1.0 - 2.0 * (float(j) + 0.5) / sqrtSample);

	float theta = PI * (float(i) + 0.5) / sqrtSample;

	vec3 rayDir = GetSphericalDirection(theta, phi);

	ScatteringResult result = ComputeSingleScattering(
		pos,
		rayDir,
		sunDir,
		params,
		groundRadius,
		atmosphereRadius,
		groundAlbedo
	);
	
	Factor_Ms[globalInvocation.z] = result.LuminanceFactor / (sqrtSample * sqrtSample);
	Luminance_Ms[globalInvocation.z] = result.Luminance / (sqrtSample * sqrtSample);
	barrier();


	// Parallel sums for an array of 64 elements.
	// 64 to 32
	if (globalInvocation.z < 32)
	{
		Factor_Ms[globalInvocation.z] += Factor_Ms[globalInvocation.z + 32];
		Luminance_Ms[globalInvocation.z] += Luminance_Ms[globalInvocation.z + 32];
	}
	barrier();


	// Parallel sums for an array of 32 elements.
	// 32 to 16
	if (globalInvocation.z < 16)
	{
		Factor_Ms[globalInvocation.z] += Factor_Ms[globalInvocation.z + 16];
		Luminance_Ms[globalInvocation.z] += Luminance_Ms[globalInvocation.z + 16];
	}
	barrier();


	// Parallel sums for an array of 16 elements.
	// 16 to 8
	if (globalInvocation.z < 8)
	{
		Factor_Ms[globalInvocation.z] += Factor_Ms[globalInvocation.z + 8];
		Luminance_Ms[globalInvocation.z] += Luminance_Ms[globalInvocation.z + 8];
	}
	barrier();


	// Parallel sums for an array of 8 elements.
	// 8 to 4
	if (globalInvocation.z < 4)
	{
		Factor_Ms[globalInvocation.z] += Factor_Ms[globalInvocation.z + 4];
		Luminance_Ms[globalInvocation.z] += Luminance_Ms[globalInvocation.z + 4];
	}
	barrier();

	// Parallel sums for an array of 4 elements.
	// 4 to 2
	if (globalInvocation.z < 2)
	{
		Factor_Ms[globalInvocation.z] += Factor_Ms[globalInvocation.z + 2];
		Luminance_Ms[globalInvocation.z] += Luminance_Ms[globalInvocation.z + 2];
	}
	barrier();


	// Parallel sums for an array of 2 elements.
	// 2 to 1
	if (globalInvocation.z < 1)
	{
		Factor_Ms[globalInvocation.z] += Factor_Ms[globalInvocation.z + 1];
		Luminance_Ms[globalInvocation.z] += Luminance_Ms[globalInvocation.z + 1];
	}
	barrier();

	if (globalInvocation.z > 0)
		return;

	// Store the value
	vec3 psi = Luminance_Ms[0] / (vec3(1.0) - Factor_Ms[0]);
	imageStore(u_MultiScatterLUT, globalInvocation.xy, vec4(max(psi, vec3(0.0)), 1.0));
}
