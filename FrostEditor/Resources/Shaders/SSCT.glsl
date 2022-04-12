/*
	Sources:
		http://roar11.com/2015/07/screen-space-glossy-reflections/
		https://github.com/LukasBanana/ForkENGINE
		https://lukas-hermanns.info/download/bachelorthesis_ssct_lhermanns.pdf


	// TODO: Maybe add a visibility buffer for better cone weight distribuiton. Look into:
		https://github.com/LukasBanana/ForkENGINE/blob/master/shaders/SSCTVisibilityMapPixelShader.glsl
		http://what-when-how.com/Tutorial/topic-547pjramj8/GPU-Pro-Advanced-Rendering-Techniques-192.html
		https://lukas-hermanns.info/download/bachelorthesis_ssct_lhermanns.pdf (p.32)

*/

#type compute
#version 460

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(binding = 0) uniform sampler2D u_ColorFrameTex;

layout(binding = 1) uniform sampler2D u_ViewPosTex;
layout(binding = 2) uniform sampler2D u_HiZBuffer;
layout(binding = 3) uniform sampler2D u_NormalTex; // includes compressed vec2 normals + metalness + roughness
layout(binding = 4) uniform sampler2D u_PrefilteredColorBuffer;
layout(binding = 5) uniform sampler2D u_VisibilityBuffer;
layout(binding = 8) uniform sampler2D u_AOBuffer;
layout(binding = 9) uniform sampler2D u_BloomTexture;

layout(binding = 6, rgba8) uniform writeonly image2D o_FrameTex;

layout(binding = 7) uniform UniformBuffer {
	mat4 ProjectionMatrix;
	mat4 InvProjectionMatrix;
	mat4 ViewMatrix;
	mat4 InvViewMatrix;
	vec4 ScreenSize;

	/*
	float SampleCount;
	float RayStep;
	float IterationCount;
	float DistanceBias;
	
	float DebugDraw;
	float IsBinarySearchEnabled;
	float IsAdaptiveStepEnabled;
	float IsExponentialStepEnabled;
	*/
} u_UniformBuffer;

// Global variables
vec2 s_UV;


// Constants
const float rayStep = 0.1f;
const float minRaySteps = 0.1f;
const float maxRayStep = 1.2f;
const int maxSteps = 30;
const float searchDist = 5.0f;
const int numBinarySearchSteps = 5;
const float relfectionSpecularFalloffExponent = 6.0f;


// Defines
#define HIZ_CROSS_EPSILON		(vec2(0.5f) / u_UniformBuffer.ScreenSize.xy)

#define INVALID_HIT_POINT		vec3(-1.0)
#define HIZ_VIEWDIR_EPSILON		0.00001

#define SIGN(x)                 ((x) >= 0.0 ? 1.0 : -1.0)
#define saturate(x)             clamp(x, 0.0, 1.0)

#define BS_DELTA_EPSILON 0.0002


// https://aras-p.info/texts/CompactNormalStorage.html
vec3 DecodeNormals(vec2 enc)
{
    float scale = 1.7777f;
    vec3 nn = vec3(enc, 0.0f) * vec3(2 * scale, 2 * scale,0) + vec3(-scale, -scale,1);
    
	float g = 2.0 / dot(nn.xyz,nn.xyz);

    vec3 n;
    n.xy = g*nn.xy;
    n.z = g-1;

	n = clamp(n, vec3(-1.0f), vec3(1.0f));

    return n;
}

