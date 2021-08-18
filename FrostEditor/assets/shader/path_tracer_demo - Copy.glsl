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
};


layout(location = 0) rayPayloadEXT hitPayload prd;

layout(binding = 2, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 0, set = 0, rgba32f) uniform image2D image;

layout(binding = 1, set = 0) uniform CameraProperties
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
    //uint seed = tea(gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x, int(clockARB()));

    const vec2 pixelCenter = vec2(gl_LaunchIDEXT.xy) + vec2(0.5);
    const vec2 inUV = pixelCenter/vec2(gl_LaunchSizeEXT.xy);
    vec2 d = inUV * 2.0 - 1.0;
    
    vec4 origin    = cam.viewInverse * vec4(0, 0, 0, 1);
    vec4 target    = cam.projInverse * vec4(d.x, d.y, 1, 1);
    vec4 direction = cam.viewInverse * vec4(normalize(target.xyz), 0);
    
    uint  rayFlags = gl_RayFlagsOpaqueEXT;
    float tMin     = 0.001;
    float tMax     = 10000.0;
    
    prd.hitValue = vec3(pushC.clearColor);
    
	traceRayEXT(topLevelAS,        // acceleration structure
                rayFlags,          // rayFlags
                0xFF,              // cullMask
                0,                 // sbtRecordOffset
                0,                 // sbtRecordStride
                0,                 // missIndex
                origin.xyz,     // ray origin
                tMin,              // ray min range
                direction.xyz,  // ray direction
                tMax,              // ray max range
                0                  // payload (location = 0)
    );



    imageStore(image, ivec2(gl_LaunchIDEXT.xy), vec4(prd.hitValue, 1.0));
    
}

#type closesthit
#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable



hitAttributeEXT vec2 attribs;

struct hitPayload
{
    vec3 hitValue;
};
layout(location = 0) rayPayloadInEXT hitPayload prd;




layout(binding = 2, set = 0) uniform accelerationStructureEXT topLevelAS;
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
	prd.hitValue = vec3(0.1f);
}

#type miss
#version 460
#extension GL_EXT_ray_tracing : require

struct hitPayload
{
    vec3 hitValue;
};
layout(location = 0) rayPayloadEXT hitPayload prd;


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
    //prd.hitValue = pushC.clearColor.xyz;
    prd.hitValue = vec3(1.0f);
}