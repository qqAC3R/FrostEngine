#type compute
#version 460

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(binding = 0) uniform sampler2D u_ColorFrameTexture;
layout(binding = 1) uniform sampler2D u_BloomTexture;
layout(binding = 2, rgba8) uniform writeonly image2D o_Texture;

layout(push_constant) uniform PushConstant {
	float Gamma;
	float Exposure;
} u_PushConstant;

// From https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
vec3 AcesApprox(vec3 v)
{
    v *= 0.6f;
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((v*(a*v+b))/(v*(c*v+d)+e), 0.0f, 1.0f);
}

void main()
{
	ivec2 loc = ivec2(gl_GlobalInvocationID.xy);

	vec3 color = texelFetch(u_ColorFrameTexture, loc, 0).rgb;
	vec3 bloomFactor = texelFetch(u_BloomTexture, loc, 0).rgb;

	color += bloomFactor;

	// Tonemapping (ACES algorithm)
	color = AcesApprox(color);

	// Gamma correction
	color = pow(color, vec3(1.0f / u_PushConstant.Gamma));

	imageStore(o_Texture, loc, vec4(color, 1.0f));
}