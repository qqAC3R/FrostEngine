#type vertex
#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in uint a_TexIndex;

layout(push_constant) uniform PushConstant
{
	mat4 ViewProjectionMatrix;
	uint UseAtlas;
} u_PushConstant;

layout(location = 0) out vec4 v_Color;
layout(location = 1) out vec2 v_TexCoord;
layout(location = 2) out flat uint v_TexIndex;

layout(set = 0, binding = 0) uniform UniformBuffer
{
	float Temp;
} u_UniformBuffer;

void main()
{
	v_Color = a_Color;
	v_TexCoord= a_TexCoord;
	v_TexIndex = a_TexIndex;

	gl_Position = u_PushConstant.ViewProjectionMatrix * vec4(a_Position, 1.0f);
}

#type fragment(Frost_Bindless)
#version 460
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable


layout(location = 0) out vec4 o_Color;

layout(location = 0) in vec4 v_Color;
layout(location = 1) in vec2 v_TexCoord;
layout(location = 2) in flat uint v_TexIndex;

// Bindless
layout(set = 1, binding = 0) uniform sampler2D u_Textures[];

layout(push_constant) uniform PushConstant
{
	mat4 ViewProjectionMatrix;
	uint UseAtlas;
} u_PushConstant;

vec4 SampleTexture(uint textureId)
{
	return texture(u_Textures[nonuniformEXT(textureId)], v_TexCoord);
}

float Median(float r, float g, float b)
{
    return max(min(r, g), min(max(r, g), b));
}

float ScreenPxRange()
{
	float pxRange = 2.0f;
    vec2 unitRange = vec2(pxRange)/vec2(textureSize(u_Textures[nonuniformEXT(v_TexIndex)], 0));
    vec2 screenTexSize = vec2(1.0)/fwidth(v_TexCoord);
    return max(0.5*dot(unitRange, screenTexSize), 1.0);
}

void main()
{
	vec4 color = SampleTexture(v_TexIndex);
	float mixOpacity = 1.0;

	if(u_PushConstant.UseAtlas == 1)
	{
		vec3 msd = color.rgb;
		float sd = Median(msd.r, msd.g, msd.b);
		float screenPxDistance = ScreenPxRange() * (sd - 0.5f);
		mixOpacity = clamp(screenPxDistance + 0.5, 0.0, 1.0);
		color = mix(color, v_Color, mixOpacity);
	}

	if(color.a <= 0.5f)
		discard;

	o_Color = color;
}