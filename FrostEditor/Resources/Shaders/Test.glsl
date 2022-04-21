#type common
#version 440
/*
 * PBR shader program for a non-occluded point light. Follows the Filament
 * material system (somewhat).
 * https://google.github.io/filament/Filament.md.html#materialsystem/standardmodel
 * This is a stop-gap until tiled deferred rendering is implemented.
 */

#type vertex
void main()
{
  vec2 position = vec2(gl_VertexID % 2, gl_VertexID / 2) * 4.0 - 1;

  gl_Position = vec4(position, 0.0, 1.0);
}

#type fragment
#define PI 3.141592654

// Camera specific uniforms.
layout(std140, binding = 0) uniform CameraBlock
{
  mat4 u_viewMatrix;
  mat4 u_projMatrix;
  mat4 u_invViewProjMatrix;
  vec3 u_camPosition;
  vec4 u_nearFar; // Near plane (x), far plane (y). z and w are unused.
};

// Point light uniforms.
layout(std140, binding = 5) uniform PointBlock
{
  vec4 u_lPositionRadius; // Position (x, y, z), radius (w).
  vec4 u_lColourIntensity; // Colour (x, y, z) and intensity (w).
};

// Uniforms for the geometry buffer.
layout(binding = 2) uniform sampler2D brdfLookUp;
layout(binding = 3) uniform sampler2D gDepth;
layout(binding = 4) uniform sampler2D gNormal;
layout(binding = 5) uniform sampler2D gAlbedo;
layout(binding = 6) uniform sampler2D gMatProp;

// Output colour variable.
layout(location = 0) out vec4 fragColour;

// PBR BRDF.
vec3 filamentBRDF(vec3 l, vec3 v, vec3 n, float roughness, float metallic,
                  vec3 dielectricF0, vec3 metallicF0, vec3 f90, vec3 diffuseAlbedo);

// Compute the light attenuation factor.
float computeAttenuation(vec3 posToLight, float invLightRadius);

// Decodes the worldspace position of the fragment from depth.
vec3 decodePosition(vec2 texCoords, sampler2D depthMap, mat4 invMVP)
{
  float depth = texture(depthMap, texCoords).r;
  vec3 clipCoords = 2.0 * vec3(texCoords, depth) - 1.0.xxx;
  vec4 temp = invMVP * vec4(clipCoords, 1.0);
  return temp.xyz / temp.w;
}

// Fast octahedron normal vector decoding.
// https://jcgt.org/published/0003/02/01/
vec2 signNotZero(vec2 v)
{
  return vec2((v.x >= 0.0) ? 1.0 : -1.0, (v.y >= 0.0) ? 1.0 : -1.0);
}
vec3 decodeNormal(vec2 texCoords, sampler2D encodedNormals)
{
  vec2 e = texture(encodedNormals, texCoords).xy;
  vec3 v = vec3(e.xy, 1.0 - abs(e.x) - abs(e.y));
  if (v.z < 0)
    v.xy = (1.0 - abs(v.yx)) * signNotZero(v.xy);
  return normalize(v);
}

void main()
{
  vec2 fTexCoords = gl_FragCoord.xy / textureSize(gDepth, 0).xy;

  vec3 position = decodePosition(fTexCoords, gDepth, u_invViewProjMatrix);
  vec3 normal = decodeNormal(fTexCoords, gNormal);
  vec4 albedoReflectance = texture(gAlbedo, fTexCoords).rgba;
  vec3 albedo = albedoReflectance.rgb;
  vec2 mr = texture(gMatProp, fTexCoords).rg;
  float metallic = mr.r;
  float roughness = mr.g;

  // Remap material properties.
  vec3 metallicF0 = albedo * metallic;
  vec3 dielectricF0 = 0.16 * albedoReflectance.aaa * albedoReflectance.aaa;

  // Dirty, setting f90 to 1.0.
  vec3 f90 = vec3(1.0);

  // Light properties.
  vec3 view = normalize(u_camPosition - position);
  vec3 posToLight = u_lPositionRadius.xyz - position;
  vec3 light = normalize(posToLight);
  vec3 halfWay = normalize(view + light);
  float nDotL = clamp(dot(normal, light), 0.0, 1.0);

  float lightRadius = u_lPositionRadius.w;
  float attenuation = computeAttenuation(posToLight, 1.0 / lightRadius);
  vec3 lightColour = u_lColourIntensity.xyz;
  float lightIntensity = u_lColourIntensity.w;

  // Compute the radiance contribution.
  vec3 radiance = filamentBRDF(light, view, normal, roughness, metallic,
                               dielectricF0, metallicF0, f90, albedo);
  radiance *= (lightIntensity * lightColour * nDotL * attenuation);

  radiance = max(radiance, vec3(0.0));
  fragColour = vec4(radiance, 1.0);
}

