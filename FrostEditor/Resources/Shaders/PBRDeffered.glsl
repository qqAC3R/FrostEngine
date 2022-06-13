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

// Voxel texture
layout(binding = 11) uniform sampler3D u_VoxelTexture;

layout(push_constant) uniform PushConstant {
    vec4 CameraPosition; // vec4 - camera position + float pointLightCount + lightCullingWorkgroup.x
	vec3 DirectionalLightDir;
	float UseLightHeatMap;
	vec4 VoxelSampleOffset;
} u_PushConstant;


struct SurfaceProperties
{
    vec3 WorldPos;
    vec3 Normal;
    vec3 ViewVector;
    
    vec4 Albedo;
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

vec4 SampleVoxels(vec3 worldPosition, float lod) {
    //worldPosition.x -= u_PushConstant.VoxelSampleOffset.x;
    //worldPosition.y -= u_PushConstant.VoxelSampleOffset.y;
    //worldPosition.z -= u_PushConstant.VoxelSampleOffset.z;

	worldPosition.x -= (u_PushConstant.CameraPosition.x - u_PushConstant.VoxelSampleOffset.x);
	worldPosition.y -= (u_PushConstant.CameraPosition.y - u_PushConstant.VoxelSampleOffset.y);
	worldPosition.z -= (u_PushConstant.CameraPosition.z - u_PushConstant.VoxelSampleOffset.z);


	float VoxelDimensions = u_PushConstant.VoxelSampleOffset.w;
	float VoxelGridWorldSize = u_PushConstant.VoxelSampleOffset.w;

    vec3 offset = vec3(1.0 / VoxelDimensions, 1.0 / VoxelDimensions, 0); // Why??
    vec3 voxelTextureUV = worldPosition / (VoxelGridWorldSize * 0.5);
    voxelTextureUV = voxelTextureUV * 0.5 + 0.5 + offset;

    if(voxelTextureUV.x > 1.0f || voxelTextureUV.x < 0.0f) return vec4(0.0f);
    if(voxelTextureUV.y > 1.0f || voxelTextureUV.y < 0.0f) return vec4(0.0f);
    if(voxelTextureUV.z > 1.0f || voxelTextureUV.z < 0.0f) return vec4(0.0f);
    //return vec4(voxelTextureUV, 1.0f);

    lod = clamp(lod, 0.0f, 5.0f);

	voxelTextureUV.y *= 1.0f;

    return textureLod(u_VoxelTexture, voxelTextureUV, lod);
}





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

	float nDotV = max(dot(N, V), 0.0);
	vec3 R = normalize(reflect(V, N));

	// Remap material properties.
	vec3 diffuseAlbedo = (1.0 - metalness) * albedo;
	vec3 F0 = mix(vec3(0.16), albedo, metalness); // instead of "vec3(0.16)" it should be "vec3(0.16 * m_Surface.Albedo.a * m_Surface.Albedo.a)"
	vec3 dielectricF0 = 0.16 * vec3(1.0f);

	// Compute the irradiance factor
	vec3 irradiance = texture(u_IrradianceMap, N).rgb * diffuseAlbedo;


	// Compute the radiance factor (depedent on the roughness)
#define MAX_MIP 5.0f
	//int envRadianceTexLevels = textureQueryLevels(u_RadianceFilteredMap);
	float lod = MAX_MIP * roughness;
	vec3 specularDecomp = textureLod(u_RadianceFilteredMap, -R, lod).rgb;

	vec2 splitSums = texture(u_BRDFLut, vec2(nDotV, roughness)).rg;
	//vec3 brdf = mix(vec3(splitSums.x), vec3(splitSums.y), F0);
	vec3 brdf = vec3(F0 * splitSums.x + splitSums.y);

	vec3 specularRadiance = brdf * specularDecomp;

	return (irradiance + specularRadiance);
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
    m_Surface.Albedo =      texture(u_AlbedoTexture, v_TexCoord).rgba;
    m_Surface.Metalness =   texture(u_NormalTexture, v_TexCoord).b;
    m_Surface.Roughness =   texture(u_NormalTexture, v_TexCoord).a;
    m_Surface.Emission =    texture(u_AlbedoTexture, v_TexCoord).a;

    // Fresnel approximation
	m_Surface.F0 = mix(Fdielectric, m_Surface.Albedo.rgb, m_Surface.Metalness);


	
	// Calculating all point lights contribution
	vec3 Lo = vec3(0.0f);

	float heatMap = 0.0f;
	uint pointLightCount = uint(m_CameraData.PointLightCount);

	for(uint i = 0; i < pointLightCount; i++)
	{
		int lightIndex = GetLightBufferIndex(int(i));
        if (lightIndex == -1)
            break;

		PointLight pointLight = LightData.u_PointLights[lightIndex];
		Lo += ComputePointLightContribution(pointLight);// * pointLight.Intensity;

		heatMap += 0.1f;
	}


	// Calculating all directional lights contribution
	DirectionalLight dirLight;
	dirLight.Direction = u_PushConstant.DirectionalLightDir;
	dirLight.Radiance = vec3(1.0f);
	dirLight.Multiplier = 0.05f;
	vec3 Ld = ComputeDirectionalLightContribution(dirLight);

	// Adding up the point light and directional light contribution
    vec3 result = Lo + Ld;

	// Adding up the ibl
	float IBLIntensity = 2.0f;
	result += ComputeIBLContriubtion() * IBLIntensity;

	// Calculating the result with the camera exposure
	result *= m_CameraData.Exposure;

	result = SampleVoxels(m_Surface.WorldPos, 0.0f).xyz;

	// Outputting the result
    o_Color = vec4(result, 1.0f);

	if(u_PushConstant.UseLightHeatMap == 1.0f)
		o_Color.rgb += + vec3(heatMap);
}