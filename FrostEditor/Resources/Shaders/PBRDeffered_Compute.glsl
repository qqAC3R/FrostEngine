#type compute
#version 450
#extension GL_EXT_scalar_block_layout : enable

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

// Constants
const float PI = 3.141592;
const float Epsilon = 0.00001;

// Constant normal incidence Fresnel factor for all dielectrics.
const vec3 Fdielectric = vec3(0.04);

const float LtcLutTextureSize = 64.0; // ltc_texture size
const float LtcLutTextureScale = (LtcLutTextureSize - 1.0) / LtcLutTextureSize;
const float LtcLutTextureBias  = 0.5 / LtcLutTextureSize;

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

struct RectangularLight
{
	vec3 Radiance;
	float Intensity;
	vec4 Vertex0; // W component stores the TwoSided bool
	vec4 Vertex1; // W component stores the Radius for it to be culled
	vec4 Vertex2;
	vec4 Vertex3;
};

// Output color
//layout(location = 1) out vec4 o_Color;
layout(binding = 0, rgba16f) uniform writeonly image2D o_Image;

// Data from the material system
//layout(binding = 1) uniform sampler2D u_PositionTexture;
layout(binding = 1) uniform sampler2D u_DepthBuffer;
layout(binding = 2) uniform sampler2D u_AlbedoTexture;
layout(binding = 3) uniform sampler2D u_NormalTexture;

// Point lights
layout(binding = 4, scalar) readonly buffer u_PointLightData { // Using scalar, it will fix our byte padding issues?
	PointLight u_PointLights[];
} PointLightData;

layout(binding = 5) readonly buffer u_VisiblePointLightData {
	int Indices[];
} VisiblePointLightData;

// PBR
layout(binding = 6) uniform samplerCube u_RadianceFilteredMap;
layout(binding = 7) uniform samplerCube u_IrradianceMap;
layout(binding = 8) uniform sampler2D u_BRDFLut;

// Camera Information/other stuff
layout(binding = 9) uniform UniformBuffer
{
	float CameraGamma;
	float CameraExposure;
	float PointLightCount;
	float LightCullingWorkgroup;
	float RectangularLightCount;
} u_UniformBuffer;

// Voxel Cone Tracing
layout(binding = 10) uniform sampler2D u_VoxelIndirectDiffuseTex;
layout(binding = 11) uniform sampler2D u_VoxelIndirectSpecularTex;

// Shadow depth texture
layout(binding = 12) uniform sampler2D u_ShadowTexture;

// Rectangular lights
layout(binding = 13) readonly buffer u_RectangularLightData {
	RectangularLight u_RectLights[];
} RectangularLightData;

layout(binding = 14) readonly buffer u_VisibleRectLightData {
	int Indices[];
} VisibleRectLightData;

layout(binding = 15) uniform sampler2D u_LTC1Lut;
layout(binding = 16) uniform sampler2D u_LTC2Lut;


// Push constants (general information)
layout(push_constant) uniform PushConstant
{
	mat4 InvViewProjection;
    vec4 CameraPosition; // vec4 - camera position + float pointLightCount + lightCullingWorkgroup.x
	vec3 DirectionalLightDir;
	float DirectionalLightIntensity;
	int UseLightHeatMap;
	
} u_PushConstant;



// Global variables
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

vec3 s_IndirectDiffuse = vec3(1.0f);
vec3 s_IndirectSpecular = vec3(1.0f);


