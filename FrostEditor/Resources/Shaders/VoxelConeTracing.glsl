#type compute
#version 460

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(binding = 0) uniform sampler3D u_VoxelTexture;

//layout(binding = 1) uniform sampler2D u_PositionTexture;
layout(binding = 1) uniform sampler2D u_DepthTexture;
layout(binding = 2) uniform sampler2D u_NormalTexture;

layout(binding = 3, rgba16f) uniform writeonly image2D u_IndirectDiffuseTexture;
layout(binding = 4, rgba16f) uniform writeonly image2D u_IndirectSpecularTexture;

layout(push_constant) uniform PushConstant
{
	mat4 InvViewProjMatrix;

	vec3 VoxelSampleOffset;
	float VoxelGrid;

	vec3 CameraPosition;
	float VoxelTextureSize;

	int UseIndirectDiffuse;
	int UseIndirectSpecular;

	float ConeTraceMaxDistance;
	int ConeTraceMaxSteps;
} u_PushConstant;

// Unroll all loops for performance - this is important
#pragma optionNV(unroll all)

// =======================================================
// Constants
const float PI = 3.141592;
// =======================================================
vec4 SampleVoxels(vec3 worldPosition, float lod)
{
	worldPosition = worldPosition - u_PushConstant.VoxelSampleOffset;

	float VoxelDimensions = u_PushConstant.VoxelTextureSize;
	float VoxelGridWorldSize = u_PushConstant.VoxelGrid;

    vec3 offset = vec3(0, 1.0 / VoxelDimensions, 0); // Why??
    vec3 voxelTextureUV = worldPosition / (VoxelGridWorldSize * 0.5);
    voxelTextureUV = voxelTextureUV * 0.5 + 0.5 + offset;

    if(voxelTextureUV.x > 1.0 || voxelTextureUV.x < 0.0) return vec4(vec3(0.0), -1.0);
    if(voxelTextureUV.y > 1.0 || voxelTextureUV.y < 0.0) return vec4(vec3(0.0), -1.0);
    if(voxelTextureUV.z > 1.0 || voxelTextureUV.z < 0.0) return vec4(vec3(0.0), -1.0);

    lod = clamp(lod, 0.0f, 5.0f);

	return textureLod(u_VoxelTexture, voxelTextureUV, lod);
}

vec4 ConeTrace(vec3 worldPos, vec3 normal, vec3 direction, float tanHalfAngle, out float occlusion)
{
#define ALPHA_THRESH 0.90

	float lod = 0.0;
    vec3 color = vec3(0);
    float alpha = 0.0;
    occlusion = 0.0;
	uint steps = 0;

	float VoxelDimensions = u_PushConstant.VoxelTextureSize;
	float VoxelGridWorldSize = u_PushConstant.VoxelGrid;
	
	float voxelWorldSize = VoxelGridWorldSize / VoxelDimensions;
	float dist = voxelWorldSize; // Start one voxel away to avoid self occlusion
	vec3 startPos = worldPos + normal * (1.0f * voxelWorldSize); // Plus move away slightly in the normal direction to avoid
														         // self occlusion in flat surfaces

	while(dist < u_PushConstant.ConeTraceMaxDistance && alpha < ALPHA_THRESH && steps < u_PushConstant.ConeTraceMaxSteps)
	{
		// Calculate cone position
		vec3 conePosition = startPos + dist * direction;

		// Cone expansion and respective mip level based on diameter
		float diameter = 2.0 * tanHalfAngle * dist;
		float lodLevel = log2(diameter / voxelWorldSize);

		// Sample the voxel texture
		vec4 voxelColor = SampleVoxels(conePosition, lodLevel);

		// If the point is outside the voxel grid, discard it
		if(voxelColor.a == -1.0f)
		{
			if(steps == 0)
			{
				return vec4(vec3(1.0f), -1.0f);
			}
			else
			{
				break;
			}
		}

		// Front-to-back compositing
        float a = (1.0 - alpha);
		color += a * voxelColor.rgb;
        alpha += a * voxelColor.a;
        
		// Calculate occlusion
		occlusion += (a * voxelColor.a) / (1.0 + 0.03 * diameter);

        dist += diameter;
        //dist += diameter * 0.5f; // smoother

		steps++;
	}

	return vec4(color, alpha);
}


const int NUM_CONES = 6;
const vec3 diffuseConeDirections[] =
{
    vec3(0.0f, 1.0f, 0.0f),
    vec3(0.0f, 0.5f, 0.866025f),
    vec3(0.823639f, 0.5f, 0.267617f),
    vec3(0.509037f, 0.5f, -0.7006629f),
    vec3(-0.50937f, 0.5f, -0.7006629f),
    vec3(-0.823639f, 0.5f, 0.267617f)
};

const float diffuseConeWeights[] =
{
    PI / 4.0,
    3.0 * PI / 20.0,
    3.0 * PI / 20.0,
    3.0 * PI / 20.0,
    3.0 * PI / 20.0,
    3.0 * PI / 20.0,
};

vec3 ComputeIndirectDiffuse(vec3 worldPos, vec3 normal, out float occlusion_out)
{
    vec4 color = vec4(0);
    occlusion_out = 0.0;

    for(int i = 0; i < NUM_CONES; i++) {
        float occlusion = 0.0;
        // 60 degree cones -> tan(30) = 0.577
        // 90 degree cones -> tan(45) = 1.0

		vec4 coneTracedColor = ConeTrace(worldPos, normal, normal * diffuseConeDirections[i], 0.577, occlusion);

		if(coneTracedColor.a == -1.0f)
			return vec3(1.0f);

        color += diffuseConeWeights[i] * coneTracedColor;
        occlusion_out += diffuseConeWeights[i] * occlusion;
    }

    occlusion_out = 1.0 - occlusion_out;

    return vec3(color);
}


