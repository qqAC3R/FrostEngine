#type vertex
#version 450

layout(push_constant) uniform Constants
{
	mat4 ProjectionMatrix;
	mat4 ViewMatrix;
} u_PushConstant;

layout(location = 0) out vec3 v_FragmentPos;
layout(location = 1) out vec2 v_TexCoord;

// Taken from: https://gist.github.com/rikusalminen/9393151
vec3 CreateCube(int vertexID)
{
	int tri = vertexID / 3;
	int idx = vertexID % 3;
	int face = tri / 2;
	int top = tri % 2;
	
	int dir = face % 3;
	int pos = face / 3;
	
	int nz = dir >> 1;
	int ny = dir & 1;
	int nx = 1 ^ (ny | nz);
	
	vec3 d = vec3(nx, ny, nz);
	float flip = 1 - 2 * pos;
	
	vec3 n = flip * d;
	vec3 u = -d.yzx;
	vec3 v = flip * d.zxy;
	
	float mirror = -1 + 2 * top;
	
	return n + mirror * (1 - 2 * (idx & 1)) * u + mirror * (1 - 2 * (idx >> 1)) * v;
}

//{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
//{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
//{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
//{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}

struct QuadInfo
{
	vec2 Position;
	vec2 TexCoord;
};

QuadInfo quadData[4] = {
	{{ 1.0, -1.0 }, { 0.0, 1.0 }},
	{{-1.0, -1.0 }, { 1.0, 1.0 }},
	{{ 1.0,  1.0 }, { 0.0, 0.0 }},
	{{-1.0,  1.0 }, { 1.0, 0.0 }},
};

void main()
{
	//v_FragmentPos = a_Position;
	vec3 cubeCoords = CreateCube(gl_VertexIndex);
	v_FragmentPos = cubeCoords;


	// Remove the translation from the view matrix
	mat4 vkProjectionMatrix = u_PushConstant.ProjectionMatrix;
	vkProjectionMatrix[1][1] *= -1;

	mat4 modViewProjection = mat4(mat3(u_PushConstant.ViewMatrix));
	vec4 clipPosition = vkProjectionMatrix * modViewProjection * vec4(cubeCoords, 1.0f);
	
	//gl_Position = clipPosition.xyww;

	//v_TexCoord = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	//gl_Position = vec4(v_TexCoord * 2.0 - 1.0, 1.0, 1.0);
	v_TexCoord = quadData[gl_VertexIndex].TexCoord;
	gl_Position = vec4(quadData[gl_VertexIndex].Position, 1.0, 1.0);

	v_FragmentPos = mat3(inverse(u_PushConstant.ViewMatrix)) * vec3(inverse(u_PushConstant.ProjectionMatrix) * gl_Position);
}

#type fragment
#version 450
#extension GL_ARB_separate_shader_objects : enable

// Output
layout(location = 0) out vec4 o_Color;

// Input
layout(location = 0) in vec3 v_FragmentPos;
layout(location = 1) in vec2 v_TexCoord;

#define SKYBOX_MODE_HDRMAP   0
#define SKYBOX_MODE_HILLAIRE 1

#define MAX_MIP 4.0

#define TWO_PI 6.283185308
#define PI 3.141592654
#define PI_OVER_TWO 1.570796327

// Uniforms
layout(binding = 0) uniform samplerCube u_EnvTexture;
layout(binding = 1) uniform sampler2D   u_HillaireLUT;
layout(binding = 2) uniform sampler2D   u_TransmittanceLUT;
layout(binding = 4) uniform sampler2D   u_MultiScatterLUT;

layout(binding = 3) uniform CameraData {
	float Gamma;
	float Exposure;
	float Lod;
	
	float SkyMode;

	vec3 SunDir;
	float SunIntensity;

	vec3 ViewPos;
	float SkyIntensity;

	float GroundRadius;
	float AtmosphereRadius;

	float SunSize;
	float Temp0;


	vec4 RayleighScattering;
	vec4 RayleighAbsorption;
	vec4 MieScattering;
	vec4 MieAbsorption;
	vec4 OzoneAbsorption;

} m_CameraData;

struct ScatteringParams
{
	vec4 RayleighScattering; //  Rayleigh scattering base (x, y, z) and height falloff (w).
	vec4 RayleighAbsorption; //  Rayleigh absorption base (x, y, z) and height falloff (w).
	vec4 MieScattering; //  Mie scattering base (x, y, z) and height falloff (w).
	vec4 MieAbsorption; //  Mie absorption base (x, y, z) and height falloff (w).
	vec4 OzoneAbsorption; //  Ozone absorption base (x, y, z) and scale (w).
};