// =======================================================
int GetPointLightBufferIndex(int i)
{
    ivec2 tileID = ivec2(gl_GlobalInvocationID.xy) / ivec2(16, 16); //Current Fragment position / Tile count
    uint index = tileID.y * uint(u_UniformBuffer.LightCullingWorkgroup) + tileID.x;

    uint offset = index * 1024;
    return VisiblePointLightData.Indices[offset + i];
}
// =======================================================
int GetRectangularLightBufferIndex(int i)
{
    ivec2 tileID = ivec2(gl_GlobalInvocationID.xy) / ivec2(16, 16); //Current Fragment position / Tile count
    uint index = tileID.y * uint(u_UniformBuffer.LightCullingWorkgroup) + tileID.x;

    uint offset = index * 1024;
    return VisibleRectLightData.Indices[offset + i];
}
// =======================================================
vec4 UpsampleTent9(sampler2D tex, float lod, vec2 uv, float radius)
{
	vec2 texSize = vec2(textureSize(tex, int(lod)));
    vec2 texelSize = 1.0f / texSize;

    vec4 offset = texelSize.xyxy * vec4(1.0f, 1.0f, -1.0f, 0.0f) * radius;

    // Center
	vec4 center = textureLod(tex, uv, lod);
    vec3 result = center.rgb * 4.0f;                          //  (0, 0)

    result += textureLod(tex, uv - offset.xy, lod).rgb;       // -( 1,  1 ) // 1
    result += textureLod(tex, uv - offset.wy, lod).rgb * 2.0; // -( 0,  1 ) // 2
    result += textureLod(tex, uv - offset.zy, lod).rgb;       // -(-1,  1 ) // 3

    result += textureLod(tex, uv + offset.zw, lod).rgb * 2.0; // +(-1,  0 ) // 4
    result += textureLod(tex, uv + offset.xw, lod).rgb * 2.0; // +( 1,  0 ) // 5

    result += textureLod(tex, uv + offset.zy, lod).rgb;       // +(-1,  1 ) // 6
    result += textureLod(tex, uv + offset.wy, lod).rgb * 2.0; // +( 0,  1 ) // 7
    result += textureLod(tex, uv + offset.xy, lod).rgb;       // +( 1,  1 ) // 8

    // Sampling order (C = center)
    //   6 7 8
    //   4 C 5
    //   3 2 1

    return vec4(result * (1.0f / 16.0f), center.a);
}

// =======================================================

//------------------------------------------------------------------------------
// Filament PBR.
//------------------------------------------------------------------------------

// Normal distribution function.
float nGGX(float nDotH, float actualRoughness)
{
	float a = nDotH * actualRoughness;
	float k = actualRoughness / (1.0 - nDotH * nDotH + a * a);
	return k * k * (1.0 / PI);
}

// Fast visibility term. Incorrect as it approximates the two square roots.
float vGGXFast(float nDotV, float nDotL, float actualRoughness)
{
	float a = actualRoughness;
	float vVGGX = nDotL * (nDotV * (1.0 - a) + a);
	float lVGGX = nDotV * (nDotL * (1.0 - a) + a);
	return 0.5 / max(vVGGX + lVGGX, 1e-5);
}

// Schlick approximation for the Fresnel factor.
vec3 sFresnel(float vDotH, vec3 f0, vec3 f90)
{
	float p = 1.0 - vDotH;
	return f0 + (f90 - f0) * p * p * p * p * p;
}

// Cook-Torrance specular for the specular component of the BRDF.
vec3 Fs_CookTorrance(float nDotH,
					float lDotH,
					float nDotV,
					float nDotL,
                    float vDotH,
					float actualRoughness,
					vec3 f0,
					vec3 f90)
{
	float D = nGGX(nDotH, actualRoughness);
	vec3 F = sFresnel(vDotH, f0, f90);
	float V = vGGXFast(nDotV, nDotL, actualRoughness);
	return D * F * V;
}

