#type raygen
#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable

struct rayInfo
{
    vec3 color;
};
layout(location = 0) rayPayloadEXT rayInfo prd;

layout(binding = 2, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 0, set = 0, rgba16f) uniform image2D image;


layout(binding = 1, set = 0) uniform CameraProperties
{
    //mat4 model;
    //mat4 view;
    //mat4 proj;

    mat4 viewInverse;
    mat4 projInverse;
} cam;

void main()
{
    const vec2 pixelCenter = vec2(gl_LaunchIDEXT.xy) + vec2(0.5);
    const vec2 inUV        = pixelCenter / vec2(gl_LaunchSizeEXT.xy);
    vec2       d           = inUV * 2.0 - 1.0;
    
    vec4 origin    = cam.viewInverse * vec4(0, 0, 0, 1);
    vec4 target    = cam.projInverse * vec4(d.x, d.y, 1, 1);
    vec4 direction = cam.viewInverse * vec4(normalize(target.xyz), 0);
    
    uint  rayFlags = gl_RayFlagsOpaqueEXT;
    float tMin     = 0.001;
    float tMax     = 1000.0;
    

    //prd.hitValue = vec3(0.5f, 0.5f, 0.5f);

    traceRayEXT(topLevelAS,     // acceleration structure
                rayFlags,       // rayFlags
                0xFF,           // cullMask
                0,              // sbtRecordOffset
                0,              // sbtRecordStride
                0,              // missIndex
                origin.xyz,     // ray origin
                tMin,           // ray min range
                direction.xyz,  // ray direction
                tMax,           // ray max range
                0               // payload (location = 0)
    );
    
    imageStore(image, ivec2(gl_LaunchIDEXT.xy), vec4(prd.color, 1.0));
}

#type miss
#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable


struct rayInfo
{
    vec3 color;
};
layout(location = 0) rayPayloadInEXT rayInfo prd;


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
    prd.color = vec3(0.9f);
    //prd = vec3(0.1f, 0.1f, 0.1f);

}


#type closesthit
#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable

hitAttributeEXT vec2 attribs;

struct rayInfo
{
    vec3 color;
};
layout(location = 0) rayPayloadInEXT rayInfo prd;

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
	prd.color = vec3(0.1f, 0.1f, 0.2f);
}