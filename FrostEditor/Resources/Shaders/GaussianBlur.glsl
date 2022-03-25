#type compute
#version 460

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;
#pragma optionNV (unroll all)

layout(binding = 0)           uniform sampler2D i_SrcImage;
layout(binding = 1) writeonly uniform image2D   o_DstImage;

layout(push_constant) uniform PushConstant {
	vec2 ScreenSize;
} u_PushConstant;

float weight[5] = float[] (0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main()
{
	vec2 s_UV = vec2(gl_GlobalInvocationID.xy) / u_PushConstant.ScreenSize;
	
    vec2 globalInvocation = vec2(gl_GlobalInvocationID.xy);
    if(globalInvocation.x > u_PushConstant.ScreenSize.x || globalInvocation.y > u_PushConstant.ScreenSize.y) return;
	
    vec2 texOffset = 1.0f / textureSize(i_SrcImage, 0);
	vec3 result = texture(i_SrcImage, s_UV).rgb * weight[0];


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

	imageStore(o_DstImage, ivec2(gl_GlobalInvocationID.xy), vec4(result, 1.0f));
}