//   for the diffuse component of the BRDF. Corrected to guarantee
// energy is conserved.
vec3 Fd_LambertCorrected(vec3 f0, vec3 f90, float vDotH, float lDotH,
                        vec3 diffuseAlbedo)
{
	// Making the assumption that the external medium is air (IOR of 1).
	vec3 iorExtern = vec3(1.0);
	// Calculating the IOR of the medium using f0.
	vec3 iorIntern = (vec3(1.0) - sqrt(f0)) / (vec3(1.0) + sqrt(f0));
	// Ratio of the IORs.
	vec3 iorRatio = iorExtern / iorIntern;
	
	// Compute the incoming and outgoing Fresnel factors.
	vec3 fIncoming = sFresnel(lDotH, f0, f90);
	vec3 fOutgoing = sFresnel(vDotH, f0, f90);
	
	// Compute the fraction of light which doesn't get reflected back into the
	// medium for TIR.
	vec3 rExtern = PI * (20.0 * f0 + 1.0) / 21.0;
	
	// Use rExtern to compute the fraction of light which gets reflected back into
	// the medium for TIR.
	vec3 rIntern = vec3(1.0) - (iorRatio * iorRatio * (vec3(1.0) - rExtern));
	
	// The TIR contribution.
	vec3 tirDiffuse = vec3(1.0) - (rIntern * diffuseAlbedo);
	
	// The final diffuse BRDF.
	return (iorRatio * iorRatio) * diffuseAlbedo * (vec3(1.0) - fIncoming) * (vec3(1.0) - fOutgoing) / (PI * tirDiffuse);
}

vec3 ComputePointLightContribution(PointLight pointLight)
{
	vec3 LightPosition = vec3(pointLight.Position);

	// Get the values from the surfaces
	vec3 albedo = m_Surface.Albedo.rgb;
	float roughness = m_Surface.Roughness;
	float metalness = m_Surface.Metalness;

	// Compute some vectors needed for calculations
	vec3 V = m_Surface.ViewVector;
	vec3 N = m_Surface.Normal;

	vec3  Ldir = normalize(LightPosition - m_Surface.WorldPos);  //  Light direction
	vec3  Lh   = normalize(V + Ldir);                            //  Half view vector

	float nDotV = max(abs(dot(N,    V)),   1e-5);
	float nDotL =   clamp(dot(N,    Ldir), 1e-5, 1.0);
	float nDotH =   clamp(dot(N,    Lh),   1e-5, 1.0);
	float lDotH =   clamp(dot(Ldir, Lh),   1e-5, 1.0);
	float vDotH =   clamp(dot(Ldir, Lh),   1e-5, 1.0);

	// Attenuation
	float Ld          = length(LightPosition - m_Surface.WorldPos); //  Light distance
	float attenuation = clamp(1.0 - (Ld * Ld) / (pointLight.Radius * pointLight.Radius), 0.0, 1.0);
	attenuation      *= mix(attenuation, 1.0, pointLight.Falloff);

	// Compute the real roughness
	float clampedRoughness = max(roughness, 0.045); // Frostbite uses a clamped factor for less artifacts
	float actualRoughness = clampedRoughness * clampedRoughness;

	// Sample the BRDF
	vec2 DFG = texture(u_BRDFLut, vec2(nDotV, roughness)).rg;
	DFG = max(DFG, vec2(1e-4));

	// Compute the fresnel at angle 0 for the metallic and dielectric 
	vec3 metallicF0 = albedo * metalness;
	vec3 dielectricF0 = 0.16 * vec3(1.0f); // instead of "vec3(1.0f)" it should be "vec3(m_Surface.Albedo.a * m_Surface.Albedo.a)"
	vec3 F90 = vec3(1.0); // Dirty, setting f90 to 1.0.

	vec3 energyDielectric = vec3(1.0) + dielectricF0 * (vec3(1.0) / DFG.y - vec3(1.0));
	vec3 energyMetallic =   vec3(1.0) + metallicF0   * (vec3(1.0) / DFG.y - vec3(1.0));


	// Computing the dielectric BRDF
	vec3 Fs = Fs_CookTorrance(nDotH, lDotH, nDotV, nDotL, vDotH, actualRoughness, dielectricF0, F90);
	vec3 Fd = Fd_LambertCorrected(dielectricF0, F90, vDotH, lDotH, albedo);
	vec3 dielectricBRDF = Fs + Fd;

	// Computing the metallic BRDF (we don't use the diffuse term since metallic surfaces dont absorb rays)
	vec3 metallicBRDF = Fs_CookTorrance(nDotH, lDotH, nDotV, nDotL, vDotH, actualRoughness, metallicF0, F90);

	// Compute the final contribution between the metallic and dielectric brdf
	vec3 finalContribution = mix(dielectricBRDF, metallicBRDF, metalness);
	return finalContribution * pointLight.Intensity * pointLight.Radiance * nDotL * attenuation;
}

