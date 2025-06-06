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
// ======================================================
// Binary search for the alternative ray tracing function.
void LinearRayTraceBinarySearch(inout vec3 rayPos, vec3 rayDir)
{
	for (uint i = 0; i < 5; ++i)
	{
		/* Check if we are out of screen */
		if (rayPos.x < 0.0 || rayPos.x > 1.0 || rayPos.y < 0.0 || rayPos.y > 1.0)
			break;
		
		/* Check if we found our final hit point */
		float depth = textureLod(u_HiZBuffer, rayPos.xy, 0.0f).g;
		float depthDelta = depth - rayPos.z;
		
		if (abs(depthDelta) < BS_DELTA_EPSILON)
			break;
		
		/*
		Move ray forwards if we are in front of geometry and
		move backwards, if we are behind geometry
		*/
		if (depthDelta > 0.0)
			rayPos += rayDir;
		else
			rayPos -= rayDir;
		
		rayDir *= 0.5;
	}
}

// Alternative ray tracing function.
bool LinearRayTrace(inout vec3 rayPos, vec3 rayDir)
{
	vec3 prevPos = rayPos;
	rayDir = normalize(rayDir);
	float stepSize = u_UniformBuffer.RayStepSize;
	int numScreenEdgeHits = 0;
	
	for (uint i = 0; i < u_UniformBuffer.RayStepCount; ++i)
	{
		/* Move to the next sample point */
		prevPos = rayPos;
		rayPos += rayDir * stepSize;
		
		if (rayPos.x < 0.0 || rayPos.x > 1.0 || rayPos.y < 0.0 || rayPos.y > 1.0)
		{
			if (numScreenEdgeHits < 3)
			{
				/* Move ray back if the jump was too long */
				numScreenEdgeHits++;
				rayPos = prevPos;
				stepSize *= 0.5;
				continue;
			}
			else
				break;
		}
		else
			numScreenEdgeHits = 0;
		
		/* Check if the ray hit any geometry (delta < 0) */
		float depth = textureLod(u_HiZBuffer, rayPos.xy, 0.0f).g;
		float depthDelta = depth - rayPos.z;
		
		if (depthDelta < 0.0)
		{
//			if (Linearize(depthDelta) < -0.01)
//				break;
			
			/*
			Move between the current and previous point and
			make a binary search, to quickly find the final hit point
			*/
			rayPos = (rayPos + prevPos) * 0.5;
			LinearRayTraceBinarySearch(rayPos, rayDir * (stepSize * 0.25));
			return true;
		}
		
		stepSize *= 1.5;
	}
	
	rayPos = INVALID_HIT_POINT;
	return false;
}

// -----------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------

vec3 IntersectDepthPlane(vec3 rayOrigin, vec3 rayDir, float t)
{
	return rayOrigin + rayDir * t;
}

vec2 GetCellCount(vec2 size, float level)
{
	return floor(size / (level > 0.0 ? exp2(level) : 1.0));
}

vec2 GetCell(vec2 ray, vec2 cellCount)
{
	return floor(ray * cellCount);
}


/*
	This function computes the new ray position where
	the ray intersects with the specified cell's boundary.
*/
vec3 IntersectCellBoundary(vec3 rayOrigin, vec3 rayDir, vec2 cellIndex, vec2 cellCount, vec2 crossStep, vec2 crossOffset)
{
	/*
		Determine in which adjacent cell the ray resides.
		The cell index always refers to the left-top corner of a cell.
		So add "crossStep" whose components are either 1.0
		(for positive direction) or 0.0 (for negative direction),
		and then add "crossOffset" so that we are in the center of that cell.
	*/
	vec2 cell = cellIndex + crossStep;
	cell /= cellCount;
	cell += crossOffset;
	
	/* Now compute interpolation factor "t" with this cell position and the ray origin */
	vec2 delta = cell - rayOrigin.xy;
	delta /= rayDir.xy;
	
	float t = min(delta.x, delta.y);
	
	/* Return final ray position */
	return IntersectDepthPlane(rayOrigin, rayDir, t);
}

vec2 GetDepthPlanes(vec2 pos, float level)
{
	//vec2 blueNoise = (2.0 * SampleBlueNoise(ivec2(gl_GlobalInvocationID.xy)).rg - vec2(1.0));
	//float noiseWeight = 0.00673;
	/* Texture lookup with <linear-clamp> sampler */
	//return textureLod(u_HiZBuffer, pos + blueNoise * noiseWeight, level).yx;
	return textureLod(u_HiZBuffer, pos, level).yx;
}

bool CrossedCellBoundary(vec2 cellIndexA, vec2 cellIndexB)
{
	return cellIndexA.x != cellIndexB.x || cellIndexA.y != cellIndexB.y;
}

