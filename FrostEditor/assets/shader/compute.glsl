#type compute
#version 450

layout (local_size_x = 32, local_size_y = 32) in;
layout(binding = 0, set = 0, rgba16f) uniform image2D u_Image;

void main()
{
	if(gl_GlobalInvocationID.x % 2 == 0 && gl_GlobalInvocationID.y % 2 == 0)
		imageStore(u_Image, ivec2(gl_GlobalInvocationID.xy), vec4(vec3(0.0f), 1.0f));
	else
		imageStore(u_Image, ivec2(gl_GlobalInvocationID.xy), vec4(1.0f));
}