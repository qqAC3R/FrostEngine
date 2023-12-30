#type compute
#version 450 core

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

precision highp float;
const float TWO_PI = 6.283185307179586;

layout(binding = 0) uniform sampler2D u_ThresholdTex;
layout(binding = 1, rgba16f) uniform image2D u_FFTTex;

layout(push_constant) uniform PushConstant
{
    vec2 Resolution;
	float SubtransformSize;
	float Horizontal;
	float IsNotInverse;
	float Normalization;
} u_PushConstant;

vec2 multiplyComplex (vec2 a, vec2 b)
{
    return vec2(a[0] * b[0] - a[1] * b[1], a[1] * b[0] + a[0] * b[1]);
}

const float TWOPI = 6.283185307179586;
vec4 fft_2 (sampler2D src, vec2 resolution, float subtransformSize, float horizontal, float forward, float normalization) {
    
	vec2 fragCoord = vec2(gl_GlobalInvocationID.xy);
	fragCoord.y = resolution.y - fragCoord.y;

	vec2 evenPos, oddPos, twiddle, outputA, outputB;
    vec4 even, odd;
    
	float index, evenIndex, twiddleArgument;
    
	index = (horizontal == 1.0 ? fragCoord.x : fragCoord.y) - 0.5;

    evenIndex = floor(index / subtransformSize) *
		(subtransformSize * 0.5) +
		mod(index, subtransformSize * 0.5) +
		0.5;


    if (horizontal == 1.0) {
        evenPos = vec2(evenIndex, fragCoord.y);
        oddPos = vec2(evenIndex, fragCoord.y);
    }
    else {
        evenPos = vec2(fragCoord.x, evenIndex);
        oddPos = vec2(fragCoord.x, evenIndex);
    }

    evenPos *= (1.0 / resolution);
    oddPos *=  (1.0 / resolution);
    
	if (horizontal == 1.0)
        oddPos.x += 0.5;
    else
        oddPos.y += 0.5;

    even = texture(src, evenPos);
    odd = texture(src, oddPos);

    twiddleArgument = (forward == 1.0 ? TWOPI : -TWOPI) * (index / subtransformSize);
    twiddle = vec2(cos(twiddleArgument), sin(twiddleArgument));
    

	float normalizationFactor = 1.0;
	if(normalization == 1.0)
	{
		normalizationFactor = 1.0 / sqrt(resolution.x * resolution.y);
	}

	return (even.rgba + vec4(
		twiddle.x * odd.xz - twiddle.y * odd.yw,
		twiddle.y * odd.xz + twiddle.x * odd.yw).xzyw) * normalizationFactor;
}

vec4 FFT(sampler2D src, vec2 resolution, float subtransformSize, float horizontal, float forward, float normalization)
{
	vec2 fragCoord = vec2(gl_GlobalInvocationID.xy);
	fragCoord.y = resolution.y - fragCoord.y;

	float index = (horizontal == 1.0 ? fragCoord.x : fragCoord.y) - 0.5;

	float evenIndex = floor(index / subtransformSize) *
				      (subtransformSize * 0.5) +
				      mod(index, subtransformSize * 0.5) +
				      0.5; // CCCCCCCCCCCCCCC

	vec2 evenPos, oddPos;

	if (horizontal == 1.0)
	{
		evenPos = vec2(evenIndex, fragCoord.y);
		oddPos = vec2(evenIndex, fragCoord.y);
	}
	else
	{
		evenPos = vec2(fragCoord.x, evenIndex);
		oddPos = vec2(fragCoord.x, evenIndex);
	}

	evenPos *= (1.0 / resolution);
	oddPos  *= (1.0 / resolution);
	
	if (horizontal == 1.0)
	  oddPos.x += 0.5;
	else
	  oddPos.y += 0.5;
	
	vec4 even = texture(src, evenPos); // CCCCCCCCCCCCCCC
	vec4 odd = texture(src, oddPos);   // CCCCCCCCCCCCCCC

	float normalizationFactor = 1.0;
	if(normalization == 1.0)
	{
		//even /= resolution.x * resolution.x;
		//odd	 /= resolution.x * resolution.x;
		normalizationFactor = 1.0 / sqrt(resolution.x * resolution.y); // CCCCCCCCCCCCCCC
	}


	float twiddleArgument = (forward == 1.0 ? TWO_PI : -TWO_PI) * (index / subtransformSize);
	vec2 twiddle = vec2(cos(twiddleArgument), sin(twiddleArgument));

	//vec2 outputA = even.rg + multiplyComplex(twiddle, odd.rg);
    //vec2 outputB = even.ba + multiplyComplex(twiddle, odd.ba);
	
	//return vec4(outputA, outputB);
	//return vec4(twiddleArgument / 10.0, 0.0, 0.0, 1.0);

	return (even.rgba + vec4(
          twiddle.x * odd.xz - twiddle.y * odd.yw,
          twiddle.y * odd.xz + twiddle.x * odd.yw
    ).xzyw) * normalizationFactor;

}


void main()
{
	vec4 fft = fft_2(u_ThresholdTex, u_PushConstant.Resolution, u_PushConstant.SubtransformSize, u_PushConstant.Horizontal, u_PushConstant.IsNotInverse, u_PushConstant.Normalization);

	imageStore(u_FFTTex, ivec2(gl_GlobalInvocationID.xy), fft);
}