/*
	Hi-Z ray tracing function.
	rayOrigin: Ray start position (in screen space).
	rayDir: Ray reflection vector (in screen space).
*/
vec3 HiZRayTrace_Optimized(vec3 rayOrigin, vec3 rayDir, float roughness)
{
	const vec2 hiZSize = vec2(textureSize(u_HiZBuffer, 0).xy);
	//const float maxMipLevel = floor(log2(max(hiZSize.x, hiZSize.y))) - 1;
	const float maxMipLevel = 5;

	/* Check if ray points towards the camera */
	if (rayDir.z <= HIZ_VIEWDIR_EPSILON)
	{
		if (LinearRayTrace(rayOrigin, rayDir))
		{
			return rayOrigin;
		}
		return INVALID_HIT_POINT;
	}

	
	/*
	Get the cell cross direction and a small offset
	to enter the next cell when doing cell crossing
	*/
	vec2 crossStep = vec2(SIGN(rayDir.x), SIGN(rayDir.y));
	vec2 crossOffset = crossStep * HIZ_CROSS_EPSILON;
	crossStep = saturate(crossStep);
	
	/* Set hit point to the original screen coordinate and depth */
	vec3 rayPos = rayOrigin;
	
	/* Scale vector such that z is 1.0 (maximum depth) */
	rayDir = rayDir / rayDir.z;
	
	/* Set starting point to the point where z equals 0.0 (minimum depth) */
	rayOrigin = rayOrigin + rayDir * -rayOrigin.z;
	
	/* Cross to next cell so that we don't get a self-intersection immediately */
	float level = 2.0f;
	
	vec2 firstCellCount = GetCellCount(hiZSize, level);
	vec2 rayCell = GetCell(rayPos.xy, firstCellCount);
	rayPos = IntersectCellBoundary(rayOrigin, rayDir, rayCell, firstCellCount, crossStep, crossOffset);


	
	/* Main tracing iteration loop */
	uint i = 0;
	
#define HIZ_STOP_LEVEL      2.0f
#define HIZ_MAX_ITERATIONS  32

	uint HiZMaxIterations = HIZ_MAX_ITERATIONS * uint(1.0 - roughness);

	for (; level >= HIZ_STOP_LEVEL && i < HIZ_MAX_ITERATIONS; i++)
	{
		if (rayPos.x < 0.0 || rayPos.x > 1.0 || rayPos.y < 0.0 || rayPos.y > 1.0)
		{
			break;
		}

		/* Get cell number of our current ray */
		vec2 cellCount = GetCellCount(hiZSize, level);
		vec2 oldCellIndex = GetCell(rayPos.xy, cellCount);
		
		/* Get minimum depth plane in which the current ray resides */
		vec2 zMinMax = GetDepthPlanes(rayPos.xy, level);


		/* Intersect only if ray depth is between minimum and maximum depth planes */
		vec3 tmpRay = rayOrigin + rayDir * max(rayPos.z, zMinMax.r); // MIN
		
		/* Get new cell number */
		vec2 newCellIndex = GetCell(tmpRay.xy, cellCount);
		
		/* If the new cell number is different from the old cell number, we know we crossed a cell */
		if(newCellIndex.x != oldCellIndex.x || newCellIndex.y != oldCellIndex.y)
		{
			/*
			Intersect the boundary of that cell instead,
			and go up a level for taking a larger step next iteration
			*/
			vec2 cell = oldCellIndex + crossStep;
			cell /= cellCount;
			cell += crossOffset;
	
			/* Now compute interpolation factor "t" with this cell position and the ray origin */
			vec2 delta = cell - rayOrigin.xy;
			delta /= rayDir.xy;
	
			float t = min(delta.x, delta.y);
	
			/* Return final ray position */
			tmpRay = IntersectDepthPlane(rayOrigin, rayDir, t);

			level = min(maxMipLevel, level + 1.0);
		}
		else
		{
			/* Go down a level in the Hi-Z */
			level -= 1.0;
		}

		rayPos = tmpRay;
	}

	return vec3(rayPos.xy, 0.0f);
}





