#type compute
#version 460

layout (local_size_x = 8, local_size_y = 8, local_size_z = 8) in;

layout(binding = 0, rgba8) uniform writeonly image3D o_VoxelTexture;
layout(binding = 1) uniform sampler3D u_VoxelTexture;

layout(push_constant) uniform Constants
{
	int SampleMipLevel;
} u_PushConstant;

const ivec3 anisoOffsets[] = ivec3[8]
(
	ivec3(1, 1, 1),
	ivec3(1, 1, 0),
	ivec3(1, 0, 1),
	ivec3(1, 0, 0),
	ivec3(0, 1, 1),
	ivec3(0, 1, 0),
	ivec3(0, 0, 1),
	ivec3(0, 0, 0)

	 //ivec3( 0,  0,  1),  // Z+
	 //ivec3( 0,  0, -1),  // Z-
	 //
	 //ivec3( 0,  1,  0),  // Y+
	 //ivec3( 0, -1,  0),  // Y-
	 //
	 //ivec3( 1,  0,  0),  // X+
	 //ivec3(-1,  0,  0),  // X-
	 //
	 //
	 //ivec3( 1,  1,  1),  // FRONT Y+
	 //ivec3(-1,  1,  1),  // FRONT Y+
	//				    
	 //ivec3( 1, -1,  1),  // FRONT Y-
	 //ivec3(-1, -1,  1),  // FRONT Y-
	//				    
	 //ivec3( 1,  1, -1),  // BEHIND Y+
	 //ivec3(-1,  1, -1),  // BEHIND Y+
	 //
	 //ivec3( 1, -1, -1),  // BEHIND Y-
	 //ivec3(-1, -1, -1)   // BEHIND Y-
);

void FetchTexels(ivec3 pos, int dir, inout vec4 val[8]) 
{
	for(int i = 0; i < 8; i++)
	{
		val[i] = texelFetch(u_VoxelTexture, pos + anisoOffsets[i], 0);

		//if(u_PushConstant.SampleMipLevel == 1)
		//{
		//	if(val[i].r != 0.0f || val[i].g != 0.0f || val[i].b != 0.0f)
		//		val[i].a = 1.0f;
		//}
	}
}

// Interesting
// https://github.com/Hanlin-Zhou/OpenGL-VXGI-Engine/blob/master/shader/MipMap.comp

void main()
{
	ivec3 writePos = ivec3(gl_GlobalInvocationID);
	ivec3 sourcePos = writePos * 2;

	if(u_PushConstant.SampleMipLevel == 0)
	{
		vec4 val = texelFetch(u_VoxelTexture, writePos, 0);

		if(val.r != 0.0f || val.g != 0.0f || val.b != 0.0f)
			val.a = 1.0f;

		imageStore(o_VoxelTexture, writePos, val);

		return;
	}

	vec4 values[8];
	FetchTexels(sourcePos, 0, values);



#if 1

	//imageStore(o_VoxelTexture, writePos, 
	//	(
	//	values[0] + values[4] +
	//	values[1] + values[5] +
	//	values[2] + values[6] +
	//	values[3] + values[7]
	//	) / 8.0f
	//);

	imageStore(o_VoxelTexture, writePos, 
		(
		values[0] + values[4] * (1 - values[0].a) + 
		values[1] + values[5] * (1 - values[1].a) +
		values[2] + values[6] * (1 - values[2].a) +
		values[3] + values[7] * (1 - values[3].a)
		) * 0.25f
	);
#endif



#if 0
	vec4 result = vec4(0.0f);
	float voxelsRendered = 0.0f;

	for(uint i = 0; i < 8; i++)
	{
		if(values[i].a >= 0.0f)
		{
			voxelsRendered++;
			result += values[i];
		}
		//imageStore(o_VoxelTexture, writePos, 
		//	(
		//	values[0] + values[4] * (1 - values[0].a) + 
		//	values[1] + values[5] * (1 - values[1].a) +
		//	values[2] + values[6] * (1 - values[2].a) +
		//	values[3] + values[7] * (1 - values[3].a)
		//	) * 0.25f
		//);
	}

	result /= voxelsRendered;

	imageStore(o_VoxelTexture, writePos, result);
#endif
}