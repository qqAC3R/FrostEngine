#type compute
#version 460

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(binding = 0) uniform sampler2D i_Texture;
layout(binding = 1) writeonly uniform image2D o_Texture;


#define INV_SQRT_OF_2PI 0.39894228040143267793994605993439  // 1.0/SQRT_OF_2PI
#define INV_PI 0.31830988618379067153776752674503

//  Used in Example: sRGB space
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
vec4 smartDeNoise(sampler2D tex, vec2 uv, float sigma, float kSigma, float threshold)
{
    float radius = round(kSigma*sigma);
    float radQ = radius * radius;

    float invSigmaQx2 = .5 / (sigma * sigma);      // 1.0 / (sigma^2 * 2.0)
    float invSigmaQx2PI = INV_PI * invSigmaQx2;    // // 1/(2 * PI * sigma^2)

    float invThresholdSqx2 = .5 / (threshold * threshold);     // 1.0 / (sigma^2 * 2.0)
    float invThresholdSqrt2PI = INV_SQRT_OF_2PI / threshold;   // 1.0 / (sqrt(2*PI) * sigma)

    vec4 centrPx = vec4(texture(tex,uv).r);

    float zBuff = 0.0;
    vec4 aBuff = vec4(0.0);
    vec2 size = vec2(textureSize(tex, 0));

    vec2 d;
    for (d.x=-radius; d.x <= radius; d.x++) {
        float pt = sqrt(radQ-d.x*d.x);       // pt = yRadius: have circular trend
        for (d.y=-pt; d.y <= pt; d.y++) {
            float blurFactor = exp( -dot(d , d) * invSigmaQx2 ) * invSigmaQx2PI;

            vec4 walkPx =  vec4(texture(tex,uv+d/size).r);
            
            vec4 dC = walkPx-centrPx;
            float deltaFactor = exp( -dot(dC.rgb, dC.rgb) * invThresholdSqx2) * invThresholdSqrt2PI * blurFactor;

            zBuff += deltaFactor;
            aBuff += deltaFactor*walkPx;
        }
    }
    return aBuff/zBuff;
}

float Sample2x2(sampler2D tex, ivec2 loc)
{
	vec4 color = textureGather(i_Texture, loc + ivec2(0, 0), 0);
	return (color.x + color.y + color.z + color.w) / 4.0f;
}

void main()
{
	// NOTE: 4x4 filter offsets image
	ivec2 loc = ivec2(gl_GlobalInvocationID.xy) - ivec2(2);
	float totalao = 0.0;

	vec2 baseSize = vec2(textureSize(i_Texture, 0));
	vec2 uv = ((vec2(loc) + 0.5.xx) / vec2(imageSize(o_Texture)));
	vec2 texelSize = 1.0 / baseSize;

	//vec4 ao = smartDeNoise(i_Texture, uv, 7, 1.29f, 0.195f);

	// NOTE: textureGather requires GL 4

	//totalao += Sample2x2(i_Texture, loc + ivec2(0, 0));
	//totalao += Sample2x2(i_Texture, loc + ivec2(2, 0));
	//totalao += Sample2x2(i_Texture, loc + ivec2(0, 2));
	//totalao += Sample2x2(i_Texture, loc + ivec2(2, 2));

	totalao += texture(i_Texture, uv + texelSize * vec2(0.0, 0.0)).r;
	totalao += texture(i_Texture, uv + texelSize * vec2(1.0, 0.0)).r;
	totalao += texture(i_Texture, uv + texelSize * vec2(0.0, 1.0)).r;
	totalao += texture(i_Texture, uv + texelSize * vec2(1.0, 1.0)).r;
	
	totalao += texture(i_Texture, uv + texelSize * vec2(3.0, 0.0)).r;
	totalao += texture(i_Texture, uv + texelSize * vec2(2.0, 0.0)).r;
	totalao += texture(i_Texture, uv + texelSize * vec2(2.0, 1.0)).r;
	totalao += texture(i_Texture, uv + texelSize * vec2(3.0, 1.0)).r;
	
	totalao += texture(i_Texture, uv + texelSize * vec2(0.0, 2.0)).r;
	totalao += texture(i_Texture, uv + texelSize * vec2(1.0, 2.0)).r;
	totalao += texture(i_Texture, uv + texelSize * vec2(0.0, 3.0)).r;
	totalao += texture(i_Texture, uv + texelSize * vec2(1.0, 3.0)).r;
	
	totalao += texture(i_Texture, uv + texelSize * vec2(2.0, 2.0)).r;
	totalao += texture(i_Texture, uv + texelSize * vec2(3.0, 2.0)).r;
	totalao += texture(i_Texture, uv + texelSize * vec2(2.0, 3.0)).r;
	totalao += texture(i_Texture, uv + texelSize * vec2(3.0, 3.0)).r;

	imageStore(o_Texture, ivec2(gl_GlobalInvocationID.xy), vec4(totalao / 16.0f, vec3(1.0f)));
}