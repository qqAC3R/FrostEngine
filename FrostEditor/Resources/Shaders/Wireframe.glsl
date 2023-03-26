#type vertex
#version 450

layout(location = 0) in vec3  a_Position;
layout(location = 1) in vec2  a_TexCoord;
layout(location = 2) in vec3  a_Normal;
layout(location = 3) in vec3  a_Tangent;
layout(location = 4) in vec3  a_Bitangent;
layout(location = 5) in float a_MaterialIndex;

layout(push_constant) uniform Constants
{
	mat4 TransformMatrix;
	vec4 Color;
} u_PushConstant;

void main()
{
	gl_Position = u_PushConstant.TransformMatrix * vec4(a_Position, 1.0);
}

#type fragment
#version 450

layout(location = 0) out vec4 o_Color;

layout(push_constant) uniform Constants
{
	mat4 TransformMatrix;
	vec4 Color;
} u_PushConstant;

void main()
{
	o_Color = u_PushConstant.Color;
}