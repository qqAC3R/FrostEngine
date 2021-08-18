#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

#include "wavefront.glsl"
#include "sampling.glsl"
#include "payload.glsl"

hitAttributeEXT vec2 attribs;

layout(location = 0) rayPayloadInEXT hitPayload prd;
layout(location = 1) rayPayloadEXT bool isShadowed;

layout(binding = 1, set = 1, scalar) buffer ScnDesc { sceneDesc i[]; } scnDesc;
layout(binding = 2, set = 1, scalar) buffer Vertices { Vertex v[]; } vertices[];
layout(binding = 3, set = 1) buffer Indices { uint i[]; } indices[];


//layout(binding = 4, set = 1, scalar) buffer MatColorBufferObject { WaveFrontMaterial m[]; } materials[];
layout(binding = 4, set = 1, scalar) buffer MatColorBufferObject { WaveFrontMaterial m; } materials[];

layout(binding = 5, set = 1) buffer MatIndexColorBuffer { int i[]; } matIndex[];
layout(binding = 6, set = 1) uniform sampler2D texturesMap[];

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