// From http://filmicworlds.com/blog/filmic-tonemapping-operators/
vec3 Uncharted2Tonemap(vec3 color)
{
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	float W = 11.2;
	return ((color*(A*color+C*B)+D*E)/(color*(A*color+B)+D*F))-E/F;
}

// From https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
vec3 AcesApprox(vec3 v)
{
    //v *= 0.6f;
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((v*(a*v+b))/(v*(c*v+d)+e), 0.0f, 1.0f);
}

float ArcCos(float x)
{
	return acos(clamp(x, -1.0, 1.0));
}


float raySphereIntersect(vec3 r0, vec3 rd, vec3 s0, float sr) {
    // - r0: ray origin
    // - rd: normalized ray direction
    // - s0: sphere center
    // - sr: sphere radius
    // - Returns distance from r0 to first intersecion with sphere,
    //   or -1.0 if no intersection.
    float a = dot(rd, rd);
    vec3 s0_r0 = r0 - s0;
    float b = 2.0 * dot(rd, s0_r0);
    float c = dot(s0_r0, s0_r0) - (sr * sr);
    if (b*b - 4.0*a*c < 0.0) {
        return -1.0;
    }
    return (-b - sqrt((b*b) - 4.0*a*c))/(2.0*a);
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

// From https://www.shadertoy.com/view/wlBXWK
vec2 RayIntersectSphere2D(
    vec3 rayOrigin, // starting position of the ray
    vec3 rayDir, // the direction of the ray
    float radius // and the sphere radius
)
{
    // ray-sphere intersection that assumes
    // the sphere is centered at the origin.
    // No intersection when result.x > result.y
    float a = dot(rayDir, rayDir);
    float b = 2.0 * dot(rayDir, rayOrigin);
    float c = dot(rayOrigin, rayOrigin) - (radius * radius);
    float d = (b*b) - 4.0*a*c;
    if (d < 0.0) return vec2(1e5,-1e5);
    return vec2(
        (-b - sqrt(d))/(2.0*a),
        (-b + sqrt(d))/(2.0*a)
    );
}

vec3 SampleHLUT(sampler2D tex,
				vec3 pos,
				vec3 sunDir,
                float groundRadius,
				float atmosphereRadius)
{
  float height = length(pos);
  vec3 up = pos / height;

  float sunCosZenithAngle = dot(sunDir, up);

  float u = clamp(0.5 + 0.5 * sunCosZenithAngle, 0.0, 1.0);
  float v = max(0.0, min(1.0, (height - groundRadius) / (atmosphereRadius - groundRadius)));

  return textureLod(tex, vec2(u, v), 0.0).rgb;
}

vec3 SampleSkyViewLUT(sampler2D lut,
					  vec3 viewPos,
					  vec3 viewDir,
                      vec3 sunDir,
					  float groundRadius)
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
  float v = 0.5 + 0.5 * sign(altitudeAngle) * sqrt(abs(altitudeAngle) / PI_OVER_TWO);

  return textureLod(lut, vec2(u, v), 0.0).rgb;
}

vec3 ComputeSunLuminance(vec3 e, vec3 s, float size, float intensity)
{
	float sunSolidAngle = size * PI * 0.005555556; // 1.0 / 180.0
	float minSunCosTheta = cos(sunSolidAngle);
	
	float cosTheta = dot(s, e);
	float angle = ArcCos(cosTheta);
	float radiusRatio = angle / sunSolidAngle;
	float limbDarkening = sqrt(clamp(1.0 - radiusRatio * radiusRatio, 0.0001, 1.0));
	
	float comp = float(cosTheta >= minSunCosTheta);
	return intensity * comp * vec3(1.0) * limbDarkening;
}

// ------------------------------------------------------------------
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
	float altitude = (length(pos) - groundRadius) * 1000.0; // convert into KM
	float rayleighDensity = exp(-altitude / params.RayleighScattering.w);

	return params.RayleighScattering.rgb * rayleighDensity;
}