vec3 ComputeDirectionalLightContribution(DirectionalLight directionalLight)
{
	// Get the values from the surfaces
	vec3 albedo = m_Surface.Albedo.rgb;
	float roughness = m_Surface.Roughness;
	float metalness = m_Surface.Metalness;

	// Compute some vectors needed for calculations
	vec3 V = m_Surface.ViewVector;
	vec3 N = m_Surface.Normal;

	vec3  Ldir = normalize(directionalLight.Direction);  //  Light direction
	vec3  Lh   = normalize(V + Ldir);                    //  Half view vector

	float nDotV = max(abs(dot(N,    V)),   1e-5);
	float nDotL =   clamp(dot(N,    Ldir), 1e-5, 1.0);
	float nDotH =   clamp(dot(N,    Lh),   1e-5, 1.0);
	float lDotH =   clamp(dot(Ldir, Lh),   1e-5, 1.0);
	float vDotH =   clamp(dot(Ldir, Lh),   1e-5, 1.0);

	// Compute the real roughness
	float clampedRoughness = max(roughness, 0.045); // Frostbite uses a clamped factor for less artifacts
	float actualRoughness = clampedRoughness * clampedRoughness;

	// Sample the BRDF
	vec2 DFG = texture(u_BRDFLut, vec2(nDotV, roughness)).rg;
	DFG = max(DFG, vec2(1e-4));

	// Compute the fresnel at angle 0 for the metallic and dielectric 
	vec3 metallicF0 = albedo * metalness;
	vec3 dielectricF0 = 0.16 * vec3(1.0f); // instead of "vec3(1.0f)" it should be "vec3(m_Surface.Albedo.a * m_Surface.Albedo.a)"
	vec3 F90 = vec3(1.0); // Dirty, setting f90 to 1.0.

	vec3 energyDielectric = vec3(1.0) + dielectricF0 * (vec3(1.0) / DFG.y - vec3(1.0));
	vec3 energyMetallic =   vec3(1.0) + metallicF0   * (vec3(1.0) / DFG.y - vec3(1.0));

	// Computing the dielectric BRDF
	vec3 Fs = Fs_CookTorrance(nDotH, lDotH, nDotV, nDotL, vDotH, actualRoughness, dielectricF0, F90);
	vec3 Fd = Fd_LambertCorrected(dielectricF0, F90, vDotH, lDotH, albedo);
	vec3 dielectricBRDF = Fs + Fd;

	// Computing the metallic BRDF (we don't use the diffuse term since metallic surfaces dont absorb rays)
	vec3 metallicBRDF = Fs_CookTorrance(nDotH, lDotH, nDotV, nDotL, vDotH, actualRoughness, metallicF0, F90);

	// Compute the final contribution between the metallic and dielectric brdf
	vec3 finalContribution = mix(dielectricBRDF, metallicBRDF, metalness);
	return finalContribution * directionalLight.Multiplier * nDotL * directionalLight.Radiance;
}


