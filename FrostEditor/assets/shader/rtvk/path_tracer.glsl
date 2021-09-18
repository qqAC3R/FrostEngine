#type raygen
#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_shader_clock : enable

//#include "assets/shader/rtvk/sampling.glsl"
//#include "assets/shader/rtvk/payload.glsl"

uint tea(uint val0, uint val1)
{
  uint v0 = val0;
  uint v1 = val1;
  uint s0 = 0;

  for(uint n = 0; n < 16; n++)
  {
    s0 += 0x9e3779b9;
    v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
    v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
  }

  return v0;
}

struct hitPayload
{
    vec3 hitValue;
    uint seed;
  	uint depth;

    vec3 rayOrigin;
    vec3 rayDirection;
    vec3 weight;
};


layout(location = 0) rayPayloadEXT hitPayload prd;

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 1, set = 0, rgba32f) uniform image2D image;

layout(binding = 0, set = 1) uniform CameraProperties
{
    mat4 model;
    mat4 view;
    mat4 proj;

    mat4 viewInverse;
    mat4 projInverse;
} cam;

layout(push_constant) uniform Constants
{
	vec4  clearColor;
	vec3  lightPosition;
	float lightIntensity;
	int   lightType;
	int	  accumulation;
    int   bounces;
} pushC;

void main() 
{
    uint seed = tea(gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x, int(clockARB()));

    const vec2 pixelCenter = vec2(gl_LaunchIDEXT.xy) + vec2(0.5);
    const vec2 inUV = pixelCenter/vec2(gl_LaunchSizeEXT.xy);
    vec2 d = inUV * 2.0 - 1.0;
    
    vec4 origin    = cam.viewInverse * vec4(0, 0, 0, 1);
    vec4 target    = cam.projInverse * vec4(d.x, d.y, 1, 1);
    vec4 direction = cam.viewInverse * vec4(normalize(target.xyz), 0);
    
    uint  rayFlags = gl_RayFlagsOpaqueEXT;
    float tMin     = 0.001;
    float tMax     = 10000.0;
    
    prd.hitValue     = vec3(0);
    prd.seed         = seed;
    prd.depth        = 0;
    prd.rayOrigin    = origin.xyz;
    prd.rayDirection = direction.xyz;
    prd.weight       = vec3(0);

    vec3 curWeight = vec3(1);
    vec3 hitValue  = vec3(0);
    for(; prd.depth < pushC.bounces; prd.depth++)
    {
        traceRayEXT(topLevelAS,        // acceleration structure
                    rayFlags,          // rayFlags
                    0xFF,              // cullMask
                    0,                 // sbtRecordOffset
                    0,                 // sbtRecordStride
                    0,                 // missIndex
                    prd.rayOrigin,     // ray origin
                    tMin,              // ray min range
                    prd.rayDirection,  // ray direction
                    tMax,              // ray max range
                    0                  // payload (location = 0)
        );

        hitValue += prd.hitValue * curWeight;
        curWeight *= prd.weight;
    }


    if(pushC.accumulation > 0)
    {
        float a         = 1.0f / float(pushC.accumulation + 1);
        vec3  old_color = imageLoad(image, ivec2(gl_LaunchIDEXT.xy)).xyz;
        imageStore(image, ivec2(gl_LaunchIDEXT.xy), vec4(mix(old_color, hitValue, a), 1.f));
    }
    else
    {
        imageStore(image, ivec2(gl_LaunchIDEXT.xy), vec4(hitValue, 1.0));
    }
    
}

#type closesthit
#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable


struct Vertex
{
  vec3 pos;
  vec3 nrm;
  vec3 color;
  vec2 texCoord;
};

struct WaveFrontMaterial
{
  vec3  ambient;
  vec3  emission;
  vec3  roughness;
  float ior;       // index of refraction

  //float dissolve;  // 1 == opaque; 0 == fully transparent
  int   illum;     // illumination model (see http://www.fileformat.info/format/material/)
  int   textureId;
};