vec3 ComputeMieScattering(vec3 pos, ScatteringParams params, float groundRadius)
{
	float altitude = (length(pos) - groundRadius) * 1000.0; // convert into KM
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

	vec3 ozoneAbsorption = params.OzoneAbsorption.w * params.OzoneAbsorption.rgb * max(0.0, 1.0 - abs(altitude - 25.0) / 15.0);

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
	vec2 atmosIntercept = RayIntersectSphere2D(pos, rayDir, atmoRadius);
    float terraIntercept = RayIntersectSphere(pos, rayDir, groundRadius);

	float mindist, maxdist;

	if (atmosIntercept.x < atmosIntercept.y){
        // there is an atmosphere intercept!
        // start at the closest atmosphere intercept
        // trace the distance between the closest and farthest intercept
        mindist = atmosIntercept.x > 0.0 ? atmosIntercept.x : 0.0;
		maxdist = atmosIntercept.y > 0.0 ? atmosIntercept.y : 0.0;
    } else {
        // no atmosphere intercept means no atmosphere!
        return vec3(0.0);
    }

	// if in the atmosphere start at the camera
    if (length(pos) < atmoRadius) mindist=0.0;

	

	// if there's a terra intercept that's closer than the atmosphere one,
    // use that instead!
    if (terraIntercept > 0.0){ // confirm valid intercepts			
        maxdist = terraIntercept;
    }

	// start marching at the min dist
    pos = pos + mindist * rayDir;


	float cosTheta = dot(rayDir, sunDir);
	
	float miePhaseValue = GetMiePhase(cosTheta);
	float rayleighPhaseValue = GetRayleighPhase(-cosTheta);

	vec3 luminance = vec3(0.0);
	vec3 transmittance = vec3(1.0);
	float t = 0.0;

	
#define NUM_STEPS 16

	for(float i = 0.0; i < float(NUM_STEPS); i += 1.0)
	{
		//float newT = ((i + 0.3) / float(NUM_STEPS)) * tMax;
		float newT = ((i + 0.3) / float(NUM_STEPS)) * clamp(maxdist-mindist, 0.0f, 1.5f);
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
	vec3 color = vec3(0.0f);

	switch(int(m_CameraData.SkyMode))
	{
		case SKYBOX_MODE_HDRMAP:
		{
			vec3 texCoord = vec3(v_FragmentPos.x, v_FragmentPos.y, v_FragmentPos.z);

			// Getting the color from the cubemap	
			vec3 envColor = textureLod(u_EnvTexture, texCoord, m_CameraData.Lod).rgb;
			color = envColor * m_CameraData.Exposure;	

			// Darken the sky, it is too bright!
			//color *= 0.5f;

			break;
		}

		case SKYBOX_MODE_HILLAIRE:
		{
			vec3 sunPos = normalize(m_CameraData.SunDir.xyz);
			sunPos *= -1.0;

			vec3 viewDir = normalize(v_FragmentPos);
			
			float sunIntensity = m_CameraData.SunIntensity;
			float sunSize = m_CameraData.SunSize;

			//float skyIntensity = m_CameraData.SkyIntensity;
			vec3 viewPos = m_CameraData.ViewPos; 

			float groundRadius = m_CameraData.GroundRadius;
			float atmosphereRadius = m_CameraData.AtmosphereRadius;

			if(raySphereIntersect(viewPos, viewDir, vec3(0.0f), groundRadius) < 0.0f)
			{
				color = ComputeSunLuminance(viewDir, sunPos, sunSize, sunIntensity);
				
			}


			vec3 groundTrans = SampleHLUT(
				u_TransmittanceLUT,
				viewPos,
				sunPos,
				groundRadius,
				atmosphereRadius
			);

			vec3 spaceTrans = SampleHLUT(
				u_TransmittanceLUT,
				vec3(0.0, groundRadius, 0.0),
				vec3(0.0, 1.0, 0.0),
				groundRadius,
                atmosphereRadius
			);


			float comp = float(RayIntersectSphere(viewPos, viewDir, groundRadius) < 0.0);

			//color *= comp * groundTrans / spaceTrans;


			float skyIntensity = m_CameraData.SunIntensity;

			if (length(viewPos) < atmosphereRadius * 1.0)
			{
				color += SampleSkyViewLUT(
					u_HillaireLUT,
					viewPos,
					viewDir,
					sunPos,
					groundRadius
				);
			}
			else
			{

				ScatteringParams params;
				params.RayleighScattering = m_CameraData.RayleighScattering;
				params.RayleighAbsorption = m_CameraData.RayleighAbsorption;
				params.MieScattering = m_CameraData.MieScattering;
				params.MieAbsorption = m_CameraData.MieAbsorption;
				params.OzoneAbsorption = m_CameraData.OzoneAbsorption;

				// As mentioned in section 7 of the paper, switch to direct raymarching outside atmosphere
				vec3 luminance = RaymarchScattering(
										viewPos,
										viewDir,
										sunPos,
										0.0,
										params,
										groundRadius,
										atmosphereRadius
				);

				color += min(max(vec3(0.0f), luminance), vec3(1.0f));
        
			}

			color *= skyIntensity;
			

			color *= m_CameraData.Exposure;

			color *= 2.0f;

			break;
		}
	}

	// Outputting the color
	o_Color = vec4(color, 1.0);
}