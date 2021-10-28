#type raygen
// -------------------------------------------------- RAY GEN SHADER ----------------------------------------------
#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_shader_clock : enable

// RayPayLoad
struct RaySpec
{
    vec3 HitValue;

    uint RayDepth;
    vec3 RayOrigin;
    vec3 RayDirection;
    vec3 RayWeight;

    uint Seed;
    vec3 DebugColor;
};
layout(location = 0) rayPayloadEXT RaySpec g_RaySpec;

layout(binding = 2, set = 0) uniform accelerationStructureEXT u_TopLevelAS;
layout(binding = 0, set = 0, rgba16f) uniform image2D u_Image;

layout(push_constant) uniform Constants
{
    mat4 ViewInverse;
    mat4 ProjInverse;
    vec3 CameraPosition;
    int  RayAccumulation;
} ps_Camera;

// Generate a random unsigned int from two unsigned int values, using 16 pairs
// of rounds of the Tiny Encryption Algorithm. See Zafar, Olano, and Curtis,
// "GPU Random Numbers via the Tiny Encryption Algorithm"
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

void main()
{
    uint seed = tea(gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x, int(clockARB()));

    const vec2 pixelCenter = vec2(gl_LaunchIDEXT.xy) + vec2(0.5);
    const vec2 inUV        = pixelCenter / vec2(gl_LaunchSizeEXT.xy);
    vec2       d           = inUV * 2.0 - 1.0;
    
    vec4 origin    = ps_Camera.ViewInverse * vec4(0, 0, 0, 1);
    vec4 target    = ps_Camera.ProjInverse * vec4(d.x, d.y, 1, 1);
    vec4 direction = ps_Camera.ViewInverse * vec4(normalize(target.xyz), 0);
    
    uint  rayFlags = gl_RayFlagsOpaqueEXT;
    float tMin     = 0.001;
    float tMax     = 1000.0;
    

    g_RaySpec.HitValue      = vec3(0.0f);
    g_RaySpec.Seed          = seed;
    g_RaySpec.RayDepth      = 0;
    g_RaySpec.RayOrigin     = origin.xyz;
    g_RaySpec.RayDirection  = direction.xyz;
    g_RaySpec.RayWeight     = vec3(0.0f);


    vec3 curWeight = vec3(1);
    vec3 hitValue  = vec3(0);

    for(; g_RaySpec.RayDepth < 4; g_RaySpec.RayDepth++)
    {
        traceRayEXT(u_TopLevelAS,             // acceleration structure
                    rayFlags,                 // rayFlags
                    0xFF,                     // cullMask
                    0,                        // sbtRecordOffset
                    0,                        // sbtRecordStride
                    0,                        // missIndex
                    g_RaySpec.RayOrigin,     // ray origin
                    tMin,                     // ray min range
                    g_RaySpec.RayDirection,  // ray direction
                    tMax,                     // ray max range
                    0                         // payload (location = 0)
        );
        hitValue += g_RaySpec.HitValue * curWeight;
        curWeight *= g_RaySpec.RayWeight;
    }
    
    // Do accumulation over time
    //if(ps_Camera.rayAccumulation > 0)
    if(false)
    {
        float a         = 1.0f / float(ps_Camera.RayAccumulation + 1);
        vec3  old_color = imageLoad(u_Image, ivec2(gl_LaunchIDEXT.xy)).xyz;
        imageStore(u_Image, ivec2(gl_LaunchIDEXT.xy), vec4(mix(old_color, hitValue, a), 1.0f));
    }
    else
    {
        imageStore(u_Image, ivec2(gl_LaunchIDEXT.xy), vec4(hitValue, 1.0f));
    }

}

#type miss
// -------------------------------------------------- RAY MISS SHADER ----------------------------------------------
#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable


// RayPayLoad
struct RaySpec
{
    vec3 HitValue;

    uint RayDepth;
    vec3 RayOrigin;
    vec3 RayDirection;
    vec3 RayWeight;

    uint Seed;
    vec3 DebugColor;
};
layout(location = 0) rayPayloadInEXT RaySpec g_RaySpec;

layout(binding = 6, set = 0) uniform samplerCube u_CubeMapSky;

void main()
{
    vec3 color = texture(u_CubeMapSky, g_RaySpec.RayDirection).rgb;
    g_RaySpec.HitValue = color;
    g_RaySpec.DebugColor = color;
    g_RaySpec.RayDepth = 100;
}

