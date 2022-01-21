#type compute
#version 460

#extension GL_EXT_scalar_block_layout : enable

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

#define INSTANCE_COUNT 16834

struct MeshDrawCommand
{
	uint        indexCount;
    uint        instanceCount;
    uint        firstIndex;
    int         vertexOffset;
    uint        firstInstance;
};

layout(set = 0, binding = 0, scalar) buffer DrawCommands
{
    MeshDrawCommand cmds[INSTANCE_COUNT];
} u_DrawCommands;

struct MeshSpecifications
{
    mat4 Transform;
    vec4 AABB_Min;
    vec4 AABB_Max;
};

layout(set = 0, binding = 1) buffer MeshSpecs
{
    MeshSpecifications specs[INSTANCE_COUNT];
} u_MeshSpecs;

layout(set = 0, binding = 2) uniform sampler2D DepthPyramid;
layout(set = 0, binding = 3) buffer DebugBuffer
{
    vec4 Value1;
    vec4 Value2;
} u_DebugBuffer;

layout(push_constant) uniform PushConstant
{
    vec4 DepthPyramidRes;
    mat4 ViewProjectionMaxtrix;
} u_PushConstant;


/*
vec4 CalculateClipSpaceAABB(MeshSpecifications meshSpec, out float aabbClipDepth)
{
    vec4 finalAABB;

    vec4 clip_AABBMin = u_PushConstant.ViewProjectionMaxtrix * meshSpec.Transform * meshSpec.AABB_Min; // Clip-space position.
    vec3 ndc_AABBMin = clip_AABBMin.xyz / clip_AABBMin.w; // Normalize the coords [-1, 1.0]
    ndc_AABBMin = 0.5f * ndc_AABBMin + 0.5f; // Normalize the coords [0, 1.0]

    vec4 clip_AABBMax = u_PushConstant.ViewProjectionMaxtrix * meshSpec.Transform * meshSpec.AABB_Max; // Clip-space position.
    vec3 ndc_AABBMax = clip_AABBMax.xyz / clip_AABBMax.w; // Normalize the coords [-1, 1.0]
    ndc_AABBMax = 0.5f * ndc_AABBMax + 0.5f; // Normalize the coords [0, 1.0]

    vec3 aabbCenter = (ndc_AABBMin * ndc_AABBMax) * 0.5f;
    aabbClipDepth = aabbCenter.z;

    finalAABB.xy = ndc_AABBMin.xy;
    finalAABB.zw = ndc_AABBMax.xy;

    return finalAABB;
}
*/

void main()
{
	uint di = gl_GlobalInvocationID.x;

    if(di >= uint(u_PushConstant.DepthPyramidRes.z)) { return; }

    MeshSpecifications meshSpec = u_MeshSpecs.specs[di];

    float meshAvgDepth;



    ///////////////////
    vec4 aabb;

    // Calculating the min aabb in NDC
    vec4 clip_AABBMin = u_PushConstant.ViewProjectionMaxtrix * meshSpec.Transform * meshSpec.AABB_Min; // Clip-space position.
    vec3 ndc_AABBMin = clip_AABBMin.xyz / clip_AABBMin.w; // Normalize the coords [-1, 1.0]
    //ndc_AABBMin = 0.5f * ndc_AABBMin + 0.5f; // Normalize the coords [0, 1.0]


    // Calculating the max aabb in NDC
    vec4 clip_AABBMax = u_PushConstant.ViewProjectionMaxtrix * meshSpec.Transform * meshSpec.AABB_Max; // Clip-space position.
    vec3 ndc_AABBMax = clip_AABBMax.xyz / clip_AABBMax.w; // Normalize the coords [-1, 1.0]
    //ndc_AABBMax = 0.5f * ndc_AABBMax + 0.5f; // Normalize the coords [0, 1.0]



    vec3 ndc_AABBCenter;

    // Calculating the center of aabb in NDC
    {
        vec3 aabbCenter = vec3((meshSpec.AABB_Max + meshSpec.AABB_Min) * 0.5f);

        vec4 clip_AABBCenter = u_PushConstant.ViewProjectionMaxtrix * meshSpec.Transform * vec4(aabbCenter, 1.0f); // Clip-space position.
        ndc_AABBCenter = clip_AABBCenter.xyz / clip_AABBCenter.w; // Normalize the coords [-1, 1.0]
        //ndc_AABBCenter = 0.5f * ndc_AABBCenter + 0.5f; // Normalize the coords [0, 1.0]

        meshAvgDepth = ndc_AABBCenter.z;
    }

    aabb.xy = ndc_AABBMin.xy;
    aabb.zw = ndc_AABBMax.xy;
    ///////////////////



    float width = abs(aabb.x - aabb.z) * u_PushConstant.DepthPyramidRes.x;
    float height = abs(aabb.y - aabb.w) * u_PushConstant.DepthPyramidRes.y;

    float level = ceil(log2(max(width, height))); //TODO:  +1???
    

    vec2 aabbCenter = (aabb.xy + aabb.zw) * 0.5;
    float depth = textureLod(DepthPyramid, aabbCenter, level).x;
    
    
    if(meshAvgDepth > depth)
    {
        u_DrawCommands.cmds[di].indexCount = 0;
        u_DrawCommands.cmds[di].instanceCount = 1;
        u_DrawCommands.cmds[di].firstIndex = 0;
        u_DrawCommands.cmds[di].vertexOffset = 0;
        u_DrawCommands.cmds[di].firstInstance = 0;
    }
    
    u_DebugBuffer.Value1.xy = aabbCenter;
    u_DebugBuffer.Value1.z = level;

    u_DebugBuffer.Value2.xyzw = aabb;


    //u_DebugBuffer.Values.x = meshAvgDepth;
    //u_DebugBuffer.Values.y = depth;
    //u_DebugBuffer.Values.z = level;
    //u_DebugBuffer.Values.w = (aabb.xy + aabb.zw) * 0.5;

    //u_DebugBuffer.Values.xy = ndc_AABBCenter.xy;
    //u_DebugBuffer.Values.zw = aabbClipSpace_Center.xy;


    //u_DebugBuffer.Values.x = aabb.x * u_PushConstant.DepthPyramidRes.x;
    //u_DebugBuffer.Values.y = aabb.y * u_PushConstant.DepthPyramidRes.y;
    //u_DebugBuffer.Values.z = aabb.z * u_PushConstant.DepthPyramidRes.x;
    //u_DebugBuffer.Values.w = aabb.w * u_PushConstant.DepthPyramidRes.y;
}