// Vector form without project to the plane (dot with the normal)
// Use for proxy sphere clipping
vec3 IntegrateEdgeVec(vec3 v1, vec3 v2)
{
    // Using built-in acos() function will result flaws
    // Using fitting result for calculating acos()
    float x = dot(v1, v2);
    float y = abs(x);

    float a = 0.8543985 + (0.4965155 + 0.0145206*y)*y;
    float b = 3.4175940 + (4.1616724 + y)*y;
    float v = a / b;

    float theta_sintheta = (x > 0.0) ? v : 0.5*inversesqrt(max(1.0 - x*x, 1e-7)) - v;

    return cross(v1, v2)*theta_sintheta;
}
vec3 EvaluateLTC(mat3 invM, RectangularLight rectLight)
{
	vec3 T1, T2;
	T1 = normalize(m_Surface.ViewVector - m_Surface.Normal * dot(m_Surface.ViewVector, m_Surface.Normal));
	T2 = cross(m_Surface.Normal, T1);

	invM = invM * transpose(mat3(T1, T2, m_Surface.Normal));

	// polygon (allocate 4 vertices for clipping)
	vec3 L[4];
	// transform polygon from LTC back to origin D0 (cosine weighted)
	L[0] = invM * (rectLight.Vertex0.xyz - m_Surface.WorldPos);
	L[1] = invM * (rectLight.Vertex1.xyz - m_Surface.WorldPos);
	L[2] = invM * (rectLight.Vertex2.xyz - m_Surface.WorldPos);
	L[3] = invM * (rectLight.Vertex3.xyz - m_Surface.WorldPos);

	// use tabulated horizon-clipped sphere
    // check if the shading point is behind the light
    vec3 dir = rectLight.Vertex0.xyz - m_Surface.WorldPos; // LTC space
    vec3 lightNormal = cross(rectLight.Vertex1.xyz - rectLight.Vertex0.xyz, rectLight.Vertex3.xyz - rectLight.Vertex0.xyz);
    bool behind = (dot(dir, lightNormal) < 0.0);

	// cos weighted space
    L[0] = normalize(L[0]);
    L[1] = normalize(L[1]);
    L[2] = normalize(L[2]);
    L[3] = normalize(L[3]);

	// integrate
    vec3 integral = vec3(0.0);
    integral += IntegrateEdgeVec(L[0], L[1]);
    integral += IntegrateEdgeVec(L[1], L[2]);
    integral += IntegrateEdgeVec(L[2], L[3]);
    integral += IntegrateEdgeVec(L[3], L[0]);

	// form factor of the polygon in direction integral
    float len = length(integral);

	float z = integral.z/len;
    if (behind)
        z = -z;

	vec2 uv = vec2(z * 0.5 + 0.5, len); // range [0, 1]
    uv = uv * LtcLutTextureScale + LtcLutTextureBias;

	// Fetch the form factor for horizon clipping
    float scale = texture(u_LTC2Lut, uv).w;

	float sum = len * scale;
	if(!behind && rectLight.Vertex0.w == 0.0)
		sum = 0.0;

	// Outgoing radiance (solid angle) for the entire polygon
    return vec3(sum);
}
vec3 ComputeAreaLightContribution()
{
	vec3 N = m_Surface.Normal;
	vec3 V = m_Surface.ViewVector;
	vec3 P = m_Surface.WorldPos;
	float dotNV = clamp(dot(N, V), 0.0, 1.0);

	vec3 albedo = vec3(m_Surface.Albedo);
	vec3 metalness = vec3(m_Surface.Metalness);

	// Get the UVs for the LUT from the Roughness and sqrt(1-cos_theta)
	vec2 uv = vec2(m_Surface.Roughness, sqrt(1.0 - dotNV));
	uv = uv * LtcLutTextureScale;

	// Get the 4 paramaters for the inverse M
	vec4 t1 = texture(u_LTC1Lut, uv);

	// Get 2 parameters for Fresnel calculation
	vec4 t2 = texture(u_LTC2Lut, uv);

	mat3 invM = mat3(
		vec3(t1.x, 0, t1.y),
		vec3(  0,  1,    0),
		vec3(t1.z, 0, t1.w)
	);

	uint areaLightsCount = uint(u_UniformBuffer.RectangularLightCount);

	vec3 result = vec3(0.0);
	for (uint i = 0; i < areaLightsCount; i++)
	{
		int lightIndex = GetRectangularLightBufferIndex(int(i));
        if (lightIndex == -1)
            break;

		const RectangularLight rectLight = RectangularLightData.u_RectLights[lightIndex];


		// Attenuation
		vec3 centerRectLightPos = ( rectLight.Vertex0.xyz +
									rectLight.Vertex1.xyz +
									rectLight.Vertex2.xyz +
									rectLight.Vertex3.xyz ) / 4;
		float rectLightRadius = rectLight.Vertex1.w;
		float falloff = 3.0f;

		float Ld          = length(centerRectLightPos - m_Surface.WorldPos); //  Light distance
		float attenuation = clamp(1.0 - (Ld * Ld) / (rectLightRadius * rectLightRadius), 0.0, 1.0);
		attenuation      *= mix(attenuation, 1.0, falloff);

		if(attenuation == 0.0)
			continue;


		// Evaluate LTC shading
		vec3 diffuse = EvaluateLTC(mat3(1.0), rectLight);
		vec3 specular = EvaluateLTC(invM, rectLight);

		// GGX BRDF shadowing and Fresnel
		// t2.x: shadowedF90 (F90 normally it should be 1.0)
		// t2.y: Smith function for Geometric Attenuation Term, it is dot(V or L, H).
		specular *= metalness * t2.x + (1.0 - metalness) * t2.y;

		// Add contribution
		result += attenuation * (rectLight.Radiance * rectLight.Intensity * (specular + albedo * diffuse));
	}

	return vec3(result);
}


