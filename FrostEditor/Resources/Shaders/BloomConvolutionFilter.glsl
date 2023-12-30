#type compute
#version 450 core

precision highp float;

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(binding = 0) uniform sampler2D u_Texture;
layout(binding = 1, rgba16f) uniform image2D u_ThresholdImage;

vec3 rgb2yuv (vec3 rgb)
{
	return vec3 (
		rgb.r * 0.299 + rgb.g * 0.587 + rgb.b * 0.114,
		rgb.r * -0.169 + rgb.g * -0.331 + rgb.b * 0.5,
		rgb.r * 0.5 + rgb.g * -0.419 + rgb.b * -0.081
	);
}

void main()
{
	vec3 c = texelFetch(u_Texture, ivec2(gl_GlobalInvocationID.xy), 0).rgb;
	float threshold = 1.0;

    vec3 yuv = rgb2yuv(c);
    float strength = smoothstep(threshold - 0.02, threshold + 0.02, yuv.x);

	imageStore(u_ThresholdImage, ivec2(gl_GlobalInvocationID.xy), vec4(strength * c, 1));
}