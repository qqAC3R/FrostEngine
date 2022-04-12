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
#extension GL_EXT_scalar_block_layout : enable

// Constants
const float PI = 3.141592;
const float Epsilon = 0.00001;

// Constant normal incidence Fresnel factor for all dielectrics.
const vec3 Fdielectric = vec3(0.04);


struct PointLight
{
    vec3 Radiance;
	float Intensity;
    float Radius;
    float Falloff;
    vec3 Position;
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
//layout(binding = 3) uniform sampler2D u_CompositeTexture;
layout(binding = 4, scalar) readonly buffer u_LightData { // Using scalar, it will fix our byte padding issues?
	PointLight u_PointLights[];
} LightData;

layout(binding = 10, scalar) readonly buffer u_VisibleLightData { // Using scalar, it will fix our byte padding issues?
	int Indices[];
} VisibleLightData;

layout(binding = 6) uniform samplerCube u_RadianceFilteredMap;
layout(binding = 7) uniform samplerCube u_IrradianceMap;
layout(binding = 8) uniform sampler2D u_BRDFLut;

layout(binding = 9) uniform CameraData {
	float Gamma;
	float Exposure;
	float PointLightCount;
	float LightCullingWorkgroup;
} m_CameraData;


layout(push_constant) uniform PushConstant {
    vec4 CameraPosition; // vec4 - camera position + float pointLightCount + lightCullingWorkgroup.x
	float UseLightHeatMap;
} u_PushConstant;


struct SurfaceProperties
{
    vec3 WorldPos;
    vec3 Normal;
    vec3 ViewVector;
    
    vec3 Albedo;
    float Roughness;
    float Metalness;
    float Emission;

    vec3 F0;
} m_Surface;



int GetLightBufferIndex(int i)
{
    ivec2 tileID = ivec2(gl_FragCoord.xy) / ivec2(16, 16); //Current Fragment position / Tile count
    uint index = tileID.y * uint(m_CameraData.LightCullingWorkgroup) + tileID.x;

    uint offset = index * 1024;
    return VisibleLightData.Indices[offset + i];
}


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
        vec3 LightPosition = vec3(pointLight.Position);

        vec3  Li = normalize(LightPosition - m_Surface.WorldPos);     //  Light direction
        vec3  Lh  = normalize(m_Surface.ViewVector + Li);             //  Half view vector
        float Ld = length(LightPosition - m_Surface.WorldPos);        //  Light distance

        float attenuation = clamp(1.0 - (Ld * Ld) / (pointLight.Radius * pointLight.Radius), 0.0, 1.0);
		attenuation *= mix(attenuation, 1.0, pointLight.Falloff);