// http://simonstechblog.blogspot.com/2011/12/microfacet-brdf.html
// Temporay use Beckmann Distribution to convert, this may change later 
float RoughnessToSpecularPower(float roughness)
{
#define MAX_SPEC_POWER 1048.0f
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

vec3 ComputeIndirectSpecular(vec3 worldPos, vec3 normal, float roughness, out float occlusion_out)
{
	roughness = clamp(roughness, 0.05f, 1.0f);
	float specularPower = RoughnessToSpecularPower(roughness);
	float coneTheta = SpecularPowerToConeAngle(specularPower);
	float tanConeHalfTheta = tan(coneTheta * 0.5);
	
	vec3 E = normalize(vec3(u_PushConstant.CameraPosition) - worldPos);
	vec3 N = normal;
	vec3 R = normalize(reflect(-E, N));
	
	// 0.2 = 22.6 degrees, 0.1 = 11.4 degrees, 0.07 = 8 degrees angle
	vec4 indirectSpecular = ConeTrace(worldPos, normal, R, tanConeHalfTheta, occlusion_out);
	
	//if(indirectSpecular.a == -1.0f)
	//	return vec3(1.0f);

	return vec3(indirectSpecular);
}

float FadeOutVoxels(vec3 worldPosition, float fadeBounds)
{
	worldPosition = worldPosition - u_PushConstant.VoxelSampleOffset;

	float VoxelDimensions = u_PushConstant.VoxelTextureSize;
	float VoxelGridWorldSize = u_PushConstant.VoxelGrid;

    vec3 voxelTextureUV = worldPosition / (VoxelGridWorldSize * 0.5);
    voxelTextureUV = voxelTextureUV * 0.5 + 0.5;
	
	float sum = 0.0f;
	float nrSamples = 0.0f;

	if(voxelTextureUV.x < 1.0 && voxelTextureUV.x >= (1.0 - fadeBounds)) { sum += (1.0 - ((1.0 - voxelTextureUV.x) / fadeBounds) ); nrSamples++; }
    if(voxelTextureUV.y < 1.0 && voxelTextureUV.y >= (1.0 - fadeBounds)) { sum += (1.0 - ((1.0 - voxelTextureUV.y) / fadeBounds) ); nrSamples++; }
    if(voxelTextureUV.z < 1.0 && voxelTextureUV.z >= (1.0 - fadeBounds)) { sum += (1.0 - ((1.0 - voxelTextureUV.z) / fadeBounds) ); nrSamples++; }
	
	if(voxelTextureUV.x >= 0.0 && voxelTextureUV.x < fadeBounds) { sum += (1.0 - (voxelTextureUV.x / fadeBounds) ); nrSamples++; }
    if(voxelTextureUV.y >= 0.0 && voxelTextureUV.y < fadeBounds) { sum += (1.0 - (voxelTextureUV.y / fadeBounds) ); nrSamples++; }
    if(voxelTextureUV.z >= 0.0 && voxelTextureUV.z < fadeBounds) { sum += (1.0 - (voxelTextureUV.z / fadeBounds) ); nrSamples++; }

	if(nrSamples == 0.0f)
		return 0.0f;

	return (sum / nrSamples);
}
// =======================================================


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
// Decodes the worldspace position of the fragment from depth.
vec3 DecodePosition(ivec2 coords)
{
	vec2 texCoords = (vec2(coords) + vec2(0.5f)) / vec2(imageSize(u_IndirectDiffuseTexture).xy);
	float depth = texelFetch(u_DepthTexture, coords, 0).r;
	vec3 clipCoords = vec3(texCoords * 2.0 - 1.0, depth);
	
	vec4 temp = u_PushConstant.InvViewProjMatrix * vec4(clipCoords, 1.0);
	return temp.xyz / temp.w;
}

void main()
{
	ivec2 globalInvocation = ivec2(gl_GlobalInvocationID.xy);
	vec2 uv = (vec2(globalInvocation) + 0.5.xx) / vec2(imageSize(u_IndirectDiffuseTexture).xy);


	// World Pos
	vec3 worldPos = DecodePosition(globalInvocation);

	// Decode normals
	vec2 encodedNormal =   texture(u_NormalTexture, uv).rg;
    vec3 normal =          DecodeNormal(encodedNormal);
	
	// PBR Values
	float roughness = texture(u_NormalTexture, uv).a;

	
	// Voxel Cone Tracing
	float voxelTraceOcclusion;
	vec3 s_IndirectDiffuse = vec3(1.0f);
	vec3 s_IndirectSpecular = vec3(1.0f);

	float fadeVoxelFactor = FadeOutVoxels(worldPos, 0.15);
	s_IndirectDiffuse  = u_PushConstant.UseIndirectDiffuse == 1 ?
							ComputeIndirectDiffuse(worldPos, normal, voxelTraceOcclusion).xyz :
							vec3(1.0);

	s_IndirectSpecular = u_PushConstant.UseIndirectSpecular == 1 ?
							ComputeIndirectSpecular(worldPos, normal, roughness, voxelTraceOcclusion) :
							vec3(1.0);

	s_IndirectDiffuse = mix(s_IndirectDiffuse, vec3(1.0f), fadeVoxelFactor);
	s_IndirectSpecular = mix(s_IndirectSpecular, vec3(1.0f), fadeVoxelFactor);
	
	imageStore(u_IndirectDiffuseTexture, globalInvocation, vec4(s_IndirectDiffuse, 1.0));
	imageStore(u_IndirectSpecularTexture, globalInvocation, vec4(s_IndirectSpecular, 1.0));
}