#type closesthit
// -------------------------------------------------- CLOSEST HIT SHADER ----------------------------------------------
#version 460
#extension GL_EXT_ray_tracing : require
//#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_ARB_shader_clock : enable

hitAttributeEXT vec2 attribs;

uint g_PrimitiveID; // Global variable

// RayPayLoad
struct RaySpec
{
    vec3 HitValue;

    uint RayDepth;
    vec3 RayOrigin;
    vec3 RayDirection;
    vec3 RayWeight;

    uint Seed;
    vec3 DebugColor;
};
layout(location = 0) rayPayloadInEXT RaySpec g_RaySpec;

layout(push_constant) uniform Constants
{
    mat4 viewInverse;
    mat4 projInverse;
    vec3 cameraPosition;
    int  rayAccumulation;
} ps_Camera;

struct Vertex
{
    vec3 Position;
    vec2 TexCoord;
    vec3 Normal;
    vec3 Tangent;
    vec3 Bitangent;
    float MaterialIndex;
};

struct InstanceInfo
{
    mat4 Transform;
    mat4 InverseTransform;

    vec3 Albedo;
    vec3 Emittance;
    float Roughness;
    float RefractionIndex;
};

layout(buffer_reference, scalar) buffer Vertices { Vertex v[]; }; // Positions of an object
layout(buffer_reference, scalar) buffer Indices { ivec3 i[]; }; // Triangle indices

layout(set = 0, binding = 3, scalar) readonly buffer VertexPointers { uint64_t vertexPointer[1000]; } u_VertexPointers;
layout(set = 0, binding = 4, scalar) readonly buffer IndexPointers { uint64_t indexPointer[1000]; } u_IndexPointers;
layout(set = 0, binding = 5, scalar) readonly buffer TransformInstancePointers { InstanceInfo mat[1000];  } u_TransformPointers;
layout(set = 0, binding = 7) readonly buffer GeometrySubmeshOffsets { uint offsets[10000];  } u_GeometrySubmeshOffsets;
layout(set = 0, binding = 8) readonly buffer GeometrySubmeshCount { uint offsets[1000];  } u_GeometrySubmeshCount;


#define M_PI 3.141592
#define TWO_PI 6.28318530717958648

// Generate a random unsigned int in [0, 2^24) given the previous RNG state
// using the Numerical Recipes linear congruential generator
uint LCG(inout uint prev)
{
    uint LCG_A = 1664525u;
    uint LCG_C = 1013904223u;
    prev       = (LCG_A * prev + LCG_C);
    return prev & 0x00FFFFFF;
}

// Generate a random float in [0, 1) given the previous RNG state
float RND(inout uint prev)
{
    return (float(LCG(prev)) / float(0x01000000));
}

// Calculate the PrimitiveIndex for getting the indices from the indexbuffer (very complicated procress)
void InitPrimitiveID()
{
    uint instanceMaxIndex = u_GeometrySubmeshCount.offsets[gl_InstanceCustomIndexEXT];
    uint instanceIndex = u_GeometrySubmeshOffsets.offsets[(instanceMaxIndex + gl_GeometryIndexEXT)];
    g_PrimitiveID = instanceIndex + gl_PrimitiveID;
}

// Randomly sampling around +Z
vec3 SamplingHemisphere(inout uint seed, in vec3 x, in vec3 y, in vec3 z)
{
    float r1 = RND(seed);
    float r2 = RND(seed);
    float sq = sqrt(1.0 - r2);

    vec3 direction = vec3(cos(2 * M_PI * r1) * sq, sin(2 * M_PI * r1) * sq, sqrt(r2));
    direction      = direction.x * x + direction.y * y + direction.z * z;

    return direction;
}

void ComputeTBN(in Vertices vertices, in Indices indices, out vec3 N, out vec3 T, out vec3 B)
{
    // Indices of the triangle
    ivec3 index = indices.i[g_PrimitiveID];

    Vertex v0 = vertices.v[index.x];
    Vertex v1 = vertices.v[index.y];
    Vertex v2 = vertices.v[index.z];

    const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    // Computing the normal at hit position
    vec3 normal = v0.Normal * barycentrics.x + v1.Normal * barycentrics.y + v2.Normal * barycentrics.z;
    // Transforming the normal to world space
    normal = normalize(vec3(u_TransformPointers.mat[gl_InstanceCustomIndexEXT].InverseTransform * vec4(normal, 0.0)));
    N = normal;

    if(abs(N.x) > abs(N.y))
        T = vec3(N.z, 0, -N.x) / sqrt(N.x * N.x + N.z * N.z);
    else
        T = vec3(0, -N.z, N.y) / sqrt(N.y * N.y + N.z * N.z);
    B = cross(N, T);
}