struct sceneDesc
{
  int  objId;
  int  txtOffset;
  mat4 transfo;
  mat4 transfoIT;
};

// Generate a random unsigned int in [0, 2^24) given the previous RNG state
// using the Numerical Recipes linear congruential generator
uint lcg(inout uint prev)
{
  uint LCG_A = 1664525u;
  uint LCG_C = 1013904223u;
  prev       = (LCG_A * prev + LCG_C);
  return prev & 0x00FFFFFF;
}

// Generate a random float in [0, 1) given the previous RNG state
float rnd(inout uint prev)
{
  return (float(lcg(prev)) / float(0x01000000));
}

// Randomly sampling around +Z
vec3 samplingHemisphere(inout uint seed, in vec3 x, in vec3 y, in vec3 z)
{
#define M_PI 3.141592

  float r1 = rnd(seed);
  float r2 = rnd(seed);
  float sq = sqrt(1.0 - r2);

  vec3 direction = vec3(cos(2 * M_PI * r1) * sq, sin(2 * M_PI * r1) * sq, sqrt(r2));
  direction      = direction.x * x + direction.y * y + direction.z * z;

  return direction;
}

// Return the tangent and binormal from the incoming normal
void createCoordinateSystem(in vec3 N, out vec3 Nt, out vec3 Nb)
{
	if(abs(N.x) > abs(N.y))
	  Nt = vec3(N.z, 0, -N.x) / sqrt(N.x * N.x + N.z * N.z);
	else
	  Nt = vec3(0, -N.z, N.y) / sqrt(N.y * N.y + N.z * N.z);
	Nb = cross(N, Nt);
}


struct hitPayload
{
    vec3 hitValue;
    uint seed;
  	uint depth;

    vec3 rayOrigin;
    vec3 rayDirection;
    vec3 weight;
};



hitAttributeEXT vec2 attribs;

layout(location = 0) rayPayloadInEXT hitPayload prd;
layout(location = 1) rayPayloadEXT bool isShadowed;

layout(binding = 0, set = 1, scalar) buffer ScnDesc { sceneDesc i[]; } scnDesc;
layout(binding = 0, set = 2, scalar) buffer Vertices { Vertex v[]; } vertices[];
layout(binding = 0, set = 3) buffer Indices { uint i[]; } indices[];


//layout(binding = 4, set = 1, scalar) buffer MatColorBufferObject { WaveFrontMaterial m[]; } materials[];
layout(binding = 0, set = 4, scalar) buffer MatColorBufferObject { WaveFrontMaterial m; } materials[];

layout(binding = 0, set = 5) buffer MatIndexColorBuffer { int i[]; } matIndex[];
layout(binding = 0, set = 6) uniform sampler2D texturesMap[];

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;

layout(push_constant) uniform Constants
{
	vec4  clearColor;
	vec3  lightPosition;
	float lightIntensity;
	int   lightType;
	int	  accumulation;
	int   bounces;
} pushC;

