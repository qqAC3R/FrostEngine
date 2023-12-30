/*
	Sources:
		http://roar11.com/2015/07/screen-space-glossy-reflections/
		https://github.com/LukasBanana/ForkENGINE
		https://lukas-hermanns.info/download/bachelorthesis_ssct_lhermanns.pdf


	Visibility buffer for better cone weight distribuiton. Look into:
		https://github.com/LukasBanana/ForkENGINE/blob/master/shaders/SSCTVisibilityMapPixelShader.glsl
		http://what-when-how.com/Tutorial/topic-547pjramj8/GPU-Pro-Advanced-Rendering-Techniques-192.html
		https://lukas-hermanns.info/download/bachelorthesis_ssct_lhermanns.pdf (p.32)

*/

#type compute
#version 460

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

// Unroll all loops for performance - this is important
#pragma optionNV(unroll all)

layout(binding = 0) uniform sampler2D u_ColorFrameTex;

layout(binding = 1) uniform sampler2D u_ViewPosTex;
layout(binding = 2) uniform sampler2D u_HiZBuffer;
layout(binding = 3) uniform sampler2D u_NormalTex; // includes compressed vec2 normals + metalness + roughness
layout(binding = 4) uniform sampler2D u_PrefilteredColorBuffer;
layout(binding = 8) uniform sampler2D u_SpatialBlueNoiseLUT;
//layout(binding = 5) uniform sampler2D u_VisibilityBuffer;

layout(binding = 6, rgba8) uniform writeonly image2D o_FrameTex;

layout(binding = 7) uniform UniformBuffer {
	mat4 ProjectionMatrix;
	mat4 InvProjectionMatrix;
	mat4 ViewMatrix;
	mat4 InvViewMatrix;
	vec4 ScreenSize;

	int UseConeTracing;
	int RayStepCount;
	float RayStepSize;
	int UseHizTracing;
} u_UniformBuffer;

// Global variables
vec2 s_UV;

// Constants
const float relfectionSpecularFalloffExponent = 6.0f;
const float M_PI = 3.14159265359f;


// Defines
#define HIZ_CROSS_EPSILON		(vec2(0.5f) / vec2(textureSize(u_HiZBuffer, 0).xy))

#define INVALID_HIT_POINT		vec3(-1.0)
#define HIZ_VIEWDIR_EPSILON		0.00001

#define SIGN(x)                 ((x) >= 0.0 ? 1.0 : -1.0)
#define saturate(x)             clamp(x, 0.0, 1.0)

#define BS_DELTA_EPSILON 0.0002

// ------------------------ LUTS --------------------------
vec4 SampleBlueNoise(ivec2 coords)
{
	return texture(u_SpatialBlueNoiseLUT, (vec2(coords) + 0.5.xx) / vec2(textureSize(u_SpatialBlueNoiseLUT, 0).xy));
}

// ======================================================
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

// Random hash function
const vec3 r_Scale = vec3(0.8f);
const float r_K = 19.19f;
vec3 HashFunc(vec3 a)
{
	a = fract(a * r_Scale);
	a += dot(a, a.xyz + r_K);
	return fract((a.xxy + a.yxx) * a.zyx);
}

uvec3 MurmurHash33(uvec3 src) {
    const uint M = 0x5bd1e995u;
    uvec3 h = uvec3(1190494759u, 2147483647u, 3559788179u);
    src *= M; src ^= src>>24u; src *= M;
    h *= M; h ^= src.x; h *= M; h ^= src.y; h *= M; h ^= src.z;
    h ^= h>>13u; h *= M; h ^= h>>15u;
    return h;
}

