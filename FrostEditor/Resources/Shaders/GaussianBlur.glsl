#type compute
#version 460

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;
#pragma optionNV (unroll all)

layout(binding = 0)           uniform sampler2D i_SrcImage;
layout(binding = 1) writeonly uniform image2D   o_DstImage;

#define GAUSSIAN_MODE_ONE_PASS 0
#define GAUSSIAN_MODE_VERTICAL_PASS 1
#define GAUSSIAN_MODE_HORIZONTAL_PASS 2

layout(push_constant) uniform PushConstant {
	vec3 ScreenSize; // ScreenSize || MipLevel
	float Mode;
} u_PushConstant;

float weight[5] = float[] (0.2270270270, 0.1945945946, 0.1216216216, 0.0540540541, 0.0162162162);

vec4 blur13(sampler2D image, vec2 uv, vec2 resolution, vec2 direction)
{
    vec4 color = vec4(0.0);
    vec2 off1 = vec2(1.411764705882353) * direction;
    vec2 off2 = vec2(3.2941176470588234) * direction;
    vec2 off3 = vec2(5.176470588235294) * direction;
    color += texture(image, uv) * 0.1964825501511404;
    color += texture(image, uv + (off1 / resolution)) * 0.2969069646728344;
    color += texture(image, uv - (off1 / resolution)) * 0.2969069646728344;
    color += texture(image, uv + (off2 / resolution)) * 0.09447039785044732;
    color += texture(image, uv - (off2 / resolution)) * 0.09447039785044732;
    color += texture(image, uv + (off3 / resolution)) * 0.010381362401148057;
    color += texture(image, uv - (off3 / resolution)) * 0.010381362401148057;
    return color;
}

vec3 DownSampleBox13(sampler2D tex, vec2 uv, vec2 texelSize)
{
    // Center
    vec3 A = texture(tex, uv).rgb;

    texelSize *= 0.5f; // Sample from center of texels

    // Inner box

    //     * = texel not sampled
    //      =========
    //      | C * D |
    //      | * A * |
    //      | B * E |
    //      =========
    vec3 B = texture(tex, uv + texelSize * vec2(-1.0f, -1.0f)).rgb;
    vec3 C = texture(tex, uv + texelSize * vec2(-1.0f, 1.0f)).rgb;
    vec3 D = texture(tex, uv + texelSize * vec2(1.0f, 1.0f)).rgb;
    vec3 E = texture(tex, uv + texelSize * vec2(1.0f, -1.0f)).rgb;

    // Outer box
    
    
    //     * = texel not sampled
    //           H
    //     L ========= I
    //     * | C * D | *
    //     G | * A * | K
    //     * | B * E | *
    //     F ========= J
    //           M
    vec3 F = texture(tex, uv + texelSize * vec2(-2.0f, -2.0f)).rgb;
    vec3 G = texture(tex, uv + texelSize * vec2(-2.0f,  0.0f)).rgb;
    vec3 H = texture(tex, uv + texelSize * vec2( 0.0f,  2.0f)).rgb;
    vec3 I = texture(tex, uv + texelSize * vec2( 2.0f,  2.0f)).rgb;
    vec3 J = texture(tex, uv + texelSize * vec2( 2.0f, -2.0f)).rgb;
    vec3 K = texture(tex, uv + texelSize * vec2( 2.0f,  0.0f)).rgb;
    vec3 L = texture(tex, uv + texelSize * vec2(-2.0f, -2.0f)).rgb;
    vec3 M = texture(tex, uv + texelSize * vec2( 0.0f, -2.0f)).rgb;

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

void main()
{
    vec2 imgSize = vec2(imageSize(o_DstImage));
	vec2 s_UV = vec2(gl_GlobalInvocationID.xy) / imgSize;

    vec2 globalInvocation = vec2(gl_GlobalInvocationID.xy);
    if(globalInvocation.x > imgSize.x || globalInvocation.y > imgSize.y) return;
	
    vec4 initialColor = texture(i_SrcImage, s_UV).rgba; // We should preserve the alpha component for future needs
    vec3 result = initialColor.rgb * weight[0];
            
    vec2 texOffset = 1.0f / textureSize(i_SrcImage, 0);
    

    uint gaussianMode = uint(u_PushConstant.Mode);
    switch(gaussianMode)
    {
        case GAUSSIAN_MODE_ONE_PASS:
        {   
            vec3 horizontalResult = result;
            vec3 verticalResult = result;

            // Horizontal blur
            for(int i = 1; i < 5; i++)
            {
                horizontalResult += texture(i_SrcImage, s_UV + vec2(texOffset.x * i, 0.0)).rgb * weight[i];
                horizontalResult += texture(i_SrcImage, s_UV - vec2(texOffset.x * i, 0.0)).rgb * weight[i];
            }
    
            // Vertical blur
            for(int i = 1; i < 5; i++)
            {
                verticalResult += texture(i_SrcImage, s_UV + vec2(0.0, texOffset.y * i)).rgb * weight[i];
                verticalResult += texture(i_SrcImage, s_UV - vec2(0.0, texOffset.y * i)).rgb * weight[i];
            }
            result = (horizontalResult + verticalResult) * 0.5f;

            break;
        }

        case GAUSSIAN_MODE_VERTICAL_PASS:
        {   
            vec3 verticalResult = result;
    
            // Vertical blur
            for(int i = 1; i < 5; i++)
            {
                verticalResult += texture(i_SrcImage, s_UV + vec2(0.0, texOffset.y * i)).rgb * weight[i];
                verticalResult += texture(i_SrcImage, s_UV - vec2(0.0, texOffset.y * i)).rgb * weight[i];
            }
            result = verticalResult;
            
            break;
        }

        case GAUSSIAN_MODE_HORIZONTAL_PASS:
        {   
            vec3 horizontalResult = result;

            // Horizontal blur
            for(int i = 1; i < 5; i++)
            {
                horizontalResult += texture(i_SrcImage, s_UV + vec2(texOffset.x * i, 0.0)).rgb * weight[i];
                horizontalResult += texture(i_SrcImage, s_UV - vec2(texOffset.x * i, 0.0)).rgb * weight[i];
            }
            result = horizontalResult;

            break;
        }

        case 3:
        {
            result = blur13(i_SrcImage, s_UV, imgSize, vec2(2.5f, 0.0)).rgb;
            result += blur13(i_SrcImage, s_UV, imgSize, vec2(0.0f, 2.5f)).rgb;
            result /= 2.0f;
            break;
        }
        case 4:
        {
            vec2 texelSize = 1.0f / imgSize;
            result = DownSampleBox13(i_SrcImage, s_UV, texelSize).rgb;
            break;
        }
    }

	imageStore(o_DstImage, ivec2(gl_GlobalInvocationID.xy), vec4(result, initialColor.a));
}