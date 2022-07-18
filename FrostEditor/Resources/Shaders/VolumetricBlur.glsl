#type compute
#version 460

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

#define KERNEL_RADIUS 6
#define SHARPNESS 3.0

layout(binding = 0) uniform sampler2D u_SrcTex;
layout(binding = 1) uniform sampler2D u_DepthBuffer; // Preferred to be a depth pyramid
layout(binding = 2) writeonly uniform image2D u_DstTex;

layout(push_constant) uniform PushConstant {
	vec2 BlurDirection; // For the blur, we should do 2 blur passes, one vertical and one horizontal
	uint DepthMipSample;
	uint Mode;
} u_PushConstant;

#define MODE_BLUR 0
#define MODE_UPSAMPLE 1

// Depth aware bilateral blur
vec4 BlurFunction(vec2 uv, float r, float centerDepth, sampler2D depthMap,
                   sampler2D blurMap, vec2 nearFar, inout float totalWeight)
{
	vec4 aoSample = texture(blurMap, uv);
	float d = textureLod(u_DepthBuffer, uv, u_PushConstant.DepthMipSample).r;
	
	const float blurSigma = float(KERNEL_RADIUS) * 0.5;
	const float blurFalloff = 1.0 / (2.0 * blurSigma * blurSigma);
	
	float ddiff = abs(d - centerDepth) * SHARPNESS;
	float w = exp2(-r * r * blurFalloff - ddiff * ddiff);
	totalWeight += w;
	
	return aoSample * w;
}

vec3 UpsampleTent9(sampler2D tex, vec2 uv, float radius)
{
	vec2 texelSize = 1.0f / vec2(textureSize(u_SrcTex, 0).xy);
    vec4 offset = texelSize.xyxy * vec4(1.0f, 1.0f, -1.0f, 0.0f) * radius;

    // Center
    vec3 result = texture(tex, uv).rgb * 4.0f;        //  (0, 0)

    result += texture(tex, uv - offset.xy).rgb;       // -( 1,  1 ) // 1
    result += texture(tex, uv - offset.wy).rgb * 2.0; // -( 0,  1 ) // 2
    result += texture(tex, uv - offset.zy).rgb;       // -(-1,  1 ) // 3

    result += texture(tex, uv + offset.zw).rgb * 2.0; // +(-1,  0 ) // 4
    result += texture(tex, uv + offset.xw).rgb * 2.0; // +( 1,  0 ) // 5

    result += texture(tex, uv + offset.zy).rgb;       // +(-1,  1 ) // 6
    result += texture(tex, uv + offset.wy).rgb * 2.0; // +( 0,  1 ) // 7
    result += texture(tex, uv + offset.xy).rgb;       // +( 1,  1 ) // 8

    // Sampling order (C = center)
    //   6 7 8
    //   4 C 5
    //   3 2 1

    return result * (1.0f / 16.0f);
}

void main()
{
	ivec2 globalInvocation = ivec2(gl_GlobalInvocationID.xy);
	ivec2 outSize = ivec2(imageSize(u_DstTex).xy);

	if (any(greaterThanEqual(globalInvocation, outSize)))
	  return;
	
	switch(u_PushConstant.Mode)
	{
		case MODE_BLUR:
		{
			vec2 texelSize = vec2(1.0) / vec2(outSize);
			vec2 uvs = (vec2(globalInvocation) + vec2(0.5)) * texelSize;

			vec4 center = texture(u_SrcTex, uvs);
			float centerDepth = textureLod(u_DepthBuffer, uvs, u_PushConstant.DepthMipSample).r;


			vec4 cTotal = center;
			float wTotal = 1.0;

			for (uint r = 1; r <= KERNEL_RADIUS; r++)
			{
				vec2 uv = uvs + (2.0 * texelSize * float(r) * u_PushConstant.BlurDirection);
				cTotal += BlurFunction(uv, float(r), centerDepth, u_DepthBuffer, u_SrcTex, u_PushConstant.BlurDirection, wTotal);
			}

			for (uint r = 1; r <= KERNEL_RADIUS; r++)
			{
				vec2 uv = uvs - (2.0 * texelSize * float(r) * u_PushConstant.BlurDirection);
				cTotal += BlurFunction(uv, float(r), centerDepth, u_DepthBuffer, u_SrcTex, u_PushConstant.BlurDirection, wTotal);
			}

			imageStore(u_DstTex, globalInvocation, cTotal / max(wTotal, 1e-4));
			break;
		}
		case MODE_UPSAMPLE:
		{
			float sampleScale = 1.0;
			vec2 texelSize = vec2(1.0) / vec2(outSize);
			vec2 uv = (vec2(globalInvocation) + vec2(0.5)) * texelSize;
			vec3 result = UpsampleTent9(u_SrcTex, uv, sampleScale);
			
			imageStore(u_DstTex, globalInvocation, vec4(result, 1.0));
			break;
		}
	}
}