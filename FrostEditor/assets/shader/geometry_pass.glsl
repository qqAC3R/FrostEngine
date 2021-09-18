#type vertex
#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;
layout(location = 2) in vec3 a_Normal;
layout(location = 3) in vec3 a_Tangent;
layout(location = 4) in vec3 a_Bitangent;

layout(location = 0) out vec3 v_WorldPos;
layout(location = 1) out vec3 v_Normal;
layout(location = 2) out vec3 v_FragmentPos;

layout(push_constant) uniform Constants
{
    mat4 TransformMatrix;
    mat4 ModelMatrix;
} pushC;

void main()
{
    vec4 worldPos = pushC.TransformMatrix * vec4(a_Position, 1.0);
    v_WorldPos = worldPos.xyz;

    mat3 normalMatrix = transpose(inverse(mat3(pushC.ModelMatrix)));
    v_Normal = normalMatrix * a_Normal;

    v_FragmentPos = vec3(pushC.ModelMatrix * vec4(a_Position, 1.0));
    
    gl_Position = worldPos;
}

#type fragment
#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 v_WorldPos;
layout(location = 1) in vec3 v_Normal;
layout(location = 2) in vec3 v_FragmentPos;

void main()
{
    vec3 lightPosition = vec3(50.0f, 20.0f, 50.0f);
    vec3 N = normalize(v_Normal);
    vec3 L = normalize(lightPosition - v_FragmentPos);
    float dotNL = max(dot(N, L), 0.0f);

    outColor = vec4(vec3(dotNL), 1.0f);
}