void main()
{
	// Object of this instance
	uint objId = scnDesc.i[gl_InstanceCustomIndexEXT].objId;

	// Indices of the triangle
	ivec3 ind = ivec3(indices[nonuniformEXT(objId)].i[3 * gl_PrimitiveID + 0],   //
	                  indices[nonuniformEXT(objId)].i[3 * gl_PrimitiveID + 1],   //
	                  indices[nonuniformEXT(objId)].i[3 * gl_PrimitiveID + 2]);  //
	// Vertex of the triangle
	Vertex v0 = vertices[nonuniformEXT(objId)].v[ind.x];
	Vertex v1 = vertices[nonuniformEXT(objId)].v[ind.y];
	Vertex v2 = vertices[nonuniformEXT(objId)].v[ind.z];


	const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
	
	// Computing the normal at hit position
	vec3 normal = v0.nrm * barycentrics.x + v1.nrm * barycentrics.y + v2.nrm * barycentrics.z;
	normal = normalize(vec3(scnDesc.i[gl_InstanceCustomIndexEXT].transfoIT * vec4(normal, 0.0))); 	// Transforming the normal to world space

	// Computing the coordinates of the hit position
	vec3 worldPos = v0.pos * barycentrics.x + v1.pos * barycentrics.y + v2.pos * barycentrics.z;
	worldPos = vec3(scnDesc.i[gl_InstanceCustomIndexEXT].transfo * vec4(worldPos, 1.0));

	// Texcoords
	vec2 texCoord = v0.texCoord * barycentrics.x + v1.texCoord * barycentrics.y + v2.texCoord * barycentrics.z;



	//int matIdx = matIndex[nonuniformEXT(objId)].i[gl_PrimitiveID];
	WaveFrontMaterial mat = materials[nonuniformEXT(objId)].m;
	

	vec3 emittance = mat.emission;

	// Pick a random direction from here and keep going.
	vec3 tangent, bitangent;
  	createCoordinateSystem(normal, tangent, bitangent);
  	vec3 rayOrigin    = worldPos;
  	
	vec3 rayDirection;

	if(mat.illum == 3)
	{
		//vec3 randomDirection = samplingHemisphere(prd.seed, tangent, bitangent, normal);
		//while(dot(randomDirection, prd.rayDirection) > 0.0f)
		//{
		//	randomDirection = samplingHemisphere(prd.seed, tangent, bitangent, normal);
		//
		//}

		rayDirection = reflect(prd.rayDirection, normal) + (mat.roughness * samplingHemisphere(prd.seed, tangent, bitangent, normal));


	}
	else if(mat.illum == 4)
	{
		if(dot(prd.rayDirection, normal) > 0.0f)
		{
			rayDirection = refract(prd.rayDirection, -normal, 1.0f / mat.ior);
		}
		else
		{
			rayDirection = refract(prd.rayDirection, normal, mat.ior);
		}
	}
	else
	{
		rayDirection = samplingHemisphere(prd.seed, tangent, bitangent, normal);
	}




	// Probability of the newRay (cosine distributed)
  	const float p = 1 / M_PI;

	// Compute the BRDF for this ray (assuming Lambertian reflection)
  	float cos_theta = dot(rayDirection, normal);
	
	
	vec3 albedo = vec3(0.5f);

	if(mat.textureId >= 0)
	{
		uint txtId = mat.textureId + scnDesc.i[gl_InstanceCustomIndexEXT].txtOffset;
		albedo *= texture(texturesMap[nonuniformEXT(txtId)], texCoord).xyz;
	}
	else
	{
		albedo *= mat.ambient;
	}



	float lightDistance  = 100000.0;
	float tMin   = 0.001;
    float tMax   = lightDistance;
	uint flags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;

	if(pushC.clearColor != vec4(0.0f))
	{
		isShadowed = true;
		traceRayEXT(topLevelAS,		 // acceleration structure
				flags,				 // rayFlags
				0xFF,				 // cullMask
				0,					 // sbtRecordOffset
				0,					 // sbtRecordStride
				1,					 // missIndex
				rayOrigin,			 // ray origin
				tMin,				 // ray min range
				pushC.lightPosition, // ray direction
				tMax,				 // ray max range
				1					 // payload (location = 1)
		);
	}

	

  	vec3  BRDF      = albedo / M_PI;

	prd.rayOrigin    = rayOrigin;
  	prd.rayDirection = rayDirection;
  	prd.hitValue     = emittance;
  	prd.weight       = BRDF * cos_theta / p;

	if(isShadowed)
	{
		prd.weight *= 0.3f;
	}

}

#type miss
#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec3 hitValue;

layout(push_constant) uniform Constants
{
	vec4  clearColor;
	vec3  lightPosition;
	float lightIntensity;
	int   lightType;
	int	  accumulation;
    int   bounces;
} pushC;


void main()
{
    hitValue = pushC.clearColor.xyz;
}