#type compute
#version 460

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(binding = 0) uniform sampler2D u_ColorFrameTexture;
layout(binding = 1) uniform sampler2D u_BloomTexture;
layout(binding = 2) uniform sampler2D u_BloomConvTexture;
layout(binding = 3) uniform sampler2D u_BloomDirtTexture;
layout(binding = 4) uniform sampler2D u_SSRTexture;
layout(binding = 5) uniform sampler2D u_AOTexture;
layout(binding = 6) uniform sampler2D u_VolumetricTexture;
layout(binding = 7) uniform sampler2D u_CloudComputeTex;
layout(binding = 10) uniform sampler2D u_SpatialBlueNoiseLUT;

// This texture is responsible for firstly adding up the bloom factor (without tone mapping)
// and then in the SSR we use it for tracing. In the last stage we just add the SSR and do tone mapping.
// We need 2 textures, because if we had only 1, then we would do tone mapping twice (once for the basic image, then when tracing we will be using the tone mapped texture, and so when we add the SSR on the final tone mapped texture, it will add the tone mapping twice).
layout(binding = 8, rgba8) uniform restrict writeonly image2D o_Texture_ForSSR;
layout(binding = 9, rgba8) uniform restrict writeonly image2D o_Texture_Final;

#define TONE_MAP_FRAME          0
#define TONE_MAP_FRAME_WITH_SSR 1

layout(push_constant) uniform PushConstant {
	float Gamma;
	float Exposure;
	float Stage;

	int UseSSR;
	int UseAO;
	int UseBloom;
} u_PushConstant;

layout(set = 0, binding = 11) uniform BloomConfiguration
{
	float BloomConvolutionAmount;
	float BloomConvolutionExposure;

	float BloomDirtContribution;

} u_BloomConfiguration;

// ------------------------ LUTS --------------------------
vec4 SampleBlueNoise(ivec2 coords)
{
	return texture(u_SpatialBlueNoiseLUT, (vec2(coords) + 0.5.xx) / vec2(textureSize(u_SpatialBlueNoiseLUT, 0).xy));
}

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

vec3 AO_MultiBounce(float ao, vec3 albedo)
{
	vec3 x = vec3(ao);

	vec3 a = 2.0404 * albedo - vec3(0.3324);
	vec3 b = -4.7951 * albedo + vec3(0.6417);
	vec3 c = 2.7552 * albedo + vec3(0.6903);

	return max(x, ((x * a + b) * x + c) * x);
}

vec4 UpsampleTent9(sampler2D tex, float lod, vec2 uv, float radius)
{
	vec2 texSize = vec2(textureSize(tex, int(lod)));
    vec2 texelSize = 1.0f / texSize;

    vec4 offset = texelSize.xyxy * vec4(1.0f, 1.0f, -1.0f, 0.0f) * radius;

    // Center
	vec4 center = textureLod(tex, uv, lod);
    vec3 result = center.rgb * 4.0f;                          //  (0, 0)

    result += textureLod(tex, uv - offset.xy, lod).rgb;       // -( 1,  1 ) // 1
    result += textureLod(tex, uv - offset.wy, lod).rgb * 2.0; // -( 0,  1 ) // 2
    result += textureLod(tex, uv - offset.zy, lod).rgb;       // -(-1,  1 ) // 3

    result += textureLod(tex, uv + offset.zw, lod).rgb * 2.0; // +(-1,  0 ) // 4
    result += textureLod(tex, uv + offset.xw, lod).rgb * 2.0; // +( 1,  0 ) // 5

    result += textureLod(tex, uv + offset.zy, lod).rgb;       // +(-1,  1 ) // 6
    result += textureLod(tex, uv + offset.wy, lod).rgb * 2.0; // +( 0,  1 ) // 7
    result += textureLod(tex, uv + offset.xy, lod).rgb;       // +( 1,  1 ) // 8

    // Sampling order (C = center)
    //   6 7 8
    //   4 C 5
    //   3 2 1

    return vec4(result * (1.0f / 16.0f), center.a);
}