// A fast approximation of specular AO given the diffuse AO. [Lagarde14]
float ComputeSpecularAO(float nDotV, float ao, float roughness)
{
  return clamp(pow(nDotV + ao, exp2(-16.0 * roughness - 1.0)) - 1.0 + ao, 0.0, 1.0);
}

vec3 ComputeIBLContriubtion()
{
	// Get the values from the surfaces
	vec3 albedo = m_Surface.Albedo.rgb;
	float roughness = clamp(m_Surface.Roughness, 0.0f, 0.95f);
	float metalness = m_Surface.Metalness;

	// Compute some vectors needed for calculations
	vec3 V = m_Surface.ViewVector;
	vec3 N = m_Surface.Normal;

	float nDotV = min(max(dot(N, V), 0.0), 0.99);
	vec3 R = normalize(reflect(V, N));

	// Remap material properties.
	vec3 diffuseAlbedo = (1.0 - metalness) * albedo;
	vec3 F0 = mix(vec3(0.16), albedo, metalness); // instead of "vec3(0.16)" it should be "vec3(0.16 * m_Surface.Albedo.a * m_Surface.Albedo.a)"
	vec3 dielectricF0 = 0.16 * vec3(1.0f);

	// Compute the irradiance factor
	vec3 irradiance = texture(u_IrradianceMap, N).rgb * diffuseAlbedo;
	irradiance *= s_IndirectDiffuse;


	// Compute the radiance factor (depedent on the roughness)
#define MAX_MIP 5.0f
	//int envRadianceTexLevels = textureQueryLevels(u_RadianceFilteredMap);
	float lod = MAX_MIP * roughness;
	vec3 specularDecomp = textureLod(u_RadianceFilteredMap, -R, lod).rgb;

	vec2 splitSums = texture(u_BRDFLut, vec2(nDotV, roughness)).rg;
	//vec3 brdf = mix(vec3(splitSums.x), vec3(splitSums.y), F0);
	vec3 brdf = vec3(F0 * splitSums.x + splitSums.y);

	vec3 specularRadiance = brdf * specularDecomp * (s_IndirectSpecular);
	//vec3 specularRadiance = brdf * (s_IndirectSpecular);
	//specularRadiance = mix(specularRadiance, brdf * s_IndirectSpecular, 0.4f);

	return (irradiance + specularRadiance);
}

// =======================================================

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

// Fast octahedron normal vector decoding.
// https://jcgt.org/published/0003/02/01/
vec2 SignNotZero(vec2 v)
{
	return vec2((v.x >= 0.0) ? 1.0 : -1.0, (v.y >= 0.0) ? 1.0 : -1.0);
}
vec3 DecodeNormal(vec2 e)
{
	vec3 v = vec3(e.xy, 1.0 - abs(e.x) - abs(e.y));
	if (v.z < 0)
		v.xy = (1.0 - abs(v.yx)) * SignNotZero(v.xy);
	return normalize(v);
}
// =======================================================

