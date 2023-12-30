#type compute
#version 460 core

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(binding = 0) uniform sampler2D u_AOTexture;
layout(binding = 1) uniform sampler2D u_AccumulationBeforeTexture;
layout(binding = 2, r32f) uniform writeonly image2D o_AccumulationFinalTexture;

layout(binding = 3) uniform sampler2D u_CurrentDepthBuffer;
layout(binding = 4) uniform sampler2D u_PreviousDepthBuffer;


layout(binding = 5) uniform UniformBuffer
{
	//mat4 InvViewProjMatrix;
	//mat4 PreviousViewProjMatrix;
	mat4 CurrentInvViewProjMatrix;

	mat4 PreviousViewProjMatrix;
	mat4 PreviousInvViewProjMatrix;

	float CameraNearClip;
	float CameraFarClip;

} u_UniformBuffer;

float LinearizeDepth(float depth)
{
   return u_UniformBuffer.CameraNearClip * u_UniformBuffer.CameraFarClip / (u_UniformBuffer.CameraFarClip + depth * (u_UniformBuffer.CameraFarClip - u_UniformBuffer.CameraNearClip));
}

vec3 ComputeWorldPosition(vec2 coords, float depth, mat4 invViewProjMatrix)
{
	//ivec2 invoke = ivec2(gl_GlobalInvocationID.xy);
	//vec2 coords = (vec2(invoke) + 0.5.xx) / vec2(imageSize(u_ShadowTextureOutput).xy);
	//depth = LinearizeDepth(depth);
	
	vec4 clipSpacePosition = vec4(coords * 2.0 - 1.0, depth, 1.0);
	vec4 viewSpacePosition = invViewProjMatrix * clipSpacePosition;
	
    viewSpacePosition /= viewSpacePosition.w; // Perspective division
	return vec3(viewSpacePosition);
}

vec2 ComputePreviousScreenPosition(vec3 currentPosition, mat4 previousViewProjMatrix)
{
	vec4 previousWorldPos = previousViewProjMatrix * vec4(currentPosition, 1.0);
	vec3 previousPosNDC = previousWorldPos.xyz / previousWorldPos.w;
	vec2 previousUV = 0.5 * previousPosNDC.xy + 0.5.xx;
	return previousUV;
}

void main()
{
	ivec2 loc = ivec2(gl_GlobalInvocationID.xy);
	ivec2 imgSize = imageSize(o_AccumulationFinalTexture).xy;

	vec2 currentUV = (vec2(loc) + 0.5.xx) / vec2(imgSize);
	
	//vec2 currentScreenPos = s_UV * 2.0 - vec2(1.0);

	// Unproject to current frame world space
	float currentDepth = texture(u_CurrentDepthBuffer, currentUV).r;

	if(currentDepth == 1.0)
	{
		imageStore(o_AccumulationFinalTexture, ivec2(gl_GlobalInvocationID.xy), vec4(1.0));
	}

	vec3 currentPosition = ComputeWorldPosition(currentUV, currentDepth, u_UniformBuffer.CurrentInvViewProjMatrix);


	// Reproject to previous frame screen space
	vec2 previousUV = ComputePreviousScreenPosition(currentPosition, u_UniformBuffer.PreviousViewProjMatrix);
	float previousDepth = texture(u_PreviousDepthBuffer, previousUV).r;
	vec3 previousPosition = ComputeWorldPosition(previousUV, previousDepth, u_UniformBuffer.PreviousInvViewProjMatrix);


	// Detect disocclusion
	float disoclussion = dot(currentPosition - previousPosition, currentPosition - previousPosition);
	
	// Fetch values
	float currentAO = texture(u_AOTexture, currentUV).r;
	vec2 accumulationAO = texture(u_AccumulationBeforeTexture, previousUV).rg;

	float currentN = 0.0;
	float previousN = accumulationAO.y * 6.0;
	
	float finalAO = 0.0;

	float mixFactor = 0.6;
	finalAO = mix(accumulationAO.x, currentAO.x, mixFactor);

	imageStore(o_AccumulationFinalTexture, ivec2(gl_GlobalInvocationID.xy), vec4(finalAO, 0.0, 1.0, 1.0));
}