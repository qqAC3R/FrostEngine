#type compute
#version 460

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(binding = 0)       uniform sampler2D         i_DepthTex;
layout(binding = 1)       uniform sampler2D         i_LastVisBuffer;
layout(binding = 2, r32f) uniform writeonly image2D o_VisBuffer;

layout(push_constant) uniform PushConstant
{
	uint   CurrentMipLevel;
	float NearPlane;
	float FarPlane;
} u_PushConstant;

// Linearize z (result is in range [0 .. 1] just like the post-projected z value).
float Linearize(float z)
{
	return (2.0 * u_PushConstant.NearPlane) / (u_PushConstant.FarPlane + u_PushConstant.NearPlane - z * (u_PushConstant.FarPlane - u_PushConstant.NearPlane));
}

float ReadDepth(ivec2 coord, uint level)
{
	return Linearize(texelFetch(i_DepthTex, coord, int(level)).r);
}

void main()
{
	ivec2 coordCoarse = ivec2(gl_GlobalInvocationID.xy);
	ivec2 coordFine = coordCoarse * ivec2(2);

	if(u_PushConstant.CurrentMipLevel == 0)
	{
		imageStore(o_VisBuffer, coordCoarse, vec4(1.0f));
		return;
	}


	uint mipPrevious = u_PushConstant.CurrentMipLevel - 1;


	vec4 fineZ;
	fineZ.x = ReadDepth(coordFine              , mipPrevious);
	fineZ.y = ReadDepth(coordFine + ivec2(1, 0), mipPrevious);
	fineZ.z = ReadDepth(coordFine + ivec2(0, 1), mipPrevious);
	fineZ.w = ReadDepth(coordFine + ivec2(1, 1), mipPrevious);
	

	/* Fetch fine visibility from previous visibility map LOD */
	vec4 visibility;
	visibility.x = texelFetch(i_LastVisBuffer, coordFine              , 0).r;
	visibility.y = texelFetch(i_LastVisBuffer, coordFine + ivec2(1, 0), 0).r;
	visibility.z = texelFetch(i_LastVisBuffer, coordFine + ivec2(0, 1), 0).r;
	visibility.w = texelFetch(i_LastVisBuffer, coordFine + ivec2(1, 1), 0).r;
	
	/* Integrate visibility */
	float maxZ = max(max(fineZ.x, fineZ.y), max(fineZ.z, fineZ.w));
	vec4 integration = (fineZ / maxZ) * visibility;
	
	/* Compute coarse visibility (with SIMD 'dot' intrinsic) */
	float coarseVisibility = dot(vec4(0.25), integration);
	
	// Store the visibility value in the current mip cell
	imageStore(o_VisBuffer, coordCoarse, vec4(coarseVisibility, vec3(1.0f)));
}