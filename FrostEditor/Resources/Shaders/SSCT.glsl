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
layout(binding = 2) uniform sampler2D u_DepthTex;
layout(binding = 3) uniform sampler2D u_NormalTex; // inclues compressed vec2 normals + metalness + roughness
layout(binding = 4) uniform sampler2D u_PrefilteredColorBuffer;

layout(binding = 5, rgba16f) uniform writeonly image2D o_FrameTex;

layout(binding = 6) uniform UniformBuffer {
	mat4 ProjectionMatrix;
	mat4 InvProjectionMatrix;
	mat4 ViewMatrix;
	mat4 InvViewMatrix;
	vec4 ScreenSize;
	
	float SampleCount;
	float RayStep;
	float IterationCount;
	float DistanceBias;
	
	float DebugDraw;
	float IsBinarySearchEnabled;
	float IsAdaptiveStepEnabled;
	float IsExponentialStepEnabled;
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

// https://aras-p.info/texts/CompactNormalStorage.html
vec3 DecodeNormals(vec2 enc)
{
    float scale = 1.7777f;
    vec3 nn = vec3(enc, 0.0f) * vec3(2 * scale, 2 * scale,0) + vec3(-scale, -scale,1);
    
	float g = 2.0 / dot(nn.xyz,nn.xyz);

    vec3 n;
    n.xy = g*nn.xy;
    n.z = g-1;

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

uvec3 murmurHash33(uvec3 src) {
    const uint M = 0x5bd1e995u;
    uvec3 h = uvec3(1190494759u, 2147483647u, 3559788179u);
    src *= M; src ^= src>>24u; src *= M;
    h *= M; h ^= src.x; h *= M; h ^= src.y; h *= M; h ^= src.z;
    h ^= h>>13u; h *= M; h ^= h>>15u;
    return h;
}

// 3 outputs, 3 inputs
vec3 hash33(vec3 src) {
    uvec3 h = murmurHash33(floatBitsToUint(src));
    return uintBitsToFloat(h & 0x007fffffu | 0x3f800000u) - 1.0;
}

vec3 BinarySearch(inout vec3 dir, inout vec3 hitCoord, out float dDepth)
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

vec4 RayCast(in vec3 dir, inout vec3 hitCoord, out float dDepth)
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
				vec3 binarySearchResult = BinarySearch(dir, hitCoord, dDepth);
				result = vec4(binarySearchResult, 1.0f);

				break;
			}
		}
	}
	return vec4(result);
}


float IsoscelesTriangleBase(float adjacentLength, float coneTheta)
{
	/*
		Simple trig and algebra - soh, cah, toa - tan(theta) = opp/adj,
		opp = tan(theta) * adj, then multiply by 2 for isosceles triangle base
	*/
	//return 2.0 * coneTheta * adjacentLength;
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
#define CNST_MAX_SPECULAR_EXP 64

    // based on phong distribution model
    if(specularPower >= exp2(CNST_MAX_SPECULAR_EXP))
    {
        return 0.0f;
    }
    const float xi = 0.244f;
    float exponent = 1.0f / (specularPower + 1.0f);
    return acos(pow(xi, exponent));
}

// Temporary: weights for the cones
float weight[10] = float[] (0.327027, 0.2945946, 0.216216, 0.194054, 0.166216, 0.126216, 0.0946216, 0.0826216, 0.0716216, 0.0596216);


vec4 ConeSampleWeightedColor(vec2 samplePos, float mipLevel)
{
	float currentWeight = weight[int(mipLevel)];
	vec3 color = textureLod(u_PrefilteredColorBuffer, samplePos.xy, mipLevel).xyz;
	
	return vec4(color, 0.0f);
}

