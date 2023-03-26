#type compute
#version 460

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(binding = 0, rgba16f) writeonly uniform image2D o_Image;

layout(binding = 1) uniform sampler2D u_Texture;
layout(binding = 2) uniform sampler2D u_BloomTexture; // Add all the bloom toghether

layout(push_constant) uniform PushConstant
{
    vec4 Settings; // (x) threshold, (y) threshold - knee, (z) knee * 2, (w) 0.25 / knee
    float LOD;
    int Mode; // See defines below
} u_PushConstant;

#define MODE_PREFILTER      0
#define MODE_DOWNSAMPLE     1
#define MODE_UPSAMPLE_FIRST 2
#define MODE_UPSAMPLE       3

const float Epsilon = 1.0e-4;

vec3 DownSampleBox13(sampler2D tex, float lod, vec2 uv, vec2 texelSize)
{
    // Center
    vec3 A = textureLod(tex, uv, lod).rgb;

    texelSize *= 0.5f; // Sample from center of texels

    // Inner box

    //     * = texel not sampled
    //      =========
    //      | C * D |
    //      | * A * |
    //      | B * E |
    //      =========
    vec3 B = textureLod(tex, uv + texelSize * vec2(-1.0f, -1.0f), lod).rgb;
    vec3 C = textureLod(tex, uv + texelSize * vec2(-1.0f, 1.0f), lod).rgb;
    vec3 D = textureLod(tex, uv + texelSize * vec2(1.0f, 1.0f), lod).rgb;
    vec3 E = textureLod(tex, uv + texelSize * vec2(1.0f, -1.0f), lod).rgb;

    // Outer box
    
    
    //     * = texel not sampled
    //           H
    //     L ========= I
    //     * | C * D | *
    //     G | * A * | K
    //     * | B * E | *
    //     F ========= J
    //           M
    vec3 F = textureLod(tex, uv + texelSize * vec2(-2.0f, -2.0f), lod).rgb;
    vec3 G = textureLod(tex, uv + texelSize * vec2(-2.0f,  0.0f), lod).rgb;
    vec3 H = textureLod(tex, uv + texelSize * vec2( 0.0f,  2.0f), lod).rgb;
    vec3 I = textureLod(tex, uv + texelSize * vec2( 2.0f,  2.0f), lod).rgb;
    vec3 J = textureLod(tex, uv + texelSize * vec2( 2.0f, -2.0f), lod).rgb;
    vec3 K = textureLod(tex, uv + texelSize * vec2( 2.0f,  0.0f), lod).rgb;
    vec3 L = textureLod(tex, uv + texelSize * vec2(-2.0f, -2.0f), lod).rgb;
    vec3 M = textureLod(tex, uv + texelSize * vec2( 0.0f, -2.0f), lod).rgb;

    // Weights
    vec3 result = vec3(0.0);
    // Inner box
    result += (B + C + D + E) * 0.5f;
    // Bottom-left box
    result += (F + G + A + M) * 0.125f;
    // Top-left box
    result += (G + H + I + A) * 0.125f;
    // Top-right box
    result += (A + I + J + K) * 0.125f;
    // Bottom-right box
    result += (M + A + K + L) * 0.125f;

    // 4 samples each
    result *= 0.25f;

    return result;
}

// Quadratic color thresholding
// curve = (threshold - knee, knee * 2, 0.25 / knee)
vec4 QuadraticThreshold(vec4 color, float threshold, vec3 curve)
{
    // Maximum pixel brightness
    float brightness = max(max(color.r, color.g), color.b);
    // Quadratic curve
    float rq = clamp(brightness - curve.x, 0.0, curve.y);
    rq = (rq * rq) * curve.z;
    color *= max(rq, brightness - threshold) / max(brightness, Epsilon);
    return color;
}

vec3 Prefilter(vec4 color, vec2 uv)
{
    float clampValue = 20.0f;
    color = min(vec4(clampValue), color);
    color = QuadraticThreshold(color, u_PushConstant.Settings.x, u_PushConstant.Settings.yzw);

    return color.xyz;
}

vec3 UpsampleTent9(sampler2D tex, float lod, vec2 uv, vec2 texelSize, float radius)
{
    vec4 offset = texelSize.xyxy * vec4(1.0f, 1.0f, -1.0f, 0.0f) * radius;

    // Center
    vec3 result = textureLod(tex, uv, lod).rgb * 4.0f;        //  (0, 0)

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

    return result * (1.0f / 16.0f);
}

vec3 rgb2yuv (vec3 rgb) {
    return vec3 (
        rgb.r * 0.299 + rgb.g * 0.587 + rgb.b * 0.114,
        rgb.r * -0.169 + rgb.g * -0.331 + rgb.b * 0.5,
        rgb.r * 0.5 + rgb.g * -0.419 + rgb.b * -0.081
    );
}

void main()
{
	vec2 imgSize = vec2(imageSize(o_Image));
	ivec2 loc = ivec2(gl_GlobalInvocationID.xy);

    if (any(greaterThanEqual(loc, imgSize)))
        return;
	

	vec2 texCoords = vec2(float(loc.x) / imgSize.x, float(loc.y) / imgSize.y);
	//vec2 texCoords = vec2(loc) / vec2(imgSize);
	texCoords += (1.0f / imgSize) * 0.5f;

	vec2 texSize = vec2(textureSize(u_Texture, int(u_PushConstant.LOD)));
    vec4 color = vec4(1, 0, 1, 1);

	if(u_PushConstant.Mode == MODE_PREFILTER)
	{
        vec2 texelSize = 1.0f / texSize;
		color.rgb = DownSampleBox13(u_Texture, 0, texCoords, texelSize);
        color.rgb = Prefilter(color, texCoords);
        //color.rgb = rgb2yuv(color.rgb);
	}
    else if (u_PushConstant.Mode == MODE_DOWNSAMPLE)
    {
        // Downsample
        vec2 texelSize = 1.0f / texSize;
        //color.rgb = textureLod(u_Texture, texCoords, u_PushConstant.LOD).rgb;
        color.rgb = DownSampleBox13(u_Texture, u_PushConstant.LOD, texCoords, texelSize);
    }
    else if (u_PushConstant.Mode == MODE_UPSAMPLE_FIRST)
    {
        vec2 bloomTexSize = vec2(textureSize(u_Texture, int(u_PushConstant.LOD + 1.0f)));
        float sampleScale = 1.0f;
        vec2 texelSize = 1.0f / bloomTexSize;
        vec3 upsampledTexture = UpsampleTent9(u_Texture, u_PushConstant.LOD + 1.0f, texCoords, texelSize, sampleScale);

        vec3 existing = textureLod(u_Texture, texCoords, u_PushConstant.LOD).rgb;
        color.rgb = existing + upsampledTexture;
    }
    else if (u_PushConstant.Mode == MODE_UPSAMPLE)
    {
        vec2 bloomTexSize = vec2(textureSize(u_BloomTexture, int(u_PushConstant.LOD + 1.0f)));
        float sampleScale = 1.0f;
        vec2 texelSize = 1.0f / bloomTexSize;
        vec3 upsampledTexture = UpsampleTent9(u_BloomTexture, u_PushConstant.LOD + 1.0f, texCoords, texelSize, sampleScale);

        vec3 existing = textureLod(u_Texture, texCoords, u_PushConstant.LOD).rgb;
        color.rgb = existing + upsampledTexture;
    }

    imageStore(o_Image, ivec2(gl_GlobalInvocationID.xy), vec4(color.rgb, 1.0f));
}
