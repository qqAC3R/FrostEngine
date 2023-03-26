#type compute
#version 460

// Credit: https://www.shadertoy.com/view/td2yDm

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;
#pragma optionNV (unroll all)

layout(binding = 0)           uniform sampler2D i_SrcImage;
layout(binding = 1) writeonly uniform image2D   o_DstImage;

void main()
{
    vec2 imgSize = vec2(textureSize(i_SrcImage, 0).xy);
	vec2 s_UV = vec2(gl_GlobalInvocationID.xy) / imgSize;

    vec2 globalInvocation = vec2(gl_GlobalInvocationID.xy);
    if(globalInvocation.x > imgSize.x || globalInvocation.y > imgSize.y) return;
	
    vec3 result = vec3(0.0);
    
    float h = 2.0;
    vec4 o = texture(i_SrcImage, s_UV + (vec2( 0,  0)/imgSize.xy));
	vec4 n = texture(i_SrcImage, s_UV + (vec2( 0,  h)/imgSize.xy));
    vec4 e = texture(i_SrcImage, s_UV + (vec2( h,  0)/imgSize.xy));
    vec4 s = texture(i_SrcImage, s_UV + (vec2( 0, -h)/imgSize.xy));
    vec4 w = texture(i_SrcImage, s_UV + (vec2(-h,  0)/imgSize.xy));

    vec4 dy = (n - s) * 0.5;
    vec4 dx = (e - w) * 0.5;
    
    vec4 edge = sqrt(dx*dx + dy*dy);
    vec4 angle = atan(dy, dx);
   
    result = edge.xyz * 5.0;

	imageStore(o_DstImage, ivec2(gl_GlobalInvocationID.xy), vec4(result, 1.0));
}