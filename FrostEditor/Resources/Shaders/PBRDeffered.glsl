#type vertex
#version 450

layout(location = 0) out vec3 v_Position;
layout(location = 1) out vec2 v_TexCoord;

void main()
{
    // Rendering a fullscreen quad using only 3 vertices
    // Credit: https://wallisc.github.io/rendering/2021/04/18/Fullscreen-Pass.html
    v_TexCoord = vec2(((gl_VertexIndex << 1) & 2), gl_VertexIndex & 2);
    v_Position = vec3((v_TexCoord * vec2(2, -2) + vec2(-1, 1)), 0.0f);

    // Setting up the vertex position in NDC. The y is reverted because the formula from above was made for HLSL
    gl_Position = vec4(v_Position.x, -v_Position.y, 0.0f, 1.0f);
}

#type fragment
#version 450

// Constants
const float PI = 3.141592;
const float Epsilon = 0.00001;

// Constant normal incidence Fresnel factor for all dielectrics.
const vec3 Fdielectric = vec3(0.04);


struct PointLight
{
    vec3 Position;
    vec3 Radiance;
    float Radius;
    float Falloff;
};

struct DirectionalLight
{
	vec3 Direction;
    vec3 Radiance;
	float Multiplier;
};

// Output color
layout(location = 0) out vec4 o_Color;

// Data from the vertex shader
layout(location = 0) in vec3 v_Position;
layout(location = 1) in vec2 v_TexCoord;

// Data from the material system
layout(binding = 0) uniform sampler2D u_PositionTexture;
layout(binding = 1) uniform sampler2D u_AlbedoTexture;
layout(binding = 2) uniform sampler2D u_NormalTexture;
layout(binding = 3) uniform sampler2D u_CompositeTexture;
layout(binding = 4) uniform LightData {
    uint u_LightCount;
    PointLight u_PointLights[1];
};

layout(push_constant) uniform PushConstant {
    vec3 CameraPosition;
} u_PushConstant;

struct SurfaceProperties
{
    vec3 WorldPos;
    vec3 Normal;
    vec3 ViewVector;
    
    vec3 Albedo;
    float Roughness;
    float Metalness;

    vec3 F0;
} m_Surface;



float NDFGGX(float cosLh, float roughness)
{
	float alpha = roughness * roughness;
	float alphaSq = alpha * alpha;

	float denom = (cosLh * cosLh) * (alphaSq - 1.0) + 1.0;
	return alphaSq / (PI * denom * denom);
}

