#type compute
#version 460

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(binding = 0) uniform sampler2D u_ColorFrameTexture;
layout(binding = 1) uniform sampler2D u_BloomTexture;
layout(binding = 2) uniform sampler2D u_SSRTexture;
layout(binding = 3) uniform sampler2D u_AOTexture;
layout(binding = 4, rgba8) uniform writeonly image2D o_Texture_ForSSR;
layout(binding = 5, rgba8) uniform writeonly image2D o_Texture_Final;

#define TONE_MAP_FRAME          0
#define TONE_MAP_FRAME_WITH_SSR 1

layout(push_constant) uniform PushConstant {
	float Gamma;
	float Exposure;
	float Stage;
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

vec3 AO_MultiBounce(float ao, vec3 albedo)
{
	vec3 x = vec3(ao);

	vec3 a = 2.0404 * albedo - vec3(0.3324);
	vec3 b = -4.7951 * albedo + vec3(0.6417);
	vec3 c = 2.7552 * albedo + vec3(0.6903);

	return max(x, ((x * a + b) * x + c) * x);
}

void main()
{
	ivec2 loc = ivec2(gl_GlobalInvocationID.xy);

	
	if(u_PushConstant.Stage == TONE_MAP_FRAME)
	{
		vec3 color = texelFetch(u_ColorFrameTexture, loc, 0).rgb;
		vec3 bloomFactor = texelFetch(u_BloomTexture, loc, 0).rgb;

		color += bloomFactor;

		// Tonemapping (ACES algorithm)
		//color = AcesApprox(color);

		// Gamma correction
		//color = pow(color, vec3(1.0f / u_PushConstant.Gamma));

		imageStore(o_Texture_ForSSR, loc, vec4(color, 1.0f));
	}
	else if(u_PushConstant.Stage == TONE_MAP_FRAME_WITH_SSR)
	{
		// Get the current pixel color from PBR shader
		vec3 color = texelFetch(u_ColorFrameTexture, loc, 0).rgb;

		// Contribution from the screen space reflections
		vec3 reflectionContribution = texelFetch(u_SSRTexture, loc, 0).rgb;
		color = mix(color, reflectionContribution.xyz, 0.4f);

		// Bloom factor
		vec3 bloomFactor = texelFetch(u_BloomTexture, loc, 0).rgb;
		color += bloomFactor;

		// Contribution from ambient occlusion
		float ao = texelFetch(u_AOTexture, loc, 0).r;
		vec3 ao_contribution = AO_MultiBounce(ao, color);
		color = color * ao_contribution;


		// Tonemapping (ACES algorithm)
		color = AcesApprox(color);

		// Gamma correction
		color = pow(color, vec3(1.0f / u_PushConstant.Gamma));

		imageStore(o_Texture_Final, loc, vec4(color, 1.0f));
	}
}