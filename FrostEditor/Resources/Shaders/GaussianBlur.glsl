#type compute
#version 460

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;
#pragma optionNV (unroll all)

layout(binding = 0)           uniform sampler2D i_SrcImage;
layout(binding = 1) writeonly uniform image2D   o_DstImage;

#define MODE_PREFILTER 0
#define MODE_DOWNSAMPLE 1
#define MODE_UPSAMPLE 2

layout(push_constant) uniform PushConstant {
	vec3 ScreenSize; // ScreenSize || MipLevel
	float Mode;
} u_PushConstant;

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

float weight[5] = float[] (0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main()
{
    vec2 imgSize = vec2(imageSize(o_DstImage));
	vec2 s_UV = vec2(gl_GlobalInvocationID.xy) / imgSize;

    vec2 globalInvocation = vec2(gl_GlobalInvocationID.xy);
    if(globalInvocation.x > u_PushConstant.ScreenSize.x || globalInvocation.y > u_PushConstant.ScreenSize.y) return;
	
    vec3 result = vec3(0.0f);

    uint blurMode = uint(u_PushConstant.Mode);
    switch(blurMode)
    {
        //case MODE_PREFILTER:
        default:
        {
	        result = texture(i_SrcImage, s_UV).rgb * weight[0];
            
            vec2 texOffset = 1.0f / textureSize(i_SrcImage, 0);
            vec3 horizontalResult = result;
            vec3 verticalResult = result;

            // Horizontal blur
            {
                for(int i = 1; i < 5; i++)
                {
                    horizontalResult += texture(i_SrcImage, s_UV + vec2(texOffset.x * i, 0.0)).rgb * weight[i];
                    horizontalResult += texture(i_SrcImage, s_UV - vec2(texOffset.x * i, 0.0)).rgb * weight[i];
                }
            }
    
            // Vertical blur
            {
                for(int i = 1; i < 5; i++)
                {
                    verticalResult += texture(i_SrcImage, s_UV + vec2(0.0, texOffset.y * i)).rgb * weight[i];
                    verticalResult += texture(i_SrcImage, s_UV - vec2(0.0, texOffset.y * i)).rgb * weight[i];
                }
            }
            result = (horizontalResult + verticalResult) * 0.5f;
        }
        //case MODE_DOWNSAMPLE:
        //{
        //    // Downsample
        //    vec2 texSize = vec2(textureSize(i_SrcImage, 0));
        //    vec2 texelSize = 1.0f / texSize;
        //    result = DownSampleBox13(i_SrcImage, 0, s_UV, texelSize);
        //}
    }


	imageStore(o_DstImage, ivec2(gl_GlobalInvocationID.xy), vec4(result, 1.0f));
}