// Compute the light attenuation factor.
float computeAttenuation(vec3 posToLight, float invLightRadius)
{
  float distSquared = dot(posToLight, posToLight);
  float factor = distSquared * invLightRadius * invLightRadius;
  float smoothFactor = max(1.0 - factor * factor, 0.0);
  return (smoothFactor * smoothFactor) / max(distSquared, 1e-4);
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
vec3 fsCookTorrance(float nDotH, float lDotH, float nDotV, float nDotL,
                    float vDotH, float actualRoughness, vec3 f0, vec3 f90)
{
  float D = nGGX(nDotH, actualRoughness);
  vec3 F = sFresnel(vDotH, f0, f90);
  float V = vGGXFast(nDotV, nDotL, actualRoughness);
  return D * F * V;
}

// Burley diffuse for the diffuse component of the BRDF.
vec3 fdBurley(float nDotV, float nDotL, float lDotH, float actualRoughness, vec3 diffuseAlbedo)
{
  vec3 f90 = vec3(0.5 + 2.0 * actualRoughness * lDotH * lDotH);
  vec3 lightScat = sFresnel(nDotL, vec3(1.0), f90);
  vec3 viewScat = sFresnel(nDotV, vec3(1.0), f90);
  return lightScat * viewScat * (1.0 / PI) * diffuseAlbedo;
}

// Lambertian diffuse for the diffuse component of the BRDF.
vec3 fdLambert(vec3 diffuseAlbedo)
{
  return diffuseAlbedo / PI;
}

// Lambertian diffuse for the diffuse component of the BRDF. Corrected to guarantee
// energy is conserved.
vec3 fdLambertCorrected(vec3 f0, vec3 f90, float vDotH, float lDotH,
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

// The final combined BRDF. Compensates for energy gain in the diffuse BRDF and
// energy loss in the specular BRDF.
vec3 filamentBRDF(vec3 l, vec3 v, vec3 n, float roughness, float metallic,
                  vec3 dielectricF0, vec3 metallicF0, vec3 f90,
                  vec3 diffuseAlbedo)
{
  vec3 h = normalize(v + l);

  float nDotV = max(abs(dot(n, v)), 1e-5);
  float nDotL = clamp(dot(n, l), 1e-5, 1.0);
  float nDotH = clamp(dot(n, h), 1e-5, 1.0);
  float lDotH = clamp(dot(l, h), 1e-5, 1.0);
  float vDotH = clamp(dot(v, h), 1e-5, 1.0);

  float clampedRoughness = max(roughness, 0.045);
  float actualRoughness = clampedRoughness * clampedRoughness;

  vec2 dfg = texture(brdfLookUp, vec2(nDotV, roughness)).rg;
  dfg = max(dfg, 1e-4.xx);
  vec3 energyDielectric = 1.0.xxx + dielectricF0 * (1.0.xxx / dfg.y - 1.0.xxx);
  vec3 energyMetallic = 1.0.xxx + metallicF0 * (1.0.xxx / dfg.y - 1.0.xxx);

  vec3 fs = fsCookTorrance(nDotH, lDotH, nDotV, nDotL, vDotH, actualRoughness, dielectricF0, f90);
  //fs *= energyDielectric;
  vec3 fd = fdLambertCorrected(dielectricF0, f90, vDotH, lDotH, diffuseAlbedo);
  vec3 dielectricBRDF = fs + fd;

  vec3 metallicBRDF = fsCookTorrance(nDotH, lDotH, nDotV, nDotL, vDotH, actualRoughness, metallicF0, f90);
  //metallicBRDF *= energyMetallic;

  return mix(dielectricBRDF, metallicBRDF, metallic);
}
//=======================================================================================
//=======================================================================================
//=======================================================================================
//=======================================================================================
//=======================================================================================
//=======================================================================================
//=======================================================================================
//=======================================================================================
//=======================================================================================
//=======================================================================================
#type common
#version 440
/*
 * PBR shader program for a non-occluded directional light. Follows the Filament
 * material system (somewhat).
 * https://google.github.io/filament/Filament.md.html#materialsystem/standardmodel
 */

#type vertex
void main()
{
  vec2 position = vec2(gl_VertexID % 2, gl_VertexID / 2) * 4.0 - 1;

  gl_Position = vec4(position, 0.0, 1.0);
}

#type fragment
#define PI 3.141592654

// Camera specific uniforms.
layout(std140, binding = 0) uniform CameraBlock
{
  mat4 u_viewMatrix;
  mat4 u_projMatrix;
  mat4 u_invViewProjMatrix;
  vec3 u_camPosition;
  vec4 u_nearFar; // Near plane (x), far plane (y). z and w are unused.
};

// Directional light uniforms.
layout(std140, binding = 5) uniform DirectionalBlock
{
  vec4 u_lColourIntensity;
  vec4 u_lDirection;
  ivec4 u_directionalSettings;
};

// Uniforms for the geometry buffer.
layout(binding = 2) uniform sampler2D brdfLookUp;
layout(binding = 3) uniform sampler2D gDepth;
layout(binding = 4) uniform sampler2D gNormal;
layout(binding = 5) uniform sampler2D gAlbedo;
layout(binding = 6) uniform sampler2D gMatProp;

// Output colour variable.
layout(location = 0) out vec4 fragColour;

// PBR BRDF.
vec3 filamentBRDF(vec3 l, vec3 v, vec3 n, float roughness, float metallic,
                  vec3 dielectricF0, vec3 metallicF0, vec3 f90, vec3 diffuseAlbedo);

// Decodes the worldspace position of the fragment from depth.
vec3 decodePosition(vec2 texCoords, sampler2D depthMap, mat4 invMVP)
{
  float depth = texture(depthMap, texCoords).r;
  vec3 clipCoords = 2.0 * vec3(texCoords, depth) - 1.0.xxx;
  vec4 temp = invMVP * vec4(clipCoords, 1.0);
  return temp.xyz / temp.w;
}

// Fast octahedron normal vector decoding.
// https://jcgt.org/published/0003/02/01/
vec2 signNotZero(vec2 v)
{
  return vec2((v.x >= 0.0) ? 1.0 : -1.0, (v.y >= 0.0) ? 1.0 : -1.0);
}
vec3 decodeNormal(vec2 texCoords, sampler2D encodedNormals)
{
  vec2 e = texture(encodedNormals, texCoords).xy;
  vec3 v = vec3(e.xy, 1.0 - abs(e.x) - abs(e.y));
  if (v.z < 0)
    v.xy = (1.0 - abs(v.yx)) * signNotZero(v.xy);
  return normalize(v);
}

void main()
{
  vec2 fTexCoords = gl_FragCoord.xy / textureSize(gDepth, 0).xy;

  vec3 position = decodePosition(fTexCoords, gDepth, u_invViewProjMatrix);
  vec3 normal = decodeNormal(fTexCoords, gNormal);
  vec4 albedoReflectance = texture(gAlbedo, fTexCoords).rgba;
  vec3 albedo = albedoReflectance.rgb;
  vec2 mr = texture(gMatProp, fTexCoords).rg;
  float metallic = mr.r;
  float roughness = mr.g;

  // Remap material properties.
  vec3 metallicF0 = albedo * metallic;
  vec3 dielectricF0 = 0.16 * albedoReflectance.aaa * albedoReflectance.aaa;

  // Dirty, setting f90 to 1.0.
  vec3 f90 = vec3(1.0);

  // Light properties.
  vec3 view = normalize(u_camPosition - position);
  vec3 light = normalize(u_lDirection.xyz);
  vec3 lightColour = u_lColourIntensity.xyz;
  float lightIntensity = u_lColourIntensity.w;
  float nDotL = clamp(dot(normal, light), 0.0, 1.0);

  // Compute the radiance contribution.
  vec3 radiance = filamentBRDF(light, view, normal, roughness, metallic,
                               dielectricF0, metallicF0, f90, albedo);
  radiance *= (lightIntensity * nDotL * lightColour);

  radiance = max(radiance, vec3(0.0));
  fragColour = vec4(radiance, 1.0);
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
vec3 fsCookTorrance(float nDotH, float lDotH, float nDotV, float nDotL,
                    float vDotH, float actualRoughness, vec3 f0, vec3 f90)
{
  float D = nGGX(nDotH, actualRoughness);
  vec3 F = sFresnel(vDotH, f0, f90);
  float V = vGGXFast(nDotV, nDotL, actualRoughness);
  return D * F * V;
}

// Burley diffuse for the diffuse component of the BRDF.
vec3 fdBurley(float nDotV, float nDotL, float lDotH, float actualRoughness, vec3 diffuseAlbedo)
{
  vec3 f90 = vec3(0.5 + 2.0 * actualRoughness * lDotH * lDotH);
  vec3 lightScat = sFresnel(nDotL, vec3(1.0), f90);
  vec3 viewScat = sFresnel(nDotV, vec3(1.0), f90);
  return lightScat * viewScat * (1.0 / PI) * diffuseAlbedo;
}

// Lambertian diffuse for the diffuse component of the BRDF.
vec3 fdLambert(vec3 diffuseAlbedo)
{
  return diffuseAlbedo / PI;
}

// Lambertian diffuse for the diffuse component of the BRDF. Corrected to guarantee
// energy is conserved.
vec3 fdLambertCorrected(vec3 f0, vec3 f90, float vDotH, float lDotH,
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

// The final combined BRDF. Compensates for energy gain in the diffuse BRDF and
// energy loss in the specular BRDF.
vec3 filamentBRDF(vec3 l, vec3 v, vec3 n, float roughness, float metallic,
                  vec3 dielectricF0, vec3 metallicF0, vec3 f90,
                  vec3 diffuseAlbedo)
{
  vec3 h = normalize(v + l);

  float nDotV = max(abs(dot(n, v)), 1e-5);
  float nDotL = clamp(dot(n, l), 1e-5, 1.0);
  float nDotH = clamp(dot(n, h), 1e-5, 1.0);
  float lDotH = clamp(dot(l, h), 1e-5, 1.0);
  float vDotH = clamp(dot(v, h), 1e-5, 1.0);

  float clampedRoughness = max(roughness, 0.045);
  float actualRoughness = clampedRoughness * clampedRoughness;

  vec2 dfg = texture(brdfLookUp, vec2(nDotV, roughness)).rg;
  dfg = max(dfg, 1e-4.xx);
  vec3 energyDielectric = 1.0.xxx + dielectricF0 * (1.0.xxx / dfg.y - 1.0.xxx);
  vec3 energyMetallic = 1.0.xxx + metallicF0 * (1.0.xxx / dfg.y - 1.0.xxx);

  vec3 fs = fsCookTorrance(nDotH, lDotH, nDotV, nDotL, vDotH, actualRoughness, dielectricF0, f90);
  //fs *= energyDielectric;
  vec3 fd = fdLambertCorrected(dielectricF0, f90, vDotH, lDotH, diffuseAlbedo);
  vec3 dielectricBRDF = fs + fd;

  vec3 metallicBRDF = fsCookTorrance(nDotH, lDotH, nDotV, nDotL, vDotH, actualRoughness, metallicF0, f90);
  //metallicBRDF *= energyMetallic;

  return mix(dielectricBRDF, metallicBRDF, metallic);
}

//=======================================================================================
//=======================================================================================
//=======================================================================================
//=======================================================================================
//=======================================================================================
//=======================================================================================
//=======================================================================================
//=======================================================================================
//=======================================================================================
//=======================================================================================

#type common
#version 440
/*
 * PBR shader program for image-based lighting. Follows the Filament
 * material system (somewhat).
 * https://google.github.io/filament/Filament.md.html#materialsystem/standardmodel
 */

#type vertex
void main()
{
  vec2 position = vec2(gl_VertexID % 2, gl_VertexID / 2) * 4.0 - 1;

  gl_Position = vec4(position, 0.0, 1.0);
}

#type fragment
#define PI 3.141592654
#define MAX_MIP 4.0

// Camera specific uniforms.
layout(std140, binding = 0) uniform CameraBlock
{
  mat4 u_viewMatrix;
  mat4 u_projMatrix;
  mat4 u_invViewProjMatrix;
  vec3 u_camPosition;
  vec4 u_nearFar; // Near plane (x), far plane (y). z and w are unused.
};

// Ambient lighting specific uniforms.
layout(std140, binding = 4) uniform AmbientBlock
{
  vec3 u_intensity; // Use HBAO (x), intensity (z). y and w unused.
};

// Samplers from the gbuffer and required lookup textures for IBL.
layout(binding = 0) uniform samplerCube irradianceMap;
layout(binding = 1) uniform samplerCube reflectanceMap;
layout(binding = 2) uniform sampler2D brdfLookUp;
layout(binding = 3) uniform sampler2D gDepth;
layout(binding = 4) uniform sampler2D gNormal;
layout(binding = 5) uniform sampler2D gAlbedo;
layout(binding = 6) uniform sampler2D gMatProp;
layout(binding = 7) uniform sampler2D hbaoTexture;

// Output colour variable.
layout(location = 0) out vec4 fragColour;

// Compute the Filament IBL contribution.
vec3 evaluateFilamentIBL(vec3 n, vec3 v, vec3 diffuseColour, vec3 f0, vec3 f90,
                         float roughness, float diffAO);

// Decodes the worldspace position of the fragment from depth.
vec3 decodePosition(vec2 texCoords, sampler2D depthMap, mat4 invMVP)
{
  float depth = textureLod(depthMap, texCoords, 0.0).r;
  vec3 clipCoords = 2.0 * vec3(texCoords, depth) - 1.0.xxx;
  vec4 temp = invMVP * vec4(clipCoords, 1.0);
  return temp.xyz / temp.w;
}

// Fast octahedron normal vector decoding.
// https://jcgt.org/published/0003/02/01/
vec2 signNotZero(vec2 v)
{
  return vec2((v.x >= 0.0) ? 1.0 : -1.0, (v.y >= 0.0) ? 1.0 : -1.0);
}
vec3 decodeNormal(vec2 texCoords, sampler2D encodedNormals)
{
  vec2 e = texture(encodedNormals, texCoords).xy;
  vec3 v = vec3(e.xy, 1.0 - abs(e.x) - abs(e.y));
  if (v.z < 0)
    v.xy = (1.0 - abs(v.yx)) * signNotZero(v.xy);
  return normalize(v);
}

void main()
{
  vec2 fTexCoords = gl_FragCoord.xy / textureSize(gDepth, 0).xy;

  vec3 position = decodePosition(fTexCoords, gDepth, u_invViewProjMatrix);
  vec3 normal = decodeNormal(fTexCoords, gNormal);
  vec3 albedo = texture(gAlbedo, fTexCoords).rgb;
  float metallic = texture(gMatProp, fTexCoords).r;
  float roughness = texture(gMatProp, fTexCoords).g;
  float ao = texture(gMatProp, fTexCoords).b;
  float emiss = texture(gMatProp, fTexCoords).a;
  float reflectance = texture(gAlbedo, fTexCoords).a;

  // Look up the HBAO value.
  if (u_intensity.x > 0)
  {
    float hbao = texture(hbaoTexture, fTexCoords).r;
    ao = min(ao, hbao);
  }

  // Remap material properties.
  vec3 diffuseAlbedo = (1.0 - metallic) * albedo;
  vec3 mappedF0 = mix((0.16 * reflectance * reflectance).xxx, albedo, metallic);

  // Dirty, setting f90 to 1.0.
  vec3 f90 = vec3(1.0);

  // Compute the FilamentIBL contribution.
  vec3 view = normalize(position - u_camPosition);
	vec3 radiance = evaluateFilamentIBL(normal, view, diffuseAlbedo, mappedF0, f90,
                                      roughness, ao) * u_intensity.z;
  // Add in emission. TODO: Emission maps.
  radiance += emiss * albedo;

  radiance = max(radiance, 0.0.xxx);
  fragColour = vec4(radiance, 1.0);
}

// A fast approximation of specular AO given the diffuse AO. [Lagarde14]
float computeSpecularAO(float nDotV, float ao, float roughness)
{
  return clamp(pow(nDotV + ao, exp2(-16.0 * roughness - 1.0)) - 1.0 + ao, 0.0, 1.0);
}

// Compute the Filament IBL contribution.
vec3 evaluateFilamentIBL(vec3 n, vec3 v, vec3 diffuseColour, vec3 f0, vec3 f90,
                         float roughness, float diffAO)
{
  float nDotV = max(dot(n, -v), 0.0);
  vec3 r = normalize(reflect(v, n));

  vec3 irradiance = texture(irradianceMap, n).rgb * diffuseColour * diffAO;

  float lod = MAX_MIP * roughness;
  vec3 specularDecomp = textureLod(reflectanceMap, r, lod).rgb;
  vec2 splitSums = texture(brdfLookUp, vec2(nDotV, roughness)).rg;

  vec3 brdf = mix(splitSums.xxx, splitSums.yyy, f0);

  vec3 specularRadiance = brdf * specularDecomp
                        * computeSpecularAO(nDotV, diffAO, roughness);

  return irradiance + specularRadiance;
}




vec3 IBL_Contribution()
{
	float NdotV = dot(m_Surface.Normal, m_Surface.ViewVector);
	float roughness = clamp(m_Surface.Roughness, 0.0f, 0.95f);

	// Specular reflection vector
	vec3 Lr = 2.0 * NdotV * m_Surface.Normal - m_Surface.ViewVector;
	

	vec3 irradiance = texture(u_IrradianceMap, m_Surface.Normal).rgb;
	vec3 F = FresnelSchlickRoughness(m_Surface.F0, NdotV, roughness);
	vec3 kd = (1.0 - F) * (1.0 - m_Surface.Metalness);
	vec3 diffuseIBL = m_Surface.Albedo.rgb * irradiance;

	//int envRadianceTexLevels = textureQueryLevels(u_RadianceFilteredMap);
	int envRadianceTexLevels = 4;
	float NoV = clamp(NdotV, 0.0, 1.0);
	vec3 R = 2.0 * dot(m_Surface.ViewVector, m_Surface.Normal) * m_Surface.Normal - m_Surface.ViewVector;
	vec3 specularIrradiance = textureLod(u_RadianceFilteredMap, RotateVectorByY(0.0f, Lr), (roughness) * envRadianceTexLevels).rgb;

	// Sample BRDF Lut, 1.0 - roughness for y-coord because texture was generated (in Sparky) for gloss model
	vec2 specularBRDF = texture(u_BRDFLut, vec2(NdotV, 1.0 - roughness)).rg;
	vec3 specularIBL = specularIrradiance * (m_Surface.F0 * specularBRDF.x + specularBRDF.y);

	return kd * diffuseIBL + specularIBL;
}