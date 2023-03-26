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
            float h = 2.0;

            vec4 o = texture(i_SrcImage, s_UV + (vec2( 0, 0)/imgSize.xy));
	        vec4 n = texture(i_SrcImage, s_UV + (vec2( 0, h)/imgSize.xy));
            vec4 e = texture(i_SrcImage, s_UV + (vec2( h, 0)/imgSize.xy));
            vec4 s = texture(i_SrcImage, s_UV + (vec2( 0,-h)/imgSize.xy));
            vec4 w = texture(i_SrcImage, s_UV + (vec2(-h, 0)/imgSize.xy));

            vec4 dy = (n - s)*.5;
            vec4 dx = (e - w)*.5;
    
            vec4 edge = sqrt(dx*dx + dy*dy);
            vec4 angle = atan(dy, dx);
   
            result = edge.xyz * 5.0;
        }
    }

	imageStore(o_DstImage, ivec2(gl_GlobalInvocationID.xy), vec4(result, initialColor.a));
}