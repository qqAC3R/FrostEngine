#type vertex
#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;

layout (location = 0) out vec2 v_TexCoord;

layout(push_constant) uniform Constants
{
	mat4 WorldSpaceMatrix;
	float GridScale;
	float GridSize;
} u_PushConstant;

void main()
{
	gl_Position = u_PushConstant.WorldSpaceMatrix * vec4(a_Position, 1.0);
	v_TexCoord = a_TexCoord;
}

#type fragment
#version 450

layout(location = 0) out vec4 o_Color;

layout(push_constant) uniform Constants
{
	mat4 WorldSpaceMatrix;
	float GridScale;
	float GridSize;
} u_PushConstant;

layout (location = 0) in vec2 v_TexCoord;

float grid(vec2 st, float res)
{
	vec2 grid = fract(st);
	return step(res, grid.x) * step(res, grid.y);
}

void main()
{
	float x = grid(v_TexCoord * u_PushConstant.GridScale, u_PushConstant.GridSize);
	o_Color = vec4(vec3(0.2), 0.5) * (1.0 - x);
	if (o_Color.a == 0.0)
		discard;
}