vec3 HiZRayTrace(vec3 rayOrigin, vec3 rayDir, float roughness)
{
	const vec2 hiZSize = vec2(textureSize(u_HiZBuffer, 0).xy);
	//const float maxMipLevel = floor(log2(max(hiZSize.x, hiZSize.y))) - 1;
	const float maxMipLevel = 5;


	/* Check if ray points towards the camera */
	if (rayDir.z <= HIZ_VIEWDIR_EPSILON)
	{
		if (LinearRayTrace(rayOrigin, rayDir))
		{
			return rayOrigin;
		}
		return INVALID_HIT_POINT;
	}
	
	
	//vec3 rayOrigin_begin = rayOrigin;
	//vec3 rayDir_begin = rayDir;
	//
	//if(LinearRayTrace(rayOrigin_begin, rayDir_begin))
	//{
	//	return vec3(rayOrigin_begin.xy, 0.0f);
	//}

	
	/*
	Get the cell cross direction and a small offset
	to enter the next cell when doing cell crossing
	*/
	vec2 crossStep = vec2(SIGN(rayDir.x), SIGN(rayDir.y));
	vec2 crossOffset = crossStep * HIZ_CROSS_EPSILON;
	crossStep = saturate(crossStep);
	
	/* Set hit point to the original screen coordinate and depth */
	vec3 rayPos = rayOrigin;
	
	/* Scale vector such that z is 1.0 (maximum depth) */
	rayDir = rayDir / rayDir.z;
	
	/* Set starting point to the point where z equals 0.0 (minimum depth) */
	rayOrigin = IntersectDepthPlane(rayOrigin, rayDir, -rayOrigin.z); // rayOrigin + rayDir * (-rayOrigin.z)
	
	/* Cross to next cell so that we don't get a self-intersection immediately */
	float level = 3.0f;
	
	vec2 firstCellCount = GetCellCount(hiZSize, level);
	vec2 rayCell = GetCell(rayPos.xy, firstCellCount);
	rayPos = IntersectCellBoundary(rayOrigin, rayDir, rayCell, firstCellCount, crossStep, crossOffset);


	
	/* Main tracing iteration loop */
	uint i = 0;
	
#define HIZ_STOP_LEVEL      2.0f
#define HIZ_MAX_ITERATIONS  32

	uint HiZMaxIterations = HIZ_MAX_ITERATIONS * uint(1.0 - roughness);

	for (; level >= HIZ_STOP_LEVEL && i < HIZ_MAX_ITERATIONS; i++)
	{
		if (rayPos.x < 0.0 || rayPos.x > 1.0 || rayPos.y < 0.0 || rayPos.y > 1.0)
		{
			break;
		}

		/* Get cell number of our current ray */
		vec2 cellCount = GetCellCount(hiZSize, level);
		vec2 oldCellIndex = GetCell(rayPos.xy, cellCount);
		
		/* Get minimum depth plane in which the current ray resides */
		vec2 zMinMax = GetDepthPlanes(rayPos.xy, level);


#if 1
		/* Intersect only if ray depth is between minimum and maximum depth planes */
		vec3 tmpRay = IntersectDepthPlane(rayOrigin, rayDir, max(rayPos.z, zMinMax.r)); // MIN
		//vec3 tmpRay = IntersectDepthPlane(rayOrigin, rayDir, clamp(rayPos.z, zMinMax.r, zMinMax.g)); // MIN
		
		/* Get new cell number */
		vec2 newCellIndex = GetCell(tmpRay.xy, cellCount);
		
		/* If the new cell number is different from the old cell number, we know we crossed a cell */
		if (CrossedCellBoundary(oldCellIndex, newCellIndex))// || tmpRay.z > zMinMax.g + 0.001)
		{
			/*
			Intersect the boundary of that cell instead,
			and go up a level for taking a larger step next iteration
			*/
			tmpRay = IntersectCellBoundary(rayOrigin, rayDir, oldCellIndex, cellCount, crossStep, crossOffset);
			level = min(maxMipLevel, level + 1.0);
		}
		else
		{
			/* Go down a level in the Hi-Z */
			level -= 1.0;
		}

		rayPos = tmpRay;
#endif

		
		
		
	}

	return vec3(rayPos.xy, 0.0f);
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
#define MAX_SPEC_POWER 2048.0f
    return clamp(2.0f / (roughness * roughness) - 2.0f, 0.0f, MAX_SPEC_POWER);
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
	float lod = 0.0f;
	vec2 screenSize = vec2(imageSize(o_FrameTex).xy);

	float specularPower = RoughnessToSpecularPower(roughness);

	/* Information */
	vec4 reflectionColor = vec4(0.0f);
	float coneTheta = SpecularPowerToConeAngle(specularPower) * 0.5f;

	
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

vec3 FresnelShlick(in float cosTheta, in vec3 F0)
{
	return F0 + (1.0f - F0) * pow(1.0f - cosTheta, 5.0f);
}

// ======================================================
// ======================================================

void main()
{
	//s_UV = vec2(gl_GlobalInvocationID.xy) / u_UniformBuffer.ScreenSize.xy;
	ivec2 pixelCoord = ivec2(gl_GlobalInvocationID.xy);
	ivec2 imageOutputSize = imageSize(o_FrameTex).xy;
	vec2 s_UV = (vec2(gl_GlobalInvocationID.xy) + vec2(0.5f)) / vec2(imageOutputSize);

	if (any(greaterThanEqual(pixelCoord, imageOutputSize)))
        return;
	

	//if(gl_GlobalInvocationID.x > u_UniformBuffer.ScreenSize.x || gl_GlobalInvocationID.y > u_UniformBuffer.ScreenSize.y) return;

	// Sample information from the gbuffer
	vec3 viewPos = texture(u_ViewPosTex, s_UV).xyz;

	if(viewPos == vec3(0.0))
	{
		imageStore(o_FrameTex, pixelCoord, vec4(vec3(0.0), 1.0));
		return;
	}

	vec3 currentColor = texture(u_ColorFrameTex, s_UV).rgb;
	float metallic = texture(u_NormalTex, s_UV).z;
	float roughness = texture(u_NormalTex, s_UV).w;
	float roughnessFactor = roughness / 2.0f;

	if(viewPos.x == 0.0f && viewPos.y == 0.0f && viewPos.z == 0.0f) {
		imageStore(o_FrameTex, pixelCoord, vec4(currentColor, 1.0f));
		return;
	}

	// Decode normals and calculate normals in view space (from model space)
	vec2 decodedNormal = texture(u_NormalTex, s_UV).xy;
    vec3 normal = DecodeNormal(decodedNormal);
	vec3 viewNormal = transpose(inverse(mat3(u_UniformBuffer.ViewMatrix))) * normal;

	// Fresnel factor
	vec3 F0 = vec3(0.04f);
	F0      = mix(F0, currentColor, metallic);
	vec3 fresnel = FresnelShlick(max(dot(normalize(viewPos), normalize(viewNormal)), 0.0f), F0);

	// SSR
	vec3 reflectionDir = normalize(reflect(normalize(viewPos), normalize(viewNormal)));
	
	
	const float nearPlane = 0.1f;
	vec3 currentCoords = vec3(s_UV, textureLod(u_HiZBuffer, s_UV, 0.0f).g);

	
	vec3 hitCoords;
	vec3 reflectionColor;
	if(u_UniformBuffer.UseConeTracing == 0)
	{
		vec3 worldPos = vec3(u_UniformBuffer.InvViewMatrix * vec4(viewPos, 1.0f));
		vec3 jitter = mix(vec3(0.0f), vec3(HashFunc(worldPos)), roughnessFactor);

		vec3 reflectVector = normalize(reflectionDir + jitter) * max(1.0f, -viewPos.z);
		vec3 screenReflectVector = ProjectPoint(viewPos.xyz + reflectVector * nearPlane) - currentCoords;

		hitCoords = HiZRayTrace_Optimized(currentCoords, normalize(screenReflectVector), roughness);
		reflectionColor = texture(u_PrefilteredColorBuffer, hitCoords.xy).rgb;
	}
	else
	{
		vec3 reflectVector = normalize(reflectionDir) * max(1.0f, -viewPos.z);
		vec3 screenReflectVector = ProjectPoint(viewPos.xyz + reflectVector * nearPlane) - currentCoords;
	
		if(u_UniformBuffer.UseHizTracing == 1)
		{
			hitCoords = HiZRayTrace_Optimized(currentCoords, normalize(screenReflectVector), roughness);
		}
		else
		{
			LinearRayTrace(currentCoords, normalize(screenReflectVector));
			hitCoords = currentCoords;
		}

		reflectionColor = ConeTrace(s_UV, hitCoords.xy, roughnessFactor).xyz;

		//vec2 blueNoise = (2.0 * SampleBlueNoise(ivec2(gl_GlobalInvocationID.xy)).rg - vec2(1.0));
		//reflectionColor += textureLod(u_PrefilteredColorBuffer, hitCoords.xy + blueNoise * 0.00673, 2).rgb;
		//reflectionColor += textureLod(u_PrefilteredColorBuffer, hitCoords.xy, 3).rgb;
		//reflectionColor += textureLod(u_PrefilteredColorBuffer, hitCoords.xy + blueNoise * 0.00673, 4).rgb;
		//reflectionColor /= 4.0;
	}

	// Fix artifacts
	vec2 dCoords = smoothstep(0.2f, 0.5f, abs(vec2(0.5f) - hitCoords.xy));
	float screenEdgeFactor = clamp(1.0f - (dCoords.x + dCoords.y), 0.0f, 1.0f);
	float multipler = pow(1.0f,
						  relfectionSpecularFalloffExponent) *
						  screenEdgeFactor *
						  -reflectionDir.z;

	vec3 resultingColor = reflectionColor * clamp(multipler, 0.0f, 0.9f);

	// Custom made function for the roughness weight
	float roughnessWeight = exp(roughness * 0.6f) * (1 - roughness);

	// Store the values
	imageStore(o_FrameTex, pixelCoord, vec4(vec3(resultingColor * roughnessWeight), 1.0f));
}