        vec3 LRadiance = vec3(pointLight.Radiance) * attenuation;

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

vec3 RotateVectorByY(float angle, vec3 vec)
{
	angle = radians(angle);
	mat3x3 rotationMatrix = { vec3(cos(angle),0.0,sin(angle)),
							vec3(0.0,1.0,0.0),
							vec3(-sin(angle),0.0,cos(angle)) };
	return rotationMatrix * vec;
}

vec3 IBL_Contribution()
{
	float NdotV = dot(m_Surface.Normal, m_Surface.ViewVector);
	// Specular reflection vector
	vec3 Lr = 2.0 * NdotV * m_Surface.Normal - m_Surface.ViewVector;


	vec3 irradiance = texture(u_IrradianceMap, m_Surface.Normal).rgb;
	vec3 F = FresnelSchlickRoughness(m_Surface.F0, NdotV, m_Surface.Roughness);
	vec3 kd = (1.0 - F) * (1.0 - m_Surface.Metalness);
	vec3 diffuseIBL = m_Surface.Albedo * irradiance;

	int envRadianceTexLevels = textureQueryLevels(u_RadianceFilteredMap);
	float NoV = clamp(NdotV, 0.0, 1.0);
	vec3 R = 2.0 * dot(m_Surface.ViewVector, m_Surface.Normal) * m_Surface.Normal - m_Surface.ViewVector;
	vec3 specularIrradiance = textureLod(u_RadianceFilteredMap, RotateVectorByY(0.0f, Lr), (m_Surface.Roughness) * envRadianceTexLevels).rgb;

	// Sample BRDF Lut, 1.0 - roughness for y-coord because texture was generated (in Sparky) for gloss model
	vec2 specularBRDF = texture(u_BRDFLut, vec2(NdotV, 1.0 - m_Surface.Roughness)).rg;
	vec3 specularIBL = specularIrradiance * (m_Surface.F0 * specularBRDF.x + specularBRDF.y);
	//vec3 specularIBL = specularIrradiance * (m_Surface.F0);

	return kd * diffuseIBL + specularIBL;
}

// From https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
vec3 AcesApprox(vec3 v)
{
    v *= 0.6f;
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((v*(a*v+b))/(v*(c*v+d)+e), 0.0f, 1.0f);
}

// https://aras-p.info/texts/CompactNormalStorage.html
vec3 DecodeNormals(vec2 enc)
{
    float scale = 1.7777f;
    vec3 nn = vec3(enc, 1.0f) * vec3(2 * scale, 2 * scale,0) + vec3(-scale, -scale,1);
    
	float g = 2.0 / dot(nn.xyz,nn.xyz);

    vec3 n;
    n.xy = g*nn.xy;
    n.z = g-1;

	n = clamp(n, vec3(-1.0f), vec3(1.0f));

    return n;
}

void main()
{
    // Calculating the normals and worldPos
    m_Surface.WorldPos =    texture(u_PositionTexture, v_TexCoord).rgb;
    m_Surface.ViewVector =  normalize(vec3(u_PushConstant.CameraPosition) - m_Surface.WorldPos);

	// Decode normals
	vec2 encodedNormals =   texture(u_NormalTexture, v_TexCoord).rg;
    m_Surface.Normal =      DecodeNormals(encodedNormals);


    // Sampling the textures from gbuffer
    m_Surface.Albedo =      texture(u_AlbedoTexture, v_TexCoord).rgb;
    m_Surface.Metalness =   texture(u_NormalTexture, v_TexCoord).b;
    m_Surface.Roughness =   texture(u_NormalTexture, v_TexCoord).a;
    m_Surface.Emission =    texture(u_AlbedoTexture, v_TexCoord).a;

    // Fresnel approximation
	m_Surface.F0 = mix(Fdielectric, m_Surface.Albedo, m_Surface.Metalness);


	DirectionalLight dirLight;
	dirLight.Direction = vec3(0.9f, 0.5f, 0.9f);
	dirLight.Radiance = vec3(1.0f);
	dirLight.Multiplier = 0.05f;

	// Calculating all point lights contribution
	vec3 Lo = vec3(0.0f);
	uint pointLightCount = uint(m_CameraData.PointLightCount);


	float heatMap = 0.0f;
	for(uint i = 0; i < pointLightCount; i++)
	{
		int lightIndex = GetLightBufferIndex(int(i));
        if (lightIndex == -1)
            break;

		PointLight pointLight = LightData.u_PointLights[lightIndex];
		Lo += PointLightContribution(pointLight) * pointLight.Intensity;

		heatMap += 0.1f;
	}


	// Calculating all directional lights contribution
	vec3 Ld = DirectionalLightContribution(dirLight);

	// Adding up the point light and directional light contribution
    vec3 result = Lo + Ld;

	// Adding up the ibl
	float IBLIntensity = 1.0f;
	result += IBL_Contribution() * IBLIntensity;

	result *= (1.0f + m_Surface.Emission);
	result *= m_CameraData.Exposure;

	// Outputting the result
    o_Color = vec4(result, 1.0f);

	if(u_PushConstant.UseLightHeatMap == 1.0f)
		o_Color.rgb += + vec3(heatMap);
}