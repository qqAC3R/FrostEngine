#type vertex
#version 450
#extension GL_ARB_separate_shader_objects : enable

//layout(location = 0) in vec3 a_Position;

layout(push_constant) uniform Constants
{
	mat4 ProjectionMatrix;
	mat4 ViewMatrix;
} u_PushConstant;

layout(location = 0) out vec3 v_FragmentPos;

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
	
	gl_Position = clipPosition.xyww;
}

#type fragment
#version 450
#extension GL_ARB_separate_shader_objects : enable

// Output
layout(location = 0) out vec4 o_Color;

// Input
layout(location = 0) in vec3 v_FragmentPos;

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

} m_CameraData;


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

void main()
{
	vec3 color = vec3(0.0f);

	switch(int(m_CameraData.SkyMode))
	{
		case SKYBOX_MODE_HDRMAP:
		{
			// Getting the color from the cubemap	
			vec3 envColor = textureLod(u_EnvTexture, v_FragmentPos, m_CameraData.Lod).rgb;
			color = envColor * m_CameraData.Exposure;	

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

			color = ComputeSunLuminance(viewDir, sunPos, sunSize, sunIntensity);


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

			color *= comp * groundTrans / spaceTrans;

			float skyIntensity = 3.0f;
			color += skyIntensity * SampleSkyViewLUT(
				u_HillaireLUT,
				viewPos,
				viewDir,
				sunPos,
				groundRadius
			);

			color *= m_CameraData.Exposure;

			break;
		}
	}

	// Outputting the color
	o_Color = vec4(color, 1.0);
}