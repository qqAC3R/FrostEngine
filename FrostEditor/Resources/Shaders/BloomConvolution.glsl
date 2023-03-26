#type compute
#version 450 core

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

precision highp float;

const float PI = 3.141592;
const float TWO_PI = 6.283185307179586;

layout(binding = 0) uniform sampler2D u_SourceTex;
layout(binding = 1, rgba16f) uniform image2D u_DstTex;

layout(push_constant) uniform PushConstant
{
    vec2 Resolution; // Resolution of screen
	float SubtransformSize; // The subpixels that we are calculating
	float Horizontal; // Horizontal or vertical pass
	float IsNotInverse; // FFT or Inverse-FFT
	float Normalization; // Normalization oftenly is performed once while we invert the texture from frequency to image
} u_PushConstant;


vec2 multiplyComplex (vec2 a, vec2 b) {
	return vec2(a[0] * b[0] - a[1] * b[1], a[1] * b[0] + a[0] * b[1]);
}

vec4 FFT(sampler2D src, vec2 resolution, float subPixelSize, float horizontal, float isNotInverse, float normalization)
{
	vec2 fragCoord = vec2(gl_GlobalInvocationID.xy);
	//fragCoord.x = u_PushConstant.Resolution.x - fragCoord.x;
	//fragCoord.y = u_PushConstant.Resolution.y - fragCoord.y;


	float index = 0.0;
    if (horizontal == 1.0)
        index = fragCoord.x - 0.5;
	else
	{
        index = fragCoord.y - 0.5;
	}

	float evenIndex = floor(index / subPixelSize) * (subPixelSize / 2.0) + mod(index, subPixelSize / 2.0);

	vec4 even = vec4(0.0), odd = vec4(0.0);

	if (horizontal == 1.0)
	{
		even = texture(src, vec2(evenIndex + 0.5, fragCoord.y) / resolution.x);
		odd = texture(src, vec2(evenIndex + resolution.x * 0.5 + 0.5, fragCoord.y) / resolution.y);
    }
	else
	{
		even = texture(src, vec2(fragCoord.x, evenIndex + 0.5) / resolution.x);
        odd = texture(src, vec2(fragCoord.x, evenIndex + resolution.y * 0.5 + 0.5) / resolution.y);
	}

	//normalisation
	//if (normalization == 1.0) {
	//    even /= (resolution.x * resolution.y);
	//    odd /= (resolution.x * resolution.y);
	//}

	//fragCoord.x = u_PushConstant.Resolution.x - fragCoord.x;

	float twiddleArgument = 0.0;
    if (isNotInverse == 1.0)
        twiddleArgument = 2.0 * PI * (index / subPixelSize);
	else
        twiddleArgument = -2.0 * PI * (index / subPixelSize);

	vec2 twiddle = vec2(cos(twiddleArgument), sin(twiddleArgument));

	if(subPixelSize == 2.0 && horizontal == 1.0)
	{
		if (isNotInverse == 1.0)
			twiddleArgument = 2.0 * PI * (index / subPixelSize);
		else
			twiddleArgument = -2.0 * PI * (index / subPixelSize);

		twiddle = vec2(sin(twiddleArgument), cos(twiddleArgument));
	}



    //vec2 twiddle = vec2(cos(PI * (index / subPixelSize)), sin(PI * (index / subPixelSize)));


	vec2 outputA = even.rg + multiplyComplex(twiddle, odd.rg);
    vec2 outputB = even.ba + multiplyComplex(twiddle, odd.ba);

	float optional1 = 0.0;
    float optional2 = 0.0;

	

    if(twiddle.x < 0.0)
    {
        optional1 = 1.0;
    }

    if(twiddle.y < 0.0)
    {
        optional2 = 1.0;
    }


	
	return normalize(vec4(outputA, 0.0, 0.0));
	//return vec4(odd);
}


void main()
{
	vec4 fft = FFT(
		u_SourceTex,
		u_PushConstant.Resolution,
		u_PushConstant.SubtransformSize,
		u_PushConstant.Horizontal,
		u_PushConstant.IsNotInverse,
		u_PushConstant.Normalization
	);

	

	imageStore(u_DstTex, ivec2(gl_GlobalInvocationID.xy), vec4(fft));
}


