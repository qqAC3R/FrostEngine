#type compute
#version 460

layout(local_size_x = 8, local_size_y = 8) in;

#define PI 3.141592654
#define NUM_STEPS 32

layout(binding = 0, rgba16f) writeonly uniform image2D u_SkyViewImage;

layout(binding = 1) uniform sampler2D u_TransmittanceLUT;
layout(binding = 2) uniform sampler2D u_MultiScatterLUT;

layout(push_constant) uniform PushConstant
{
	vec4 RayleighScattering;
	vec4 RayleighAbsorption;
	vec4 MieScattering;
	vec4 MieAbsorption;
	vec4 OzoneAbsorption;
	vec4 PlanetAbledo_Radius; // Planet albedo (x, y, z) and radius.
	vec4 SunDir_AtmRadius; // Sun direction (x, y, z) and atmosphere radius (w).
	vec4 ViewPos;  // View position (x, y, z). w is unused.
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

vec3 RaymarchScattering(vec3 pos,
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

	vec3 luminance = vec3(0.0f);
	vec3 transmittance = vec3(1.0f);
	float t = 0.0f;

	for(float i = 0.0f; i < float(NUM_STEPS); i += 1.0f)
	{
		float newT = ((i + 0.3) / float(NUM_STEPS)) * tMax;
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

		luminance += scatteringIntegral * transmittance;

		transmittance *= sampleTransmittance;
	}

	return luminance;
}

void main()
{
	ScatteringParams params;
	params.RayleighScattering = m_SkyParams.RayleighScattering;
	params.RayleighAbsorption = m_SkyParams.RayleighAbsorption;
	params.MieScattering = m_SkyParams.MieScattering;
	params.MieAbsorption = m_SkyParams.MieAbsorption;
	params.OzoneAbsorption = m_SkyParams.OzoneAbsorption;

	float groundRadius = m_SkyParams.PlanetAbledo_Radius.w;
	float atmosphereRadius = m_SkyParams.SunDir_AtmRadius.w;

	
	vec3 sunDir = normalize(-m_SkyParams.SunDir_AtmRadius.xyz);
	//vec3 sunDir = normalize(-vec3(-0.45f, 0.9f, 0.65f));
	vec3 viewPos = vec3(m_SkyParams.ViewPos.xyz);

	ivec2 globalInvocation = ivec2(gl_GlobalInvocationID.xy);
	vec2 size = vec2(imageSize(u_SkyViewImage).xy);
	float u = (0.5 + globalInvocation.x) / size.x;
	float v = (0.5 + globalInvocation.y) / size.y;
	
	float azimuthAngle = 2.0 * PI * (u - 0.5);
	float adjV;
	if(v < 0.5f)
	{
		float coord = 1.0f - 2.0f * v;
		adjV = -(coord * coord);
	}
	else
	{
		float coord = v * 2.0f - 1.0f;
		adjV = coord * coord;
	}

	float height = length(viewPos);
	vec3 up = viewPos / height;

	float horizonAngle = ArcCos(sqrt(height * height - groundRadius * groundRadius) / height) - 0.5f * PI;
	float altitudeAngle = adjV * 0.5f * PI - horizonAngle;

	float cosAltitude = cos(altitudeAngle);
	vec3 rayDir = normalize(vec3(cosAltitude * sin(azimuthAngle), sin(altitudeAngle), -cosAltitude * cos(azimuthAngle)));

	float sunAltitude = (0.5f * PI) - acos(dot(sunDir, up));
	vec3 newSunDir = normalize(vec3(0.0f, sin(sunAltitude), -cos(sunAltitude)));

	float atmosphereDist = RayIntersectSphere(viewPos, rayDir, atmosphereRadius);
	float groundDist = RayIntersectSphere(viewPos, rayDir, groundRadius);
	float tMax = (groundDist < 0.0f) ? atmosphereDist : groundDist;

	vec3 luminance = RaymarchScattering(viewPos,
										rayDir,
										newSunDir,
										tMax,
										params,
										groundRadius,
										atmosphereRadius
	);

	imageStore(u_SkyViewImage, globalInvocation, vec4(vec3(luminance), 1.0f));

}