#type compute
#version 460
#extension GL_EXT_scalar_block_layout : enable

#define TILE_SIZE 16
layout(local_size_x = TILE_SIZE, local_size_y = TILE_SIZE, local_size_z = 1) in;

struct PointLight
{
    vec3 Radiance;
	float Intensity;
    float Radius;
    float Falloff;
    vec3 Position;
};

layout(binding = 0, scalar) readonly buffer u_LightData { // Using scalar, it will fix our byte padding issues?
	PointLight u_PointLights[];
} LightData;

layout(binding = 1) writeonly buffer u_VisibleLightsBuffer {
	int Indices[];
} LightIndices;


layout(binding = 2, scalar) readonly buffer u_RendererData { // Using scalar, it will fix our byte padding issues?
    vec2 ScreenSize;
    int PointLightCount;
    int Padding;

    mat4 ViewMatrix;
    mat4 ProjectionMatrix;
    mat4 ViewProjectionMatrix;
} RendererData;

layout(binding = 3) uniform sampler2D u_DepthBuffer;



// Shared values between all the threads in the current work group
shared uint minDepthInt;
shared uint maxDepthInt;
shared uint visibleLightCount;
shared vec4 frustumPlanes[6];

// Shared thread local storage(TLS) for visible indices, will be written out to the global buffer(u_VisibleLightsBuffer) at the end.
shared int visibleLightIndices[1024];


void main()
{
    // Locations
    ivec2 location = ivec2(gl_GlobalInvocationID.xy);  // Global shader invocation location across the Dispatch call
    ivec2 itemID = ivec2(gl_LocalInvocationID.xy);     // Local shader invocation location inside the workgroup
    ivec2 tileID = ivec2(gl_WorkGroupID.xy);
    ivec2 tileNumber = ivec2(gl_NumWorkGroups.xy);
    uint index = tileID.y * tileNumber.x + tileID.x;


    // Initialize shared global values for depth and light count at the first invocation of this workgroup(only done at first thread invocation)
    if (gl_LocalInvocationIndex == 0)
    {
        minDepthInt = 0xFFFFFFFF;
        maxDepthInt = 0;
        visibleLightCount = 0;
    }
    barrier();

    // Calculate the minimum and maximum depth values (from the depth buffer) for this group's tile
    vec2 textureCoord = vec2(location) / RendererData.ScreenSize;
    float depth = texture(u_DepthBuffer, textureCoord).x;
    depth = RendererData.ProjectionMatrix[3][2] / (depth + RendererData.ProjectionMatrix[2][2]);


    // Convert depth to uint so we can do atomic min and max comparisons between the threads
    uint depthInt = floatBitsToUint(depth);
    atomicMin(minDepthInt, depthInt);
    atomicMax(maxDepthInt, depthInt);
    barrier();



    // One thread should calculate the frustum planes to be used for this tile (only done at first thread invocation)
    if (gl_LocalInvocationIndex == 0)
    {
        // Convert the min and max across the entire tile back to float
        float minDepth = uintBitsToFloat(minDepthInt);
        float maxDepth = uintBitsToFloat(maxDepthInt);

        // Steps based on tile sale
        vec2 negativeStep = (2.0 * vec2(tileID)) / vec2(tileNumber);
        vec2 positiveStep = (2.0 * vec2(tileID + ivec2(1, 1))) / vec2(tileNumber);


        // Set up starting values for planes using steps and min and max z values
        frustumPlanes[0] = vec4( 1.0,  0.0,  0.0,  1.0 - negativeStep.x); // Left
        frustumPlanes[1] = vec4(-1.0,  0.0,  0.0, -1.0 + positiveStep.x); // Right
        frustumPlanes[2] = vec4( 0.0,  1.0,  0.0,  1.0 - negativeStep.y); // Bottom
        frustumPlanes[3] = vec4( 0.0, -1.0,  0.0, -1.0 + positiveStep.y); // Top
        frustumPlanes[4] = vec4( 0.0,  0.0, -1.0, -minDepth); // Near
        frustumPlanes[5] = vec4( 0.0,  0.0,  1.0,  maxDepth); // Far


        // Transform the first four planes to the camera's perspective
        for (uint i = 0; i < 4; i++)
        {
            frustumPlanes[i] *= RendererData.ViewProjectionMatrix;
            frustumPlanes[i] /= length(frustumPlanes[i].xyz);
        }

        // Transform the depth planes
        frustumPlanes[4] *= RendererData.ViewMatrix;
        frustumPlanes[4] /= length(frustumPlanes[4].xyz);
        frustumPlanes[5] *= RendererData.ViewMatrix;
        frustumPlanes[5] /= length(frustumPlanes[5].xyz);
    }
    barrier();



    // Cull lights
    // Parallelize the threads against the lights now
    // Can handle 256(TILE_SIZE * TILE_SIZE) simultaniously
    uint threadCount = TILE_SIZE * TILE_SIZE;
    uint passCount = (RendererData.PointLightCount + threadCount - 1) / threadCount;
    for (uint i = 0; i < passCount; i++)
    {
        uint lightIndex = i * threadCount + gl_LocalInvocationIndex;
        if(lightIndex >= RendererData.PointLightCount)
            break;

        vec4 position = vec4(LightData.u_PointLights[lightIndex].Position, 1.0f);
        float radius = LightData.u_PointLights[lightIndex].Radius;
        //radius += radius * 0.3;

        // Check if light radius is in frustums
        float distance = 0.0;
        for (uint j = 0; j < 6; j++)
        {
            distance = dot(position, frustumPlanes[j]) + radius;

            if (distance <= 0.0) // No intersection
                break;
        }


        if (distance > 0.0)
        {
            // Add index to the shared array of visible indices
            uint offset = atomicAdd(visibleLightCount, 1);
            visibleLightIndices[offset] = int(lightIndex); // Add to Thread Local Storage(TLS)
        }
    }
    barrier();


    // One thread should fill the global light buffer
    if (gl_LocalInvocationIndex == 0)
    {
        uint offset = index * 1024; // Determine position in global buffer

        for (uint i = 0; i < visibleLightCount; i++)
        {
            LightIndices.Indices[offset + i] = visibleLightIndices[i];
        }
        
        if (visibleLightCount != 1024)
        {
            // Unless we have totally filled the entire array, mark it's end with -1
            // Final shader step will use this to determine where to stop (without having to pass the light count)
            LightIndices.Indices[offset + visibleLightCount] = -1;
        }
    }
}