#type compute
#version 460

#extension GL_EXT_scalar_block_layout : enable

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;


struct MeshDrawCommand
{
	uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int  vertexOffset;
    uint firstInstance;
};
layout(set = 0, binding = 0, scalar) buffer DrawCommands
{
    MeshDrawCommand cmds[];
} u_DrawCommands;



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


layout(set = 0, binding = 2) uniform sampler2D DepthPyramid;
layout(set = 0, binding = 3) buffer DebugBuffer
{
    mat4 Value[];
} u_DebugBuffer;

layout(push_constant) uniform PushConstant
{
    vec4 DepthPyramidRes;
    mat4 ViewMatrix;
    mat4 ProjectionMaxtrix;
    float CamNear;
    float CamFar;
    float Padding1;
    float Padding2;
} u_PushConstant;



void main()
{
	uint idx = gl_GlobalInvocationID.x;

    // If the global invocation is bigger then needed, then just return
    if(idx >= uint(u_PushConstant.DepthPyramidRes.z)) { return; }

    // The object is already frustum culled
    if(u_DrawCommands.cmds[idx].indexCount == 0) { return; }

    // Get the neccesarry data
    MeshSpecifications meshSpec = u_MeshSpecs.specs[idx];



    // Setting up the variables
    vec3 bbmin = vec3(meshSpec.AABB_Min);
    vec3 bbmax = vec3(meshSpec.AABB_Max);
    vec3 bbsize = bbmax - bbmin;
    mat4 modelView = u_PushConstant.ViewMatrix * meshSpec.ModelMatrix;

    // Creating the verticies for the aabb
    const int CORNER_COUNT = 8;
    vec3 corners[CORNER_COUNT] = {
	    bbmin,
	    bbmin + vec3(bbsize.x, 0.f, 0.f),
	    bbmin + vec3(0.f, bbsize.y, 0.f),
	    bbmin + vec3(0.f, 0.f, bbsize.z),
	    bbmin + vec3(bbsize.xy, 0.f),
	    bbmin + vec3(0.f, bbsize.yz),
	    bbmin + vec3(bbsize.x, 0.f, bbsize.z),
	    bbmax
    };

    vec2 ndcMin = vec2(1.0f);
    vec2 ndcMax = vec2(-1.0f);
    float zMin = 1.0f;

    
    for (int i = 0; i < CORNER_COUNT; i ++)
    {
        // Compute the vertex in camera view space
	    vec4 viewPos = modelView * vec4(corners[i], 1.f);

        // Compute the vertex in NDC
	    vec4 clipPos = u_PushConstant.ProjectionMaxtrix * viewPos;
	    vec3 ndcPos = clipPos.xyz / clipPos.w;
        
        // Clip objects behind near plane
	    ndcPos.z *= step(viewPos.z, u_PushConstant.CamNear);

        // Clamping the values
        ndcPos.xy = max(vec2(-1.0f), min(vec2(1.0f), ndcPos.xy));
	    ndcPos.z =  max(       0.f,  min(1.0f,       ndcPos.z));

        // Comparing all the values from the aabb to find min and max
        ndcMin = min(ndcMin, ndcPos.xy);
	    ndcMax = max(ndcMax, ndcPos.xy);
        zMin = min(zMin, ndcPos.z);
    }

    // Calculating the neccesary mip level to be sampled
    vec2 viewport = u_PushConstant.DepthPyramidRes.xy;
    vec2 screenPosMin = (ndcMin * 0.5f + 0.5f) * viewport;
    vec2 screenPosMax = (ndcMax * 0.5f + 0.5f) * viewport;
    vec2 screenRect = (screenPosMax - screenPosMin);
    float screenSize = max(screenRect.x, screenRect.y) - 0.01;

    int mip = int(ceil(log2(screenSize)));


    // In the typical scenario we would sample 1 texel,
    // but for more precise results, we will sample 4 texels (all the aabb vertices in NDC)
    vec2 uvMin = (ndcMin * 0.5f + 0.5f);
    vec2 uvMax = (ndcMax * 0.5f + 0.5f);

    vec2 coords[4] = {
        uvMin,
        vec2(uvMin.x, uvMax.y),
        vec2(uvMax.x, uvMin.y),
        uvMin
    };

    // Sampling the depth pyramid and finding the maximum value of the depth
    float sceneZ = 0.0f;
    for(uint i = 0; i < 4; i++)
    {
        sceneZ = min(sceneZ, textureLod(DepthPyramid, coords[i], mip).g);
    }


    // Now for the result, we check if the `zMin` (being the minimum z values of the current mesh),
    // has lower z value then the sampled depth pyramid (`sceneZ`). If so, then the object is in front, so it shouldn't be culled,
    // otherwise the object will be culled
    if((zMin > sceneZ))
    {
        u_DrawCommands.cmds[idx].indexCount = 0;
        u_DrawCommands.cmds[idx].instanceCount = 1;
        u_DrawCommands.cmds[idx].firstIndex = 0;
        u_DrawCommands.cmds[idx].vertexOffset = 0;
        u_DrawCommands.cmds[idx].firstInstance = 0;
    }
    

    // Debugging
    u_DebugBuffer.Value[idx][0].x = mip;
    u_DebugBuffer.Value[idx][0].y = float(!(zMin <= sceneZ));
    u_DebugBuffer.Value[idx][0].z = zMin;
    u_DebugBuffer.Value[idx][0].w = sceneZ;

    u_DebugBuffer.Value[idx][1].xy = screenPosMin;
    u_DebugBuffer.Value[idx][1].zw = screenPosMax;

    u_DebugBuffer.Value[idx][2].x = float(idx);
    u_DebugBuffer.Value[idx][2].y = screenSize;


}