// Random hash function
const vec3 r_Scale = vec3(0.8f);
const float r_K = 19.19f;
vec3 HashFunc(vec3 a)
{
	a = fract(a * r_Scale);
	a += dot(a, a.xyz + r_K);
	return fract((a.xxy + a.yxx));
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

vec3 ProjectPoint(vec3 viewPoint)
{
	vec4 projPoint = u_UniformBuffer.ProjectionMatrix * vec4(viewPoint, 1.0f);
	projPoint.xyz /= projPoint.w;
	projPoint.xy = projPoint.xy * 0.5f + 0.5f;
	return projPoint.xyz;
}

vec3 BinarySearch_ViewSpace(inout vec3 dir, inout vec3 hitCoord, out float dDepth)
{
	float depth;
	vec4 projectedCoord;
	
	for(int i = 0; i < numBinarySearchSteps; i++)
	{
		projectedCoord = u_UniformBuffer.ProjectionMatrix * vec4(hitCoord, 1.0f);
		projectedCoord.xy /= projectedCoord.w;
		projectedCoord.xy = projectedCoord.xy * 0.5f + 0.5f;

		vec3 viewPos = texture(u_ViewPosTex, projectedCoord.xy).xyz;
		depth = viewPos.z;

		dDepth = hitCoord.z - depth;

		dir *= 0.5f;

		if(dDepth > 0.0f)
			hitCoord += dir;
		else
			hitCoord -= dir;

	}

	projectedCoord = u_UniformBuffer.ProjectionMatrix * vec4(hitCoord, 1.0f);
	projectedCoord.xy /= projectedCoord.w;
	projectedCoord.xy = projectedCoord.xy * 0.5f + 0.5f;

	return vec3(projectedCoord.xy, dDepth);
}

vec4 RayCast_ViewSpace(in vec3 dir, inout vec3 hitCoord, out float dDepth)
{
	dir *= rayStep;

	float depth = 0.0f;
	int steps = 0;

	vec4 projectedCoord = vec4(0.0f);

	vec4 result = vec4(0.0f);

	for(int i = 0; i < maxSteps; i++)
	{
		hitCoord += dir;

		projectedCoord = u_UniformBuffer.ProjectionMatrix * vec4(hitCoord, 1.0f);
		projectedCoord.xy /= projectedCoord.w;
		projectedCoord.xy = projectedCoord.xy * 0.5f + 0.5f;

		if(projectedCoord.x > 1.0f || projectedCoord.x < 0.0f) continue;
		if(projectedCoord.y > 1.0f || projectedCoord.y < 0.0f) continue;

		vec3 position = texture(u_ViewPosTex, projectedCoord.xy).xyz;
		depth = position.z;

		if(depth > 1000.0f)
			continue;

		dDepth = hitCoord.z - depth;

		if((dir.z - dDepth) < maxRayStep)
		{
			if(dDepth <= 0.0f)
			{
				vec3 binarySearchResult = BinarySearch_ViewSpace(dir, hitCoord, dDepth);
				result = vec4(binarySearchResult, 1.0f);

				break;
			}
		}
	}
	return vec4(result);
}




// Binary search for the alternative ray tracing function.
void LinearRayTraceBinarySearch(inout vec3 rayPos, vec3 rayDir)
{
	for (uint i = 0; i < 5; ++i)
	{
		/* Check if we are out of screen */
		if (rayPos.x < 0.0 || rayPos.x > 1.0 || rayPos.y < 0.0 || rayPos.y > 1.0)
			break;
		
		/* Check if we found our final hit point */
		float depth = textureLod(u_HiZBuffer, rayPos.xy, 0.0f).r;
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
	float stepSize = 0.04;
	int numScreenEdgeHits = 0;
	
	for (uint i = 0; i < 5; ++i)
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
		float depth = textureLod(u_HiZBuffer, rayPos.xy, 0.0f).r;
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

float GetDepthPlanes(vec2 pos, float level)
{
	/* Texture lookup with <linear-clamp> sampler */
	return textureLod(u_HiZBuffer, pos, level).r;
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
vec3 HiZRayTrace(vec3 rayOrigin, vec3 rayDir)
{
	const vec2 hiZSize = u_UniformBuffer.ScreenSize.xy;
	const float maxMipLevel = floor(log2(max(hiZSize.x, hiZSize.y))) - 1;

	
	/* Check if ray points towards the camera */
	if (rayDir.z <= HIZ_VIEWDIR_EPSILON)
	{
		if (LinearRayTrace(rayOrigin, rayDir))
		{
			return rayOrigin;
		}
		return INVALID_HIT_POINT;
	}


	vec3 rayOrigin_begin = rayOrigin;
	vec3 rayDir_begin = rayDir;
	
	if(LinearRayTrace(rayOrigin_begin, rayDir_begin))
	{
		return vec3(rayOrigin_begin.xy, 0.0f);
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
	rayOrigin = IntersectDepthPlane(rayOrigin, rayDir, -rayOrigin.z); // rayOrigin + rayDir * (-rayOrigin.z)
	
	/* Cross to next cell so that we don't get a self-intersection immediately */
	float level = 2.0f;
	
	vec2 firstCellCount = GetCellCount(hiZSize, level);
	vec2 rayCell = GetCell(rayPos.xy, firstCellCount);
	rayPos = IntersectCellBoundary(rayOrigin, rayDir, rayCell, firstCellCount, crossStep, crossOffset);


	
	/* Main tracing iteration loop */
	uint i = 0;
	
#define HIZ_STOP_LEVEL      2.0f
#define HIZ_MAX_ITERATIONS  32

	for (; level >= HIZ_STOP_LEVEL && i < HIZ_MAX_ITERATIONS; i++)
	{
		if (rayPos.x < 0.0 || rayPos.x > 1.0 || rayPos.y < 0.0 || rayPos.y > 1.0)
		{
			//rayPos = INVALID_HIT_POINT;
			break;
			//level = 0.0f;
		}

		/* Get cell number of our current ray */
		vec2 cellCount = GetCellCount(hiZSize, level);
		vec2 oldCellIndex = GetCell(rayPos.xy, cellCount);
		
		/* Get minimum depth plane in which the current ray resides */
		float zMin = GetDepthPlanes(rayPos.xy, level);
		
		
		/* Intersect only if ray depth is between minimum and maximum depth planes */
		vec3 tmpRay = IntersectDepthPlane(rayOrigin, rayDir, max(rayPos.z, zMin)); // MIN
		
		/* Get new cell number */
		vec2 newCellIndex = GetCell(tmpRay.xy, cellCount);
		
		/* If the new cell number is different from the old cell number, we know we crossed a cell */
		if (CrossedCellBoundary(oldCellIndex, newCellIndex))
		{
			/*
			Intersect the boundary of that cell instead,
			and go up a level for taking a larger step next iteration
			*/
			tmpRay = IntersectCellBoundary(rayOrigin, rayDir, oldCellIndex, cellCount, crossStep, crossOffset);
			level = min(maxMipLevel, level + 2.0);
		}
		
		rayPos = tmpRay;
		
		
		
		/* Go down a level in the Hi-Z */
		level -= 1.0;
	}

	return vec3(rayPos.xy, 0.0f);

	/*
	if(rayPos == INVALID_HIT_POINT)
	{
		if(LinearRayTrace(rayOrigin_begin, rayDir_begin))
		{
			return vec3(rayOrigin_begin.xy, 0.0f);
		}
		return INVALID_HIT_POINT;
	}
	else
	{
		if(LinearRayTrace(rayOrigin_begin, rayDir_begin))
		{
			//return vec3(rayOrigin_begin.xy, 0.0f);
			return vec3((rayPos.xy * vec2(0.03f) + rayOrigin_begin.xy * vec2(0.97f)), 0.0f);
		}
		else
		{
			return vec3(rayPos.xy, 0.0f);
		}

		//return INVALID_HIT_POINT;
	}
	*/


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


// Temporary: weights for the cones
float weight[10] = float[] (0.327027, 0.2945946, 0.216216, 0.194054, 0.166216, 0.126216, 0.0946216, 0.0826216, 0.0716216, 0.0596216);


/*
vec4 ConeSampleWeightedColor(vec2 samplePos, float mipLevel)
{
	float currentWeight = weight[int(mipLevel)];
	vec3 color = textureLod(u_PrefilteredColorBuffer, samplePos.xy, mipLevel).xyz;
	
	return vec4(color, 0.0f);
}
*/

vec4 ConeSampleWeightedColor(vec2 samplePos, float mipLevel)
{
	/* Sample color buffer with pre-integrated visibility */
	vec3 color = textureLod(u_PrefilteredColorBuffer, samplePos, mipLevel).rgb;
	float visibility = textureLod(u_VisibilityBuffer, samplePos, mipLevel).r;
	return vec4(color * visibility, visibility);
}

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
		float mipLevel = log2(incircleSize * max(u_UniformBuffer.ScreenSize.x, u_UniformBuffer.ScreenSize.y));
		lod = mipLevel;

		/* Sample the color buffer (using visibility buffer) */
		reflectionColorArray[i] = ConeSampleWeightedColor(samplePos, mipLevel);
		

		if (reflectionColor.a > 1.0f)
			break;
		
		/* Calculate next smaller triangle that approximates the cone in screen space */
		adjacentLength = IsoscelesTriangleNextAdjacent(adjacentLength, incircleSize);
	}

	reflectionColor = vec4(0.0f);
	
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

vec3 fresnelShlick(in float cosTheta, in vec3 F0)
{
	return F0 + (1.0f - F0) * pow(1.0f - cosTheta, 5.0f);
}

vec3 MultiBounce(float ao, vec3 albedo)
{
	vec3 x = vec3(ao);

	vec3 a = 2.0404 * albedo - vec3(0.3324);
	vec3 b = -4.7951 * albedo + vec3(0.6417);
	vec3 c = 2.7552 * albedo + vec3(0.6903);

	return max(x, ((x * a + b) * x + c) * x);
}

bool IsSurfaceInBloom()
{
	ivec2 pixelCoord = ivec2(gl_GlobalInvocationID.xy);
	vec3 color = texelFetch(u_BloomTexture, pixelCoord, 0).rgb;
	if(color.x > 0.99f || color.y > 0.99f || color.z > 0.99f) return true;
	return false;
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

void main()
{
	s_UV = vec2(gl_GlobalInvocationID.xy) / u_UniformBuffer.ScreenSize.xy;
	ivec2 pixelCoord = ivec2(gl_GlobalInvocationID.xy);

	if(gl_GlobalInvocationID.x > u_UniformBuffer.ScreenSize.x || gl_GlobalInvocationID.y > u_UniformBuffer.ScreenSize.y) return;

	// Sample information from the gbuffer
	vec3 viewPos = texelFetch(u_ViewPosTex, pixelCoord, 0).xyz;
	vec3 currentColor = texelFetch(u_ColorFrameTex, pixelCoord, 0).rgb;
	float metallic = texelFetch(u_NormalTex, pixelCoord, 0).z;
	float roughness = texelFetch(u_NormalTex, pixelCoord, 0).w;
	float roughnessFactor = roughness / 2.0f;

	if(viewPos.x == 0.0f && viewPos.y == 0.0f && viewPos.z == 0.0f) {
		imageStore(o_FrameTex, pixelCoord, vec4(currentColor, 1.0f));
		return; 
	}

	// Decode normals and calculate normals in view space (from model space)
	vec2 decodedNormals = texelFetch(u_NormalTex, pixelCoord, 0).xy;
    vec3 normal = DecodeNormals(decodedNormals);
	vec3 viewNormal = transpose(inverse(mat3(u_UniformBuffer.ViewMatrix))) * normal;

	// Fresnel factor
	vec3 F0 = vec3(0.04f);
	F0      = mix(F0, currentColor, metallic);
	vec3 fresnel = fresnelShlick(max(dot(normalize(viewPos), normalize(viewNormal)), 0.0f), F0);

	// SSR
	float depth = 0.0f;
	vec3 hitPos = viewPos.xyz;

	vec3 worldPos = vec3(vec4(viewPos, 1.0f) * u_UniformBuffer.InvViewMatrix);
	vec3 reflectionDir = normalize(reflect(normalize(viewPos), normalize(viewNormal)));


	
	const float nearPlane = 0.1f;
	vec3 currentCoords = vec3(s_UV, textureLod(u_HiZBuffer, s_UV, 0.0f).r);

	vec3 reflectVector = normalize(reflectionDir) * max(1.0f, -viewPos.z);
	vec3 screenReflectVector = ProjectPoint(viewPos.xyz + reflectVector * nearPlane) - currentCoords;

	// Firstly we ray trace to find the intersection point
	//vec3 hitCoords = RayCast_ViewSpace(normalize(reflectionDir) * max(1.0f, -viewPos.z), hitPos, depth).xyz;
	vec3 hitCoords = HiZRayTrace(currentCoords, screenReflectVector);


	// 0.2 = 22.6 degrees, 0.1 = 11.4 degrees, 0.07 = 8 degrees angle
	vec4 coneTracedColor = ConeTrace(s_UV, hitCoords.xy, roughnessFactor);

	// Fix artifacts
	vec2 dCoords = smoothstep(0.2f, 0.5f, abs(vec2(0.5f) - hitCoords.xy));
	float screenEdgeFactor = clamp(1.0f - (dCoords.x + dCoords.y), 0.0f, 1.0f);
	float multipler = pow(1.0f,
						  relfectionSpecularFalloffExponent) *
						  screenEdgeFactor *
						  -reflectionDir.z;

	vec3 resultingColor = coneTracedColor.xyz * clamp(multipler, 0.0f, 0.9f);

	// Store the values
	imageStore(o_FrameTex, pixelCoord, vec4(vec3(resultingColor), 1.0f));
}