vec3 ComputeWorldPos(float depth)
{
	ivec2 invoke = ivec2(gl_GlobalInvocationID.xy);
	vec2 coords = (vec2(invoke) + 0.5.xx) / vec2(imageSize(o_Image).xy);

	vec4 clipSpacePosition = vec4(coords * 2.0 - 1.0, depth, 1.0);
	vec4 viewSpacePosition = u_PushConstant.InvViewProjection * clipSpacePosition;
	
    viewSpacePosition /= viewSpacePosition.w; // Perspective division
	return vec3(viewSpacePosition);
}

void main()
{
	ivec2 pixelCoord = ivec2(gl_GlobalInvocationID.xy);

    // Calculating the normals and worldPos
	float depth =           texelFetch(u_DepthBuffer, pixelCoord, 0).r; 
    m_Surface.WorldPos =    ComputeWorldPos(depth);
    m_Surface.ViewVector =  normalize(vec3(u_PushConstant.CameraPosition) - m_Surface.WorldPos);

	// Decode normals
	vec2 encodedNormals =   texelFetch(u_NormalTexture, pixelCoord, 0).rg;
    m_Surface.Normal =      DecodeNormal(encodedNormals);


    // Sampling the textures from gbuffer
    m_Surface.Albedo =      texelFetch(u_AlbedoTexture, pixelCoord, 0).rgb;
    m_Surface.Metalness =   texelFetch(u_NormalTexture, pixelCoord, 0).b;
    m_Surface.Roughness =   texelFetch(u_NormalTexture, pixelCoord, 0).a;
    m_Surface.Emission =    texelFetch(u_AlbedoTexture, pixelCoord, 0).a;

    // Fresnel approximation
	m_Surface.F0 = mix(Fdielectric, m_Surface.Albedo.rgb, m_Surface.Metalness);


	// Sample the Voxel Cone Tracing textures
	s_IndirectDiffuse = texelFetch(u_VoxelIndirectDiffuseTex, pixelCoord, 0).rgb;
	s_IndirectSpecular = texelFetch(u_VoxelIndirectSpecularTex, pixelCoord, 0).rgb;


	// Calculating all point lights contribution
	vec3 Lo = vec3(0.0f);

	float heatMap = 0.0f;
	uint pointLightCount = uint(u_UniformBuffer.PointLightCount);

	for(uint i = 0; i < pointLightCount; i++)
	{
		int lightIndex = GetPointLightBufferIndex(int(i));
        if (lightIndex == -1)
            break;

		PointLight pointLight = PointLightData.u_PointLights[lightIndex];
		Lo += ComputePointLightContribution(pointLight);// * pointLight.Intensity;

		heatMap += 0.1f;
	}

	Lo += ComputeAreaLightContribution();

	Lo += m_Surface.Albedo * m_Surface.Emission;


	// Calculating all directional lights contribution
	DirectionalLight dirLight;
	dirLight.Direction = u_PushConstant.DirectionalLightDir;
	dirLight.Radiance = vec3(1.0f);
	dirLight.Multiplier = u_PushConstant.DirectionalLightIntensity;
	vec3 Ld = ComputeDirectionalLightContribution(dirLight);

	// Adding up the point light and directional light contribution
    vec3 result = Lo + (Ld * texelFetch(u_ShadowTexture, pixelCoord, 0).rgb);

	// Adding up the ibl
	float IBLIntensity = 3.0f;
	result += ComputeIBLContriubtion() * IBLIntensity;

	// Calculating the result with the camera exposure
	result *= u_UniformBuffer.CameraExposure;

	// Outputting the result
    vec4 o_Color = vec4(result, 1.0f);

	if(u_PushConstant.UseLightHeatMap == 1)
		o_Color.rgb += vec3(heatMap);

	imageStore(o_Image, pixelCoord, o_Color);
}