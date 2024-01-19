#type compute
#version 450 core

layout(binding = 0) uniform sampler2D u_InputImage;
layout(rgba8, binding = 1) uniform image2D u_OutputImage;

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

void main() {
    const ivec2 inputImageSize = textureSize(u_InputImage, 0);
    if(gl_GlobalInvocationID.x >= inputImageSize.x || gl_GlobalInvocationID.y >= inputImageSize.y)
        return;

    vec4 color = texelFetch(u_InputImage, ivec2(gl_GlobalInvocationID.xy), 0);
    imageStore(u_OutputImage, ivec2(gl_GlobalInvocationID).xy, vec4(color.rgb, 1.0));
}