#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 a_Position;
//layout(location = 1) in vec3 a_Color;
layout(location = 1) in vec2 a_TexCoord;

//layout(location = 0) out vec3 fragColor;
layout(location = 0) out vec2 fragTexCoord;

//layout(binding = 0) uniform UniformBufferObject {
//    mat4 model;
//    mat4 view;
//    mat4 proj;
//
//    mat4 inverseView;
//    mat4 inverseProjection;
//} ubo;

void main() {
    //fragColor = a_Color;
    fragTexCoord = a_TexCoord;
    
    //gl_Position = ubo.proj * ubo.view * ubo.model * vec4(a_Position, 1.0);
    gl_Position = vec4(a_Position, 1.0);
}