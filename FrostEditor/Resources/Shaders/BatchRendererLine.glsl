#type vertex
#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;

layout(push_constant) uniform PushConstant
{
	mat4 ViewProjectionMatrix;
} u_PushConstant;

layout(location = 0) out vec4 v_Color;

void main()
{
	v_Color = a_Color;

	gl_Position = u_PushConstant.ViewProjectionMatrix * vec4(a_Position, 1.0f);
}

#type fragment
#version 460

layout(location = 0) out vec4 o_Color;

layout(location = 0) in vec4 v_Color;

void main()
{
	o_Color = v_Color;
}