// https://github.com/LukasBanana/ForkENGINE/blob/master/shaders/SSCTReflectionPixelShader.glsl#L600
vec4 ConeTrace(vec3 dir, float tanHalfAngle, vec2 startPos, vec2 endPos, out vec2 lastProjCoord, float roughness)
{
	float lod = 0.0f;
	vec4 color = vec4(0.0f);
	float dist = 1.0f;
	float remainingAlpha = 1.0f;

	float gloss = 1.0f - roughness;
	float glossMult = gloss;
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
	
	for(uint i = 0; i < 7; i++)
	{
		/* Intersection length is the adjacent side, get the opposite side using trigonometry */
		float oppositeLength = IsoscelesTriangleBase(adjacentLength, coneTheta);
		
		/* Calculate in-radius of the isosceles triangle now */
		float incircleSize = IsoscelesTriangleInRadius(oppositeLength, adjacentLength);
		
		/* Get the sample position in screen space */
		vec2 samplePos = startPos.xy + adjacentUnit * (adjacentLength - incircleSize);
		

		/*
		Convert the in-radius into screen space and then check what power N
		we have to raise 2 to reach it. That power N becomes our mip level to sample from.
		*/
		float mipLevel = log2(incircleSize * max(u_UniformBuffer.ScreenSize.x, u_UniformBuffer.ScreenSize.y));
		lod = mipLevel;

		// Sample the color
		reflectionColor += vec4((ConeSampleWeightedColor(samplePos, mipLevel) * weight[7 - i]).xyz, weight[7 - i]);
		

		if (reflectionColor.a > 1.0)
			break;
		
		/* Calculate next smaller triangle that approximates the cone in screen space */
		adjacentLength = IsoscelesTriangleNextAdjacent(adjacentLength, incircleSize);
	}

	/*
	for(uint i = 0; i < 20; i++)
	{
		float diameter = IsoscelesTriangleBase(dist, coneTheta);
		float inCircleSize = IsoscelesTriangleInRadius(diameter, dist);

        float mipLevel = log2(inCircleSize * max(u_UniformBuffer.ScreenSize.x, u_UniformBuffer.ScreenSize.y));

		vec3 currentPos = startPos + (dir * dist);

		vec4 projectedCoord = vec4(0.0f);
		projectedCoord = u_UniformBuffer.ProjectionMatrix * vec4(currentPos, 1.0f);
		projectedCoord.xy /= projectedCoord.w;
		projectedCoord.xy = projectedCoord.xy * 0.5f + 0.5f;

		if(projectedCoord.x > 1.0f || projectedCoord.x < 0.0f) continue;
		if(projectedCoord.y > 1.0f || projectedCoord.y < 0.0f) continue;


		float currentWeight = glossMult;

		vec3 newColor = (textureLod(u_PrefilteredColorBuffer, projectedCoord.xy, mipLevel).xyz * currentWeight);

		remainingAlpha -= currentWeight;
		if(remainingAlpha < 0.0f)
		{
			newColor *= (1.0f - abs(remainingAlpha));
		}
		color += vec4(newColor, currentWeight);

		if(color.a > 1.0f)
		{
			break;
		}

		glossMult *= gloss;
		dist += diameter * 0.86f;
		lastProjCoord = projectedCoord.xy;
	}
	*/

	// Normalize the values
	reflectionColor.rgb /= reflectionColor.a;

	return vec4(vec3(reflectionColor), lod);
}

vec3 fresnelShlick(in float cosTheta, in vec3 F0)
{
	return F0 + (1.0f - F0) * pow(1.0f - cosTheta, 5.0f);
}

void main()
{
	s_UV = vec2(gl_GlobalInvocationID.xy) / u_UniformBuffer.ScreenSize.xy;
	ivec2 pixelCoord = ivec2(gl_GlobalInvocationID.xy);

	// Sample information from the gbuffer
	vec3 viewPos = texture(u_ViewPosTex, s_UV).xyz;
	vec3 currentColor = texture(u_ColorFrameTex, s_UV).rgb;
	float metallic = texture(u_NormalTex, s_UV).z;
	float roughness = texture(u_NormalTex, s_UV).w;

	metallic = 1.0f;
	roughness = 0.0f;
	
	if(viewPos.x == 0.0f && viewPos.y == 0.0f && viewPos.z == 0.0f) {
		imageStore(o_FrameTex, pixelCoord, vec4(currentColor, 1.0f));
		return; 
	}

	if(metallic <= 0.1f || roughness > 0.8f)
	{
		imageStore(o_FrameTex, pixelCoord, vec4(currentColor, 1.0f));
		return;
	}


	// Decode normals and calculate normals in view space (from model space)
	vec2 decodedNormals = texture(u_NormalTex, s_UV).xy;
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
	vec3 jitter = mix(vec3(0.0f), vec3(hash33(worldPos)), roughness);
	vec3 reflectionDir = normalize(reflect(normalize(viewPos), normalize(viewNormal)));

	
#if 0
	vec4 finalCoords = RayCast(normalize(jitter + reflectionDir) * max(1.0f, -viewPos.z), hitPos, depth);


	// Fix artifacts
	vec2 dCoords = smoothstep(0.2, 0.6, abs(vec2(0.5f) - finalCoords.xy));
	float screenEdgeFactor = clamp(1.0f - (dCoords.x + dCoords.y), 0.0f, 1.0f);
	float multipler = pow(1.0f,
						  relfectionSpecularFalloffExponent) *
						  screenEdgeFactor *
						  -reflectionDir.z;


	vec3 resultingColor = texture(u_ColorFrameTex, finalCoords.xy).rgb * clamp(multipler, 0.0f, 0.9f) * fresnel;
#endif

	
#if 1
	// Firstly we ray trace to find the intersection point
	vec4 hitCoords = RayCast(normalize(jitter + reflectionDir) * max(1.0f, -viewPos.z), hitPos, depth);



	// 0.2 = 22.6 degrees, 0.1 = 11.4 degrees, 0.07 = 8 degrees angle
	vec2 lastProjCoord;
	vec4 coneTracedColor = ConeTrace(normalize(reflectionDir) * max(0.1f, -viewPos.z), 0.030f, s_UV, hitCoords.xy, lastProjCoord, 0.1f);



	// Fix artifacts
	vec2 dCoords = smoothstep(0.2, 0.6, abs(vec2(0.5f) - lastProjCoord.xy));
	float screenEdgeFactor = clamp(1.0f - (dCoords.x + dCoords.y), 0.0f, 1.0f);
	float multipler = pow(1.0f,
						  relfectionSpecularFalloffExponent) *
						  screenEdgeFactor *
						  -reflectionDir.z;

	vec3 resultingColor = coneTracedColor.xyz * clamp(multipler, 0.0f, 0.9f);
#endif

	// Calculate the final result
	vec3 finalColor = mix(currentColor, resultingColor.xyz, 0.4f);

	// Store the values
	imageStore(o_FrameTex, pixelCoord, vec4(vec3(finalColor), 1.0f));
}