void main()
{
	ivec2 loc = ivec2(gl_GlobalInvocationID.xy);
	vec2 imgSize = vec2(imageSize(o_Texture_Final));
	vec2 uv = (vec2(loc) + 0.5f.xx) / vec2(imageSize(o_Texture_Final).xy);
	
	if (any(greaterThanEqual(loc, imgSize)))
	  return;


	if(u_PushConstant.Stage == TONE_MAP_FRAME)
	{
		vec3 color = texelFetch(u_ColorFrameTexture, loc, 0).rgb;
		vec3 bloomFactor = texelFetch(u_BloomTexture, loc, 0).rgb;

		color += bloomFactor;

		imageStore(o_Texture_ForSSR, loc, vec4(color, 1.0f));
	}
	else if(u_PushConstant.Stage == TONE_MAP_FRAME_WITH_SSR)
	{
		// Get the current pixel color from PBR shader
		vec3 color = texelFetch(u_ColorFrameTexture, loc, 0).rgb;

		// Contribution from the screen space reflections
		if(u_PushConstant.UseSSR == 1)
		{
			vec2 texSize = vec2(textureSize(u_SSRTexture, 0));
			vec2 texelSize = 2.0f / texSize;

			// Sampling more directions to compensate for the fact that we are rendering SSR on half-resolution
			//vec3 reflectionContribution = texture(u_SSRTexture, uv + blueNoise * noiseWeight).rgb;
			vec3 reflectionContribution = vec3(0.0);

			reflectionContribution += texture(u_SSRTexture, uv).rgb;

			//reflectionContribution += texture(u_SSRTexture, uv + vec2(texSize.x, 0.0) + blueNoise * 0.001).rgb;
			//reflectionContribution += texture(u_SSRTexture, uv - vec2(texSize.x, 0.0) + blueNoise * 0.001).rgb;
			//reflectionContribution += texture(u_SSRTexture, uv + vec2(0.0, texSize.y)  + blueNoise * 0.001).rgb;
			//reflectionContribution += texture(u_SSRTexture, uv - vec2(0.0, texSize.y)  + blueNoise * 0.001).rgb;
			//reflectionContribution /= 4.0;

			color += reflectionContribution.xyz;
		}

		

		// Contribution from ambient occlusion
		if(u_PushConstant.UseAO == 1)
		{
			float ao = texture(u_AOTexture, uv).r;
			//vec3 ao_contribution = AO_MultiBounce(ao, color);
			color = color * ao;
		}

		
		// Bloom factor
		if(u_PushConstant.UseBloom == 1)
		{
			ivec2 bloomConvResolution = ivec2(textureSize(u_BloomConvTexture, 0).xy);
			ivec2 offset = (bloomConvResolution - ivec2(imgSize)) / 2;
			vec3 bloomConvolution = texelFetch(u_BloomConvTexture, offset + loc, 0).rgb;

			//vec3 bloomFactor = texelFetch(u_BloomTexture, loc, 0).rgb;
			//color = mix(color, bloomConvolution * 0.3, 0.05);

			
			vec3 colorDelta = color;
			colorDelta = mix(colorDelta, bloomConvolution * u_BloomConfiguration.BloomConvolutionExposure, u_BloomConfiguration.BloomConvolutionAmount);
			colorDelta = colorDelta - color;
			color += colorDelta;

			vec3 dirt = texture(u_BloomDirtTexture, uv).rgb;
			color += max(colorDelta * dirt * u_BloomConfiguration.BloomDirtContribution * 100.0, vec3(0.0));
			//color = mix(color, bloomConvolution * dirt * u_BloomConfiguration.BloomConvolutionExposure, u_BloomConfiguration.BloomDirtContribution);

			//color = mix(color, bloomConvolution * dirt * u_BloomConfiguration.BloomConvolutionExposure, u_BloomConfiguration.BloomDirtContribution);
		}

		//vec4 cloudContribution = UpsampleTent9(u_CloudComputeTex, 0.0, uv, 3.0);
		//color *= cloudContribution.a;
		//color += cloudContribution.rgb;


		vec4 volumetricContribution = texture(u_VolumetricTexture, uv);
		color *= volumetricContribution.a;
		color += volumetricContribution.rgb;

		
		// Tonemapping (ACES algorithm)
		color = AcesApprox(color);

		// Gamma correction
		color = pow(color, vec3(1.0f / u_PushConstant.Gamma));

		imageStore(o_Texture_Final, loc, vec4(color, 1.0f));
	}
}