// 3 outputs, 3 inputs
vec3 Hash33(vec3 src) {
    uvec3 h = MurmurHash33(floatBitsToUint(src));
    return uintBitsToFloat(h & 0x007fffffu | 0x3f800000u) - 1.0;
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
// ======================================================
vec3 ProjectPoint(vec3 viewPoint)
{
	vec4 projPoint = u_UniformBuffer.ProjectionMatrix * vec4(viewPoint, 1.0f);
	projPoint.xyz /= projPoint.w;
	projPoint.xy = projPoint.xy * 0.5f + 0.5f;
	return projPoint.xyz;
}



const float FLOAT_MAX = 3.402823466e+38f;
const int BASE_HIZ_MIP_LEVEL = 0;



void InitialAdvanceRay(vec3 origin, vec3 direction, vec3 invDirection, vec2 currentMipResolution, vec2 currentMipResolutionInv, vec2 floorOffset, vec2 uvOffset, out vec3 position, out float currentT)
{
	vec2 currentMipPosition = currentMipResolution * origin.xy;

	// Intersect ray with the half box that is pointing away from the ray origin.
	vec2 xyPlane = floor(currentMipPosition) + floorOffset;
	xyPlane = xyPlane * currentMipResolutionInv + uvOffset;

	// o + d * t = p' => t = (p' - o) / d
	vec2 t = (xyPlane - origin.xy) * invDirection.xy;
	currentT = min(t.x, t.y);
	position = origin + currentT * direction;
}

void SecondAdvanceRay(vec3 origin, vec3 direction, vec3 invDirection, vec2 currentMipPosition, vec2 currentMipResolutionInv, vec2 floorOffset, vec2 uvOffset, inout vec3 position, inout float currentT)
{
	// Create boundary planes
	vec2 xyPlane = floor(currentMipPosition) + floorOffset;
	xyPlane = xyPlane * currentMipResolutionInv + uvOffset;

	// o + d * t = p' => t = (p' - o) / d
	vec2 t = (xyPlane - origin.xy) * invDirection.xy;
	currentT = min(t.x, t.y);
	position = origin + currentT * direction;

}




ivec2 GetDepthMipResolution(int mipLevel)
{
	return ivec2(textureSize(u_HiZBuffer, mipLevel));
}

float FetchDepth(ivec2 coords, int lod)
{
	return texelFetch(u_HiZBuffer, coords, lod).r;
}

//#define INVERTED_DEPTH_RANGE

bool AdvanceRay(vec3 origin, vec3 direction, vec3 invDirection, vec2 currentMipPosition, vec2 currentMipResolutionInv, vec2 floorOffset, vec2 uvOffset, float surfaceZ, inout vec3 position, inout float currentT)
{
	// Create boundary planes
	vec2 xyPlane = floor(currentMipPosition) + floorOffset;
	xyPlane = xyPlane * currentMipResolutionInv + uvOffset;
	vec3 boundaryPlanes = vec3(xyPlane, surfaceZ);

	// Intersect ray with the half box that is pointing away from the ray origin.
	// o + d * t = p' => t = (p' - o) / d
	vec3 t = (boundaryPlanes - origin) * invDirection;

	// Prevent using z plane when shooting out of the depth buffer.
	#ifdef INVERTED_DEPTH_RANGE
	t.z = direction.z < 0 ? t.z : FLOAT_MAX;
	#else
	t.z = direction.z > 0 ? t.z : FLOAT_MAX;
	#endif

		// Choose nearest intersection with a boundary.
	float tMin = min(min(t.x, t.y), t.z);

	#ifdef INVERTED_DEPTH_RANGE
//		// Larger z means closer to the camera.
	bool aboveSurface = surfaceZ < position.z;
	#else
		// Smaller z means closer to the camera.
	bool aboveSurface = surfaceZ > position.z;
	#endif

		// Decide whether we are able to advance the ray until we hit the xy boundaries or if we had to clamp it at the surface.
	bool skippedTile = tMin != t.z && aboveSurface;

	// Make sure to only advance the ray if we're still above the surface.
	currentT = aboveSurface ? tMin : currentT;

	// Advance ray
	position = origin + currentT * direction;

	return skippedTile;
}




// Requires origin and direction of the ray to be in screen space [0, 1] x [0, 1]
vec3 HierarchicalRaymarch(
		vec3 origin,
		vec3 direction,
		vec2 screenSize, // Hi-Z Screen Size
		int mostDetailedMip,
		uint minTraversalOccupancy,
		uint maxTraversalIntersections,
		out uint iterations,
		out bool validHit)
{
	vec3 invDirection = direction != vec3(0.0) ? 1.0 / direction : vec3(FLOAT_MAX);

	// Start on mip with highest detail.
	int currentMip = mostDetailedMip;

	vec2 currentMipResolution = vec2(GetDepthMipResolution(currentMip));
	vec2 currentMipResolutionInv = 1.0 / currentMipResolution;

	// Offset to the bounding boxes uv space to intersect the ray with the center of the next pixel.
	// This means we ever so slightly over shoot into the next region. 
	vec2 uvOffset = 0.005 * exp2(float(mostDetailedMip)) / screenSize;
	uvOffset.x = (direction.x < 0.0 ? -uvOffset.x : uvOffset.x);
	uvOffset.y = (direction.y < 0.0 ? -uvOffset.y : uvOffset.y);

	// Offset applied depending on current mip resolution to move the boundary to the left/right upper/lower border depending on ray direction.
	vec2 floorOffset;
	floorOffset.x = (direction.x < 0.0 ? 0.0 : 1.0);
	floorOffset.y = (direction.y < 0.0 ? 0.0 : 1.0);


	// Initially advance ray to avoid immediate self intersections.
	float currentT;
	vec3 position;
	InitialAdvanceRay(
		origin,
		direction,
		invDirection,
		currentMipResolution,
		currentMipResolutionInv,
		floorOffset,
		uvOffset,
		position,
		currentT
	);
	vec2 currentMipPosition = currentMipResolution * position.xy;


	// This is a way to prevent artifacts on bumpy surfaces (rough normals).
	SecondAdvanceRay(
		origin,
		direction,
		invDirection,
		currentMipPosition,
		currentMipResolutionInv,
		floorOffset,
		uvOffset,
		position,
		currentT
	);

	iterations = 0;
	while (iterations < maxTraversalIntersections && currentMip >= mostDetailedMip)
	{
		//break;

		currentMipPosition = currentMipResolution * position.xy;
		float surfaceZ = FetchDepth(ivec2(currentMipPosition), currentMip);

		bool skippedTile = AdvanceRay(
				origin,
				direction,
				invDirection,
				currentMipPosition,
				currentMipResolutionInv,
				floorOffset,
				uvOffset,
				surfaceZ,
				position,
				currentT
		);

		currentMip += skippedTile ? 1 : -1;
		

		currentMipResolution *= skippedTile ? 0.5 : 2.0;
		currentMipResolutionInv *= skippedTile ? 2.0 : 0.5;

		++iterations;

	}

	validHit = (iterations < maxTraversalIntersections);

	return position;
}




// -----------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------
float IsoscelesTriangleBase(float adjacentLength, float coneTheta)
{
	/*
		Simple trig and algebra - soh, cah, toa - tan(theta) = opp/adj,
		opp = tan(theta) * adj, then multiply by 2 for isosceles triangle base
	*/
	return 2.0 * tan(coneTheta) * adjacentLength;
}

float IsoscelesTriangleInRadius(float a, float h)
{
	float h4 = h * 4.0f;
	return (a * (sqrt(a * a + h4 * h) - a)) / max(h4, 0.00001);
}

float IsoscelesTriangleNextAdjacent(float adjacentLength, float incircleRadius)
{
	/*
	Subtract the diameter of the incircle
	to get the adjacent side of the next level on the cone
	*/
	return adjacentLength - (incircleRadius * 2.0);
}


//http://simonstechblog.blogspot.com/2011/12/microfacet-brdf.html
// temporay use Beckmann Distribution to convert, this may change later 
float RoughnessToSpecularPower(float roughness)
{
#define MAX_SPEC_POWER 2048.0
    return clamp(2.0 / (roughness * roughness) - 2.0, 0.0, MAX_SPEC_POWER);
}

float SpecularPowerToConeAngle(float specularPower)
{
#define CNST_MAX_SPECULAR_EXP 1024.0

	if (specularPower < CNST_MAX_SPECULAR_EXP)
	{
		/* Based on phong reflection model */
		const float xi = 0.244;
		float exponent = 1.0 / (specularPower + 1.0);
		return acos(pow(xi, exponent));
	}
	return 0.0;
}

/*  // Old Concept
vec4 ConeSampleWeightedColor(vec2 samplePos, float mipLevel)
{
	// Sample color buffer with pre-integrated visibility
	vec3 color = textureLod(u_PrefilteredColorBuffer, samplePos, mipLevel).rgb;
	float visibility = textureLod(u_VisibilityBuffer, samplePos, mipLevel).r;
	return vec4(color * visibility, visibility);
}
*/

vec4 ConeSampleWeightRoughness(vec2 samplePos, float mipLevel, float roughness)
{
	/* Sample color buffer with pre-integrated visibility */
	vec3 color = textureLod(u_PrefilteredColorBuffer, samplePos, mipLevel).rgb;
	return vec4(color * roughness, roughness);
}

// https://github.com/LukasBanana/ForkENGINE/blob/master/shaders/SSCTReflectionPixelShader.glsl#L600
vec4 ConeTrace(vec2 startPos, vec2 endPos, float roughness)
{
	roughness = max(min(roughness, 0.95), 0.01);

	float lod = 0.0f;
	vec2 screenSize = vec2(imageSize(o_FrameTex).xy);

	float specularPower = RoughnessToSpecularPower(roughness);

	/* Information */
	vec4 reflectionColor = vec4(0.0f);
	//float coneTheta = SpecularPowerToConeAngle(specularPower) * 0.5f;
	float coneTheta = roughness * M_PI * 0.025;


	
	/* Cone tracing using an isosceles triangle to approximate a cone in screen space */
	vec2 deltaPos = endPos.xy - startPos.xy;
	
	float adjacentLength = length(deltaPos);
	vec2 adjacentUnit = normalize(deltaPos);


	/* Append offset to adjacent length, so we have our first inner circle */
	adjacentLength += IsoscelesTriangleBase(adjacentLength, coneTheta);
	
	vec4 reflectionColorArray[7];

	for(uint i = 0; i < 7; i++)
	{
		/* Intersection length is the adjacent side, get the opposite side using trigonometry */
		float oppositeLength = IsoscelesTriangleBase(adjacentLength, coneTheta);
		
		/* Calculate in-radius of the isosceles triangle now */
		float incircleSize = IsoscelesTriangleInRadius(oppositeLength, adjacentLength);
		
		/* Get the sample position in screen space */
		vec2 samplePos = startPos.xy + adjacentUnit * (adjacentLength - incircleSize);
		
		/* Clamp the sample pos coord */
		//if(samplePos.x > 1.0f || samplePos.x < 0.0f) continue;
		//if(samplePos.y > 1.0f || samplePos.y < 0.0f) continue;


		/*
		Convert the in-radius into screen space and then check what power N
		we have to raise 2 to reach it. That power N becomes our mip level to sample from.
		*/
		float mipLevel = log2(incircleSize * max(screenSize.x, screenSize.y));
		lod = mipLevel;

		/* Sample the color buffer (using visibility buffer) */
		reflectionColorArray[i] = ConeSampleWeightRoughness(samplePos, mipLevel, specularPower);
		

		if (reflectionColor.a > 1.0f)
			break;
		
		/* Calculate next smaller triangle that approximates the cone in screen space */
		adjacentLength = IsoscelesTriangleNextAdjacent(adjacentLength, incircleSize);
	}

	reflectionColor = vec4(0.0f);
	
	// Reverse the operation
	for(uint i = 6; i >= 0; i--)
	{
		reflectionColor += reflectionColorArray[i];
	
		if (reflectionColor.a > 1.0f)
			break;
	}

	// Normalize the values
	reflectionColor /= reflectionColor.a;

	return vec4(vec3(reflectionColor), lod);
}

vec3 FresnelSchlickRoughness(vec3 F0, float cosTheta, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

// ======================================================
// ======================================================

void main()
{
	ivec2 pixelCoord = ivec2(gl_GlobalInvocationID.xy);
	ivec2 imageOutputSize = imageSize(o_FrameTex).xy;
	vec2 s_UV = (vec2(gl_GlobalInvocationID.xy) + vec2(0.5f)) / vec2(imageOutputSize);

	if (any(greaterThanEqual(pixelCoord, imageOutputSize)))
        return;
	
	// Sample information from the gbuffer
	vec3 viewPos = texture(u_ViewPosTex, s_UV).xyz;

	// If the view positon is 0, then we suppose that we hit geometry that has no information (skybox probably)
	if(viewPos == vec3(0.0)) { imageStore(o_FrameTex, pixelCoord, vec4(vec3(0.0), 1.0)); return; }

	// Decode normals and calculate normals in view space (from model space)
	vec2 decodedNormal = texture(u_NormalTex, s_UV).xy;
    vec3 normal = DecodeNormal(decodedNormal);
	vec3 viewNormal = transpose(inverse(mat3(u_UniformBuffer.ViewMatrix))) * normal;


	
	
	
	


	// Constants
	//const float nearPlane = 0.1f;
	ivec2 baseDepthResolution = GetDepthMipResolution(BASE_HIZ_MIP_LEVEL);
	uint maxSteps = 50;

	// Positions
	const vec3 toPositionVS = normalize(viewPos);
	const vec3 reflectVS = normalize(reflect(normalize(viewPos), normalize(viewNormal)));
	const vec4 positionPrimeSS4 = u_UniformBuffer.ProjectionMatrix * vec4(viewPos.xyz + reflectVS, 1.0f);
	vec3 positionPrimeSS = (positionPrimeSS4.xyz / positionPrimeSS4.w);
	positionPrimeSS.xy = positionPrimeSS.xy * 0.5f + 0.5f;

	// Screen space positions/directions
	vec3 positionSS = vec3(s_UV, textureLod(u_HiZBuffer, s_UV, 0.0f).r);
	vec3 reflectSS = vec3(positionPrimeSS - positionSS);

	uint iterations;
	bool validHit = false;

	vec3 hitCoords = HierarchicalRaymarch(positionSS, reflectSS, baseDepthResolution, BASE_HIZ_MIP_LEVEL, 4, maxSteps, iterations, validHit);


	// SSR
	vec3 reflectionDir = normalize(reflect(normalize(viewPos), normalize(viewNormal)));
	
	
	
	//vec3 hitCoords;
	vec3 reflectionColor;
	{
		//vec3 reflectVector = normalize(reflectionDir) * max(1.0f, -viewPos.z);
		//vec3 screenReflectVector = ProjectPoint(viewPos.xyz + reflectVector * nearPlane) - currentCoords;
	
		/*
		if(u_UniformBuffer.UseHizTracing == 1)
		{
			hitCoords = HiZRayTrace_Optimized(currentCoords, normalize(screenReflectVector), roughness);
		}
		*/


		float roughness = texture(u_NormalTex, s_UV).w;
		float roughnessFactor = roughness / 2.0f;

		if(u_UniformBuffer.UseConeTracing == 1)
		{
			reflectionColor = ConeTrace(s_UV, hitCoords.xy, roughness).xyz;
		}
		else
			reflectionColor = texelFetch(u_PrefilteredColorBuffer, ivec2(hitCoords.xy * textureSize(u_PrefilteredColorBuffer, 0)), 0).rgb;


		vec3 albedo = texture(u_ColorFrameTex, s_UV).rgb;
		float metalness = texture(u_NormalTex, s_UV).z;

		vec3 F0 = mix(vec3(0.04), albedo, metalness);

		reflectionColor.rgb *= FresnelSchlickRoughness(F0, max(dot(normalize(viewNormal), toPositionVS), 0.0), roughness);

		//float confidence = validHit ? ValidateHit(positionSS, ray, toPositionVS, reflectVS, baseDepthResolution, u_SSRInfo.RoughnessDepthTolerance, metalnessRoughness.g) : 0;
	}

	// Fix artifacts
	vec2 dCoords = smoothstep(0.2f, 0.6f, abs(vec2(0.5f) - hitCoords.xy));
	float screenEdgeFactor = clamp(1.0f - (dCoords.x + dCoords.y), 0.0f, 1.0f);
	float multipler = pow(1.0f,
						  relfectionSpecularFalloffExponent) *
						  screenEdgeFactor *
						  -reflectionDir.z;
	
	vec3 resultingColor = reflectionColor * clamp(multipler, 0.0f, 0.9f);
	
	// Custom made function for the roughness weight
	//float roughnessWeight = exp(roughness * 0.6f) * (1 - roughness);

	// Store the values
	imageStore(o_FrameTex, pixelCoord, vec4(vec3(resultingColor), 1.0f));
}