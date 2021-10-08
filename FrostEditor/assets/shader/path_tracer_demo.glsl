#type raygen
#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_shader_clock : enable

struct rayInfo
{
    vec3 hitValue;

    uint seed;
    uint bounces;

    vec3 rayOrigin;
    vec3 rayDirection;
    vec3 weight;

    vec3 color;
};
layout(location = 0) rayPayloadEXT rayInfo rayPayLoad;

layout(binding = 2, set = 0) uniform accelerationStructureEXT u_TopLevelAS;
layout(binding = 0, set = 0, rgba16f) uniform image2D u_Image;

layout(push_constant) uniform Constants
{
    mat4 viewInverse;
    mat4 projInverse;
    vec3 cameraPosition;
    int  rayAccumulation;
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
    
    vec4 origin    = ps_Camera.viewInverse * vec4(0, 0, 0, 1);
    vec4 target    = ps_Camera.projInverse * vec4(d.x, d.y, 1, 1);
    vec4 direction = ps_Camera.viewInverse * vec4(normalize(target.xyz), 0);
    
    uint  rayFlags = gl_RayFlagsOpaqueEXT;
    float tMin     = 0.001;
    float tMax     = 1000.0;
    

    rayPayLoad.hitValue     = vec3(0);
    rayPayLoad.seed         = seed;
    rayPayLoad.bounces      = 0;
    rayPayLoad.rayOrigin    = origin.xyz;
    rayPayLoad.rayDirection = direction.xyz;
    rayPayLoad.weight       = vec3(0);


    vec3 curWeight = vec3(1);
    vec3 hitValue  = vec3(0);

    for(; rayPayLoad.bounces < 4; rayPayLoad.bounces++)
    {
        traceRayEXT(u_TopLevelAS,     // acceleration structure
                    rayFlags,       // rayFlags
                    0xFF,           // cullMask
                    0,              // sbtRecordOffset
                    0,              // sbtRecordStride
                    0,              // missIndex
                    rayPayLoad.rayOrigin,     // ray origin
                    tMin,           // ray min range
                    rayPayLoad.rayDirection,  // ray direction
                    tMax,           // ray max range
                    0               // payload (location = 0)
        );

        hitValue += rayPayLoad.hitValue * curWeight;
        curWeight *= rayPayLoad.weight;
    }
    
    // Do accumulation over time
    //if(ps_Camera.rayAccumulation > 0)
    if(false)
    {
        float a         = 1.0f / float(ps_Camera.rayAccumulation + 1);
        vec3  old_color = imageLoad(u_Image, ivec2(gl_LaunchIDEXT.xy)).xyz;
        imageStore(u_Image, ivec2(gl_LaunchIDEXT.xy), vec4(mix(old_color, hitValue, a), 1.0f));
    }
    else
    {
        imageStore(u_Image, ivec2(gl_LaunchIDEXT.xy), vec4(hitValue, 1.0f));
    }

}

#type miss
#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable


struct rayInfo
{
    vec3 hitValue;

    uint seed;
    uint bounces;

    vec3 rayOrigin;
    vec3 rayDirection;
    vec3 weight;

    vec3 color;
};
layout(location = 0) rayPayloadInEXT rayInfo rayPayLoad;
layout(binding = 6, set = 0) uniform samplerCube u_CubeMapSky;

void main()
{
    vec3 color = texture(u_CubeMapSky, rayPayLoad.rayDirection).rgb;
    rayPayLoad.hitValue = color;
    rayPayLoad.color = vec3(0.9f);
    rayPayLoad.bounces = 100;
}


#type closesthit
#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_ARB_shader_clock : enable

hitAttributeEXT vec2 attribs;

struct rayInfo
{
    vec3 hitValue;

    uint seed;
    uint bounces;

    vec3 rayOrigin;
    vec3 rayDirection;
    vec3 weight;

    vec3 color;
};
layout(location = 0) rayPayloadInEXT rayInfo rayPayLoad;

struct Vertex
{
    vec3 Position;
    vec2 TexCoord;
    vec3 Normal;
    vec3 Tangent;
    vec3 Bitangent;
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

layout(push_constant) uniform Constants
{
    mat4 viewInverse;
    mat4 projInverse;
    vec3 cameraPosition;
    int  rayAccumulation;
} ps_Camera;

layout(buffer_reference, scalar) buffer Vertices { Vertex v[]; }; // Positions of an object
layout(buffer_reference, scalar) buffer Indices { ivec3 i[]; }; // Triangle indices

layout(binding = 3, set = 0, scalar) readonly buffer VertexPointers { uint64_t vertexPointer[1000]; } u_VertexPointers;
layout(binding = 4, set = 0, scalar) readonly buffer IndexPointers { uint64_t indexPointer[1000]; } u_IndexPointers;
layout(binding = 5, set = 0, scalar) readonly buffer TransformInstancePointers { InstanceInfo mat[1000];  } u_TransformPointers;


#define M_PI 3.141592
#define TWO_PI 6.28318530717958648


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
vec3 SamplingHemisphere(inout uint seed, in vec3 x, in vec3 y, in vec3 z)
{
    float r1 = rnd(seed);
    float r2 = rnd(seed);
    float sq = sqrt(1.0 - r2);

    vec3 direction = vec3(cos(2 * M_PI * r1) * sq, sin(2 * M_PI * r1) * sq, sqrt(r2));
    direction      = direction.x * x + direction.y * y + direction.z * z;

    return direction;
}

//-----------------------------------------------------------------------
//-----------------------------------------------------------------------
vec3 ImportanceSampleGTR2(float rgh, inout uint seed)
{
    float r1 = rnd(seed);
    float r2 = rnd(seed);
    
    float a = max(0.001, rgh);

    float phi = r1 * TWO_PI;

    float cosTheta = sqrt((1.0 - r2) / (1.0 + (a * a - 1.0) * r2));
    float sinTheta = clamp(sqrt(1.0 - (cosTheta * cosTheta)), 0.0, 1.0);
    float sinPhi   = sin(phi);
    float cosPhi   = cos(phi);

    return vec3(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta);
}

// Input Ve: view direction
// Input alpha_x, alpha_y: roughness parameters
// Input U1, U2: uniform random numbers
// Output Ne: normal sampled with PDF D_Ve(Ne) = G1(Ve) * max(0, dot(Ve, Ne)) * D(Ne) / Ve.z
vec3 sampleGGXVNDF(vec3 Ve, float alpha_x, float alpha_y, float U1, float U2)
{
    // Section 3.2: transforming the view direction to the hemisphere configuration
    vec3 Vh = normalize(vec3(alpha_x * Ve.x, alpha_y * Ve.y, Ve.z));
    
    // Section 4.1: orthonormal basis (with special case if cross product is zero)
    float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
    vec3 T1 = lensq > 0 ? vec3(-Vh.y, Vh.x, 0) * inversesqrt(lensq) : vec3(1,0,0);
    vec3 T2 = cross(Vh, T1);
    
    // Section 4.2: parameterization of the projected area
    float r = sqrt(U1);
    float phi = 2.0 * M_PI * U2;
    float t1 = r * cos(phi);
    float t2 = r * sin(phi);
    float s = 0.5 * (1.0 + Vh.z);
    t2 = (1.0 - s)*sqrt(1.0 - t1*t1) + s*t2;
    
    // Section 4.3: reprojection onto hemisphere
    vec3 Nh = t1*T1 + t2*T2 + sqrt(max(0.0, 1.0 - t1*t1 - t2*t2))*Vh;
    
    // Section 3.4: transforming the normal back to the ellipsoid configuration
    vec3 Ne = normalize(vec3(alpha_x * Nh.x, alpha_y * Nh.y, max(0.0, Nh.z)));
    return Ne;
}

void ComputeTBN(in Vertices vertices, in Indices indices, out vec3 N, out vec3 T, out vec3 B)
{
    // Indices of the triangle
    ivec3 index = indices.i[gl_PrimitiveID];

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
    ivec3 index = indices.i[gl_PrimitiveID];

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
    const float NdotD = dot(rayPayLoad.rayDirection, normal);

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

    worldPos = worldPos + rayPayLoad.rayDirection * 0.001f;
    return refract(rayPayLoad.rayDirection, refrNormal, refrEta);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = M_PI * denom * denom;

    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float RadicalInverse_VdC(uint bits) 
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}
// ----------------------------------------------------------------------------
vec2 Hammersley(uint i, uint N)
{
    return vec2(float(i)/float(N), RadicalInverse_VdC(i));
}

vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
{
    float a = roughness * roughness;
	
    float phi = 2.0 * M_PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta*cosTheta);
	
    // from spherical coordinates to cartesian coordinates
    vec3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;
	
    // from tangent-space vector to world-space sample vector
    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);
	
    vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    vec3 test = tangent + bitangent + N;
    return normalize(H);
}  

vec3 Reflect(vec3 normal, vec3 tangent, float roughness)
{
    vec2 Xi = Hammersley(rayPayLoad.seed, 1);
    vec3 H  = ImportanceSampleGGX(Xi, normal, roughness);
    vec3 L  = normalize(2.0 * dot(normal, H) * H - normal);
    return L;
}

void main()
{
    Vertices vertices = Vertices(u_VertexPointers.vertexPointer[gl_InstanceCustomIndexEXT]);
    Indices indices = Indices(u_IndexPointers.indexPointer[gl_InstanceCustomIndexEXT]);
    InstanceInfo instanceInfo = u_TransformPointers.mat[gl_InstanceCustomIndexEXT];

    // Indices of the triangle
    ivec3 index = indices.i[gl_PrimitiveID];

    Vertex v0 = vertices.v[index.x];
    Vertex v1 = vertices.v[index.y];
    Vertex v2 = vertices.v[index.z];


    vec3 normal, tangent, bitangent;
  	ComputeTBN(vertices, indices, normal, tangent, bitangent);

    vec3 worldPos;
    ComputeWorldPos(vertices, indices, worldPos);




    vec3 albedo = instanceInfo.Albedo;
    vec3 emittance = instanceInfo.Emittance;
    float refractionIndex = instanceInfo.RefractionIndex;
    float roughness = instanceInfo.Roughness;
    vec3 lightPosition = vec3(20.0f, 10.0f, 10.0f);

    vec3 rayDirection = SamplingHemisphere(rayPayLoad.seed, tangent, bitangent, normal);

    if(roughness < 1.0f)
    {
        vec3 reflectedVector = reflect(rayPayLoad.rayDirection, normal);
        rayDirection = (reflectedVector + rayDirection * roughness);
        //rayDirection = reflectedVector + (rayDirection * vec3(roughness));

        /*
        float anisotropy = 0.0f;
        // Calculate anisotropic roughness along the tangent and bitangent directions
        float aspect = sqrt(1.0 - anisotropy * 0.9);
        float ax = max(0.001, roughness / aspect);
        float ay = max(0.001, roughness * aspect);

        float r1 = rnd(rayPayLoad.seed);
        float r2 = rnd(rayPayLoad.seed);

        vec3 viewVector = normalize(ps_Camera.cameraPosition - worldPos);

        rayDirection *= sampleGGXVNDF(viewVector, ax, ay, r1, r2);
        */

        /*
        vec3 V = normalize(ps_Camera.cameraPosition - worldPos);
        vec2 Xi = Hammersley(0, 1);
        vec3 H  = ImportanceSampleGGX(Xi, normal, roughness);
        vec3 L  = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(dot(normal, L), 0.0);
        //if(NdotL > 0.0f)
        {
            rayDirection = H;
            //rayDirection += reflectedVector;
            //rayDirection = normalize(rayDirection);
            //rayDirection = SamplingHemisphere(rayPayLoad.seed, tangent, bitangent, normal);
        }
        */
    }
    if(refractionIndex != 1.0f)
    {
        rayDirection = Refract(normal, worldPos, rayDirection, refractionIndex);
    }

   


    //vec3 F0 = vec3(0.04); 
    //F0 = mix(F0, albedo, metallic);


    
    //vec3 V = normalize(ps_Camera.cameraPosition - worldPos);

    //vec3 L = normalize(lightPosition - worldPos);
    //vec3 H = normalize(V + L);


    //float NDF = DistributionGGX(normal, H, roughness);
    //float G = GeometrySmith(normal, V, L, roughness);
    //vec3 F = FresnelSchlick(clamp(dot(H, V), 0.0, 1.0), F0);

    //vec3 numerator = NDF * G * F; 
    //vec3 numerator = F; 
    //float denominator = 4 * max(dot(normal, V), 0.0) * max(dot(normal, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
    //float denominator = 4 * max(dot(normal, V), 0.0);
    //vec3 specular = numerator / denominator;
    
    //vec3 kS = F;
    //vec3 kD = vec3(1.0) - kS;
    //kD *= 1.0 - metallic;
    //float NdotL = max(dot(normal, L), 0.0);
    
    // Probability of the newRay (cosine distributed)
  	const float p = 1 / M_PI;

	// Compute the BRDF for this ray (assuming Lambertian reflection)
  	float cos_theta = dot(rayDirection, normal);
    
    vec3 BRDF = (albedo / M_PI);

    rayPayLoad.rayOrigin    = worldPos;
  	rayPayLoad.rayDirection = rayDirection;
  	rayPayLoad.hitValue     = emittance;
  	rayPayLoad.weight       = BRDF * cos_theta / p;
  	rayPayLoad.color        = normal;
}