void ComputeWorldPos(in Vertices vertices, in Indices indices, out vec3 worldPos)
{
    // Indices of the triangle
    ivec3 index = indices.i[g_PrimitiveID];

    Vertex v0 = vertices.v[index.x];
    Vertex v1 = vertices.v[index.y];
    Vertex v2 = vertices.v[index.z];

    const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

    // Computing the coordinates of the hit position
    worldPos = v0.Position * barycentrics.x + v1.Position * barycentrics.y + v2.Position * barycentrics.z;

    // Transforming the position to world space
    worldPos = vec3(u_TransformPointers.mat[gl_InstanceCustomIndexEXT].Transform * vec4(worldPos, 1.0));
}

vec3 Refract(vec3 normal, vec3 worldPos, vec3 rayDirection, float refractionIndex)
{
    const float NdotD = dot(g_RaySpec.RayDirection, normal);

    vec3 refrNormal = normal;
    float refrEta;

    if(NdotD > 0.0f)
    {
        refrNormal = -normal;
        refrEta = 1.0f / refractionIndex;
    }
    else {
        refrNormal = normal;
        refrEta = refractionIndex;
    }

    worldPos = worldPos + g_RaySpec.RayDirection * 0.001f;
    return refract(g_RaySpec.RayDirection, refrNormal, refrEta);
}

void main()
{
    // Initializing the Primitive Index
    InitPrimitiveID();

    // Getting the buffers
    Vertices vertices = Vertices(u_VertexPointers.vertexPointer[gl_InstanceCustomIndexEXT]);
    Indices indices = Indices(u_IndexPointers.indexPointer[gl_InstanceCustomIndexEXT]);
    InstanceInfo instanceInfo = u_TransformPointers.mat[gl_InstanceCustomIndexEXT];

    // Indices of the triangle
    ivec3 index = indices.i[g_PrimitiveID];

    // Getting the verticies
    Vertex v0 = vertices.v[index.x];
    Vertex v1 = vertices.v[index.y];
    Vertex v2 = vertices.v[index.z];


    // Compute the normals
    vec3 normal, tangent, bitangent;
  	ComputeTBN(vertices, indices, normal, tangent, bitangent);

    // Compute the world pos
    vec3 worldPos;
    ComputeWorldPos(vertices, indices, worldPos);

    // Setting up the material information
    vec3 albedo = instanceInfo.Albedo;
    vec3 emittance = instanceInfo.Emittance;
    float refractionIndex = instanceInfo.RefractionIndex;
    float roughness = instanceInfo.Roughness;

    // Sampling a random direction from the hemisphere (for path tracing)
    vec3 rayDirection = normalize(SamplingHemisphere(g_RaySpec.Seed, tangent, bitangent, normal));

    // If the roughness is less than 1.0f, then reflect the ray with an offset (dependent of the roughness factor)
    if(roughness < 1.0f)
    {
        vec3 reflectedVector = normalize(reflect(g_RaySpec.RayDirection, normal));
        rayDirection = normalize((reflectedVector + rayDirection * roughness));
    }

    // If the reflection index is not 1.0f (which is air) we should refract the ray
    if(refractionIndex != 1.0f)
    {
        rayDirection = normalize(Refract(normal, worldPos, rayDirection, refractionIndex));
    }

   
    // Probability of the newRay (cosine distributed)
  	const float p = 1 / M_PI;

	// Compute the BRDF for this ray (assuming Lambertian reflection)
  	float cos_theta = dot(rayDirection, normal);
    
    // Calculating the final brdf
    vec3 BRDF = (albedo / M_PI);

    // Setting up the RaySpec for the next bounce
    g_RaySpec.RayOrigin    = worldPos;
  	g_RaySpec.RayDirection = rayDirection;
  	g_RaySpec.HitValue     = emittance;
  	g_RaySpec.RayWeight    = BRDF * cos_theta / p;
  	g_RaySpec.DebugColor   = normal;
}