vec3 FresnelSchlickRoughness(vec3 F0, float cosTheta, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

// Single term for separable Schlick-GGX below.
float GeometrySchlickG1(float cosTheta, float k)
{
	return cosTheta / (cosTheta * (1.0 - k) + k);
}

// Schlick-GGX approximation of geometric attenuation function using Smith's method.
float GeometrySchlickGGX(float cosLi, float NdotV, float roughness)
{
	float r = roughness + 1.0;
	float k = (r * r) / 8.0; // Epic suggests using this roughness remapping for analytic lights.
	return GeometrySchlickG1(cosLi, k) * GeometrySchlickG1(NdotV, k);
}

vec3 PointLightContribution(PointLight pointLight)
{
    vec3 Lc = vec3(0.0);
    {
        vec3 LightPosition = pointLight.Position;

        vec3  Li = normalize(LightPosition - m_Surface.WorldPos);     //  Light direction
        vec3  Lh  = normalize(m_Surface.ViewVector + Li);             //  Half view vector
        float Ld = length(LightPosition - m_Surface.WorldPos);        //  Light distance

        float attenuation = clamp(1.0 - (Ld * Ld) / (pointLight.Radius * pointLight.Radius), 0.0, 1.0);
		attenuation *= mix(attenuation, 1.0, pointLight.Falloff);

        vec3 LRadiance = pointLight.Radiance * attenuation;

        // Calculate angles between surface normal and various light vectors.
		float cosLi = max(0.0, dot(m_Surface.Normal, Li));
		float cosLh = max(0.0, dot(m_Surface.Normal, Lh));


        float NdotV = dot(m_Surface.Normal, m_Surface.ViewVector);

        // Cook-Torrance BRDF
        vec3 F   = FresnelSchlickRoughness(m_Surface.F0, max(dot(Lh, m_Surface.ViewVector), 0.0), m_Surface.Roughness);       
        float D  = NDFGGX(cosLh, m_Surface.Roughness);        
        float G  = GeometrySchlickGGX(cosLi, NdotV, m_Surface.Roughness);      
        

        vec3 kd = (1.0 - F) * (1.0 - m_Surface.Metalness);
		vec3 diffuseBRDF = kd * m_Surface.Albedo;

        
		// Specular
		vec3 specularBRDF = (F * D * G) / max(Epsilon, 4.0 * cosLi * NdotV);
		specularBRDF = clamp(specularBRDF, vec3(0.0f), vec3(10.0f));
		Lc += (diffuseBRDF + specularBRDF) * LRadiance * cosLi;
    }
    return Lc;
}

vec3 DirectionalLightContribution(DirectionalLight directionLight)
{
	vec3 result = vec3(0.0);

	{
		vec3 Li = directionLight.Direction;
		vec3 Lradiance = directionLight.Radiance * directionLight.Multiplier;
		vec3 Lh = normalize(Li + m_Surface.ViewVector);

		// Calculate angles between surface normal and various light vectors.
		float cosLi = max(0.0, dot(m_Surface.Normal, Li));
		float cosLh = max(0.0, dot(m_Surface.Normal, Lh));

		float NdotV = dot(m_Surface.Normal, m_Surface.ViewVector);

		vec3 F = FresnelSchlickRoughness(m_Surface.F0, max(0.0, dot(Lh, m_Surface.ViewVector)), m_Surface.Roughness);
		float D = NDFGGX(cosLh, m_Surface.Roughness);
		float G = GeometrySchlickGGX(cosLi, NdotV, m_Surface.Roughness);

		vec3 kd = (1.0 - F) * (1.0 - m_Surface.Metalness);
		vec3 diffuseBRDF = kd * m_Surface.Albedo;

		// Cook-Torrance
		vec3 specularBRDF = (F * D * G) / max(Epsilon, 4.0 * cosLi * NdotV);
		specularBRDF = clamp(specularBRDF, vec3(0.0f), vec3(10.0f));
		result += (diffuseBRDF + specularBRDF) * Lradiance * cosLi;
	}
	
	return result;
}

void main()
{
    // Calculating the normals and worldPos
    m_Surface.WorldPos =    texture(u_PositionTexture, v_TexCoord).rgb;
    m_Surface.Normal =      normalize(texture(u_NormalTexture, v_TexCoord).rgb);
    m_Surface.ViewVector =  normalize(u_PushConstant.CameraPosition - m_Surface.WorldPos);

    // Sampling the textures from gbuffer
    m_Surface.Albedo =      texture(u_AlbedoTexture, v_TexCoord).rgb;
    m_Surface.Metalness =   texture(u_CompositeTexture, v_TexCoord).r;
    m_Surface.Roughness =   texture(u_CompositeTexture, v_TexCoord).g;

    // Fresnel approximation
	m_Surface.F0 = mix(Fdielectric, m_Surface.Albedo, m_Surface.Metalness);


    PointLight pointLight;
	pointLight.Position = vec3(5.0f, 9.0f, 3.0f);
	pointLight.Radiance = vec3(1.0f);
	pointLight.Radius = 40.0f;
	pointLight.Falloff = 1.0f;

	DirectionalLight dirLight;
	dirLight.Direction = vec3(0.9f, 0.5f, 0.9f);
	dirLight.Radiance = vec3(1.0f);
	dirLight.Multiplier = 0.05f;

	vec3 Lo = PointLightContribution(pointLight);
	vec3 Ld = DirectionalLightContribution(dirLight);

    vec3 result = Lo + Ld;

    o_Color = vec4(result, 1.0f);
}