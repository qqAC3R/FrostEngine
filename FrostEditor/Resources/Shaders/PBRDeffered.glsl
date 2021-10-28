#type vertex
#version 450

layout(location = 0) out vec3 v_Position;
layout(location = 1) out vec2 v_TexCoord;

void main()
{
    // Rendering a fullscreen quad using only 3 vertices
    // Credit: https://wallisc.github.io/rendering/2021/04/18/Fullscreen-Pass.html
    v_TexCoord = vec2(((gl_VertexIndex << 1) & 2), gl_VertexIndex & 2);
    v_Position = vec3((v_TexCoord * vec2(2, -2) + vec2(-1, 1)), 0.0f);

    // Setting up the vertex position in NDC. The y is reverted because the formula from above was made for HLSL
    gl_Position = vec4(v_Position.x, -v_Position.y, 0.0f, 1.0f);
}

#type fragment
#version 450

// Constants
const float PI = 3.141592;
const float Epsilon = 0.00001;

// Constant normal incidence Fresnel factor for all dielectrics.
const vec3 Fdielectric = vec3(0.04);


struct PointLight
{
    vec3 Position;
    vec3 Radiance;
    float Radius;
    float Falloff;
};

// Output color
layout(location = 0) out vec4 o_Color;

// Data from the vertex shader
layout(location = 0) in vec3 v_Position;
layout(location = 1) in vec2 v_TexCoord;

// Data from the material system
layout(binding = 0) uniform sampler2D u_PositionTexture;
layout(binding = 1) uniform sampler2D u_AlbedoTexture;
layout(binding = 2) uniform sampler2D u_NormalTexture;
layout(binding = 3) uniform sampler2D u_CompositeTexture;
layout(binding = 4) uniform LightData {
    uint u_LightCount;
    PointLight u_PointLights[1];
};

layout(push_constant) uniform PushConstant {
    vec3 CameraPosition;
} u_PushConstant;

struct SurfaceProperties
{
    vec3 WorldPos;
    vec3 Normal;
    vec3 ViewVector;
    
    vec3 Albedo;
    float Roughness;
    float Metalness;

    vec3 F0;
} m_Surface;

void main()
{
    // Calculating the normals and worldPos
    m_Surface.WorldPos =    texture(u_PositionTexture, v_TexCoord).rgb;
    m_Surface.Normal =      normalize(texture(u_NormalTexture, v_TexCoord).rgb);
    m_Surface.ViewVector =  normalize(u_PushConstant.CameraPosition - m_Surface.WorldPos);

    // Sampling the textures from gbuffer
    m_Surface.Albedo =      texture(u_AlbedoTexture, v_TexCoord).rgb;
    m_Surface.Metalness =   texture(u_CompositeTexture, v_TexCoord).r;
    m_Surface.Roughness =   texture(u_CompositeTexture, v_TexCoord).g;

    o_Color = vec4(m_Surface.Normal, 1.0f);
}