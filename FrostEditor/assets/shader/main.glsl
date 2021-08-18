#type vertex
#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;

layout(location = 0) out vec2 fragTexCoord;

void main()
{
    fragTexCoord = a_TexCoord;
    
    gl_Position = vec4(a_Position, 1.0);
}

#type fragment
#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 outColor;

layout(binding = 0, set = 0) uniform sampler2D u_FullScreenTexture;

layout(push_constant) uniform Constants
{
    vec3 color;
} pushC;
layout(location = 0) in vec2 fragTexCoord;

void main()
{
    float gamma = 1.0f / 2.2f;
    outColor = pow(texture(u_FullScreenTexture, fragTexCoord).rgba, vec4(gamma));
}