#type compute
#version 460

#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

// Set default precision for floating-point variables to highp
precision highp float;

struct MeshInstancedVertexBuffer
{
	mat4 ModelSpaceMatrix;
	mat4 WorldSpaceMatrix;
	mat4 PreviousWorldSpaceMatrix;
	uint64_t BoneInformationBDA;
	uint MaterialIndexOffset;
	uint EntityID;
	//uint IsMeshVisible;
	//float a_Padding0;
	//float a_Padding1;
	//uint a_Padding1;
	//uint a_Padding2;
	//vec2 a_Vec;
	
};
layout(set = 0, binding = 0) buffer InstancedVertexBuffer
{
    MeshInstancedVertexBuffer meshInstancedVertexBuffer[];
} u_InstancedVertexBuffers;


struct MeshSpecifications
{
    mat4 ModelMatrix;
    vec4 AABB_Min;
    vec4 AABB_Max;
};
layout(set = 0, binding = 1) buffer MeshSpecs
{
    MeshSpecifications specs[];
} u_MeshSpecs;

layout(set = 0, binding = 2) uniform sampler2D u_DepthPyramid;

layout(push_constant) uniform PushConstant
{
	mat4 ProjectionMatrix;
	mat4 ViewMatrix;
	uint NumberOfSubmeshes;
	float CameraNearClip;
} u_PushConstant;

uint is_skybox(vec2 mn, vec2 mx )
{
    return uint(step(dot(mn, mx), 0.0));
}

void main()
{
	uint result = 1;

	uint globalInvocation = gl_GlobalInvocationID.x;

	// The index is higher than the number submehes in the list
	if(globalInvocation > u_PushConstant.NumberOfSubmeshes) return;

	// If the submesh is already culled by frustum culling, we do not need to compute the occlusion culling anymore.
	if(u_InstancedVertexBuffers.meshInstancedVertexBuffer[globalInvocation].ModelSpaceMatrix[3][3] == 0.0) return;

	// Get the neccesarry data
    MeshSpecifications meshSpec = u_MeshSpecs.specs[globalInvocation];


	// Setting up the variables
    vec3 minAABB = vec3(meshSpec.AABB_Min);
    vec3 maxAABB = vec3(meshSpec.AABB_Max);
    vec3 sizeAABB = maxAABB - minAABB;
    //mat4 modelView = u_PushConstant.ViewMatrix * meshSpec.ModelMatrix;


	// Creating the verticies for the aabb
    const int CORNER_COUNT = 8;
    vec3 corners[CORNER_COUNT] = {
	    minAABB,
	    minAABB + vec3(sizeAABB.x, 0.0, 0.0),
	    minAABB + vec3(0.0, sizeAABB.y, 0.0),
	    minAABB + vec3(0.0, 0.0, sizeAABB.z),
	    minAABB + vec3(sizeAABB.xy, 0.0),
	    minAABB + vec3(0.0, sizeAABB.yz),
	    minAABB + vec3(sizeAABB.x, 0.0, sizeAABB.z),
	    maxAABB
    };

	vec2 ndcMin = vec2(1.0);
    vec2 ndcMax = vec2(-1.0);
	float computedZ = 1.0;

	for (int i = 0; i < CORNER_COUNT; i ++)
    {
		// Compute the vertex in camera view space
	    //vec4 viewPos = modelView * vec4(corners[i], 1.0);

		// Compute the vertex in camera view space
	    //vec4 clipPos = u_PushConstant.ViewProjectionMaxtrix * vec4(corners[i], 1.0);
		vec4 viewPos = u_PushConstant.ViewMatrix * meshSpec.ModelMatrix * vec4(corners[i], 1.0);

		vec4 clipPos = u_PushConstant.ProjectionMatrix * viewPos;
		vec3 ndcPos = clipPos.xyz / clipPos.w;

		// clip objects behind near plane
	    ndcPos.z *= step(viewPos.z, u_PushConstant.CameraNearClip);


		// Clamping the values
        ndcPos.xy = clamp(ndcPos.xy, vec2(-1.0), vec2(1.0));
	    ndcPos.z =  clamp(ndcPos.z, 0.0, 1.0);

		// Comparing all the values from the aabb to find min and max
        ndcMin = min(ndcMin, ndcPos.xy);
	    ndcMax = max(ndcMax, ndcPos.xy);
        computedZ = min(computedZ, ndcPos.z);
	}
	result = max(result, is_skybox(ndcMin, ndcMax));

	vec2 uvMin = (ndcMin * 0.5 + 0.5);
    vec2 uvMax = (ndcMax * 0.5 + 0.5);

	// Calculating the neccesary mip level to be sampled
    vec2 viewport = vec2(textureSize(u_DepthPyramid, 0).xy);

	vec2 screenPosMin = uvMin * viewport;
    vec2 screenPosMax = uvMax * viewport;

	vec2 screenRect = (screenPosMax - screenPosMin);
	float screenSize = max(screenRect.x, screenRect.y);


	//int maxMipsPossible = int(floor(log2(max(viewport.x, viewport.y))) + 1);
	//float mip = float(ceil(log2(screenSize))) + 0.01;
	float mip = float(ceil(log2(screenSize)));
	//uvec2 dim = (uvec2(screenPosMax) >> mip) - (uvec2(screenPosMin) >> mip);
	//int use_lower = int(step(float(dim.x), 2.0) * step(float(dim.y), 2.0));
	//mip = use_lower * max(0, mip - 1) + (1 - use_lower) * mip;
	float level_lower = max(mip - 1.0, 0.0);
	vec2 scale = vec2(exp2(-level_lower));
	vec2 a = floor(screenPosMin * scale);
	vec2 b =  ceil(screenPosMax * scale);
	vec2 dims = b - a;

	// Use the lower level if we only touch <= 2 texels in both dimensions
	if (dims.x <= 2.0 && dims.y <= 2.0)
		mip = level_lower;

	//vec2 uv_scale = vec2(uvec2(viewport) >> mip) / viewport / vec2(1024 >> mip);
    //uvMin = screenPosMin * uv_scale;
    //uvMax = screenPosMax * uv_scale;

	// In the typical scenario we would sample 1 texel,
    // but for more precise results, we will sample 4 texels (all the aabb vertices in NDC)
	vec2 coordsAABB[4] = {
        uvMin,
        vec2(uvMin.x, uvMax.y),
        vec2(uvMax.x, uvMin.y),
        uvMax
    };

	// Sampling the depth pyramid and finding the maximum value of the depth
    float sampledDepth = 0.0;
    for(uint i = 0; i < 4; i++)
    {
        sampledDepth = max(sampledDepth, textureLod(u_DepthPyramid, coordsAABB[i], mip).g);
    }
	

	// https://github.com/sydneyzh/gpu_occlusion_culling_vk/blob/gh-gpu-culling/data/shaders/visibility.comp

	// If the manual computed Z coordonate is ??
    //if(computedZ >= sampledDepth)
    {
		// cull occluder
		uint res_occluder = 1 - uint(step(sampledDepth, computedZ));
		//result *= max(1 - consts.use_occlusion_culling, res_occluder);
		result *= res_occluder;
    }
	//u_InstancedVertexBuffers.meshInstancedVertexBuffer[globalInvocation].IsMeshVisible = result;
	//u_InstancedVertexBuffers.meshInstancedVertexBuffer[globalInvocation].ModelSpaceMatrix[3][3] = float(result);


	//u_InstancedVertexBuffers.meshInstancedVertexBuffer[globalInvocation].a_Padding0 = mip;
	//u_InstancedVertexBuffers.meshInstancedVertexBuffer[globalInvocation].a_Vec = screenRect;
	//u_InstancedVertexBuffers.meshInstancedVertexBuffer[globalInvocation].a_Vec.y = sampledDepth;

}