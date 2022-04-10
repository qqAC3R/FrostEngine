#type compute
#version 460

#define TRANSMITTANCE_STEPS 40

layout(local_size_x = 8, local_size_y = 8) in;

layout(binding = 0, rgba16f) restrict writeonly uniform image2D u_TransmittanceLUT;

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

vec3 ComputeSkyTrasmittance(vec3 pos,
							vec3 sunDir,
							ScatteringParams params,
							float groundRadius,
							float atmosphereRadius)
{
	if (RayIntersectSphere(pos, sunDir, groundRadius) > 0.0)
		return vec3(0.0);

	float atmosphereDist = RayIntersectSphere(pos, sunDir, atmosphereRadius);

	float dist = 0.0f;
	vec3 transmittance = vec3(1.0);
	for(uint i = 0; i < TRANSMITTANCE_STEPS; i++)
	{
		float newDist = ((float(i) + 0.3) / float(TRANSMITTANCE_STEPS)) * atmosphereDist;
		float dt = newDist - dist;

		dist = newDist;

		vec3 newPos = pos + dist * sunDir;
		vec3 extinction = ComputeExtinction(newPos, params, groundRadius);

		transmittance *= exp(-dt * extinction);
	}

	return transmittance;
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

	ivec2 globalInvocation = ivec2(gl_GlobalInvocationID.xy);
	vec2 size = vec2(imageSize(u_TransmittanceLUT).xy);
	vec2 coords = vec2(globalInvocation) + vec2(0.5f);

	vec2 uv = coords / size;

	float sunCosTheta = 2.0 * uv.x - 1.0;
	float sunTheta = ArcCos(sunCosTheta);

	float height = mix(groundRadius, atmosphereRadius, uv.y);

	vec3 pos = vec3(0.0f, height, 0.0f);
	vec3 sunDir = normalize(vec3(0.0f, sunCosTheta, -sin(sunTheta)));

	vec3 transmittance = ComputeSkyTrasmittance(pos, sunDir, params, groundRadius, atmosphereRadius);

	imageStore(u_TransmittanceLUT, globalInvocation, vec4(max(transmittance, vec3(0.0f)), 1.0f));
}