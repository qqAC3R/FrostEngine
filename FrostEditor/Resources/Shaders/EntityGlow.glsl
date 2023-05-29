#type compute
#version 450

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

// `u_CaptureEntityTexture` is used for writting the selected entity into a separate texture (for it to be gaussian blurred)
// and then added back into the final texture using `u_FinalTexture`
layout(binding = 0) uniform writeonly image2D u_CaptureEntityTexture;
layout(binding = 1, rgba8) uniform    image2D u_FinalTexture;

layout(binding = 2) uniform sampler2D u_GaussianBlurTexture;
layout(binding = 3) uniform usampler2D u_EntityIDTexture;


// 0 - Capure the selected entity into a separate texture (for it to be blurred later)
// 1 - Add the gaussian blurred texture into the final texutre
#define MODE_CAPTURE_SELECTED_ENTITY 0
#define MODE_FINAL_ADDITION 1

layout(push_constant) uniform Constants
{
	vec4 SelectedEntityColor;

	uint SelectedEntityID;

	// 0 - Capure the selected entity into a separate texture (for it to be blurred later)
	// 1 - Add the gaussian blurred texture into the final texutre
	uint FilterMode;

	float CosTime;

} u_PushConstant;

void main()
{
	vec2 coord = vec2(gl_GlobalInvocationID.xy) / textureSize(u_GaussianBlurTexture, 0).xy;
	vec4 result = vec4(0.0);

	if(u_PushConstant.SelectedEntityID == 4294967295)
	{
		imageStore(u_CaptureEntityTexture, ivec2(gl_GlobalInvocationID.xy), vec4(vec3(0.0), 1.0));
		return;
	}
	

	switch(u_PushConstant.FilterMode)
	{
		case MODE_CAPTURE_SELECTED_ENTITY:
		{
			uint entityId = texelFetch(u_EntityIDTexture, ivec2(gl_GlobalInvocationID.xy), 0).r;
			if(entityId == u_PushConstant.SelectedEntityID)
			{
				result.rgb = u_PushConstant.SelectedEntityColor.rgb;

				// NOTE: It is mandatory to set the alpha channel to 1.0 for later uses
				result.a = 1.0;
			}

			imageStore(u_CaptureEntityTexture, ivec2(gl_GlobalInvocationID.xy), result);
			break;
		}
		case MODE_FINAL_ADDITION:
		{
			vec4 gaussianBlurredTex = texture(u_GaussianBlurTexture, coord).rgba;
			result = imageLoad(u_FinalTexture, ivec2(gl_GlobalInvocationID.xy));
			uint entityId = texture(u_EntityIDTexture, coord).r;
			

			// If alpha channel is 0, then we should add it to the texture because it is part of the edge of the entity
			// If alpha channel was 1, then we shouldn't add it to the texture because the coord is part of the entity itself, and not the edge
			if(u_PushConstant.SelectedEntityID != entityId)
			{
				result.rgb += (gaussianBlurredTex.rgb) * max((1.0 - pow(cos(u_PushConstant.CosTime * 3.0), 4)), 0.4);// * cos(u_PushConstant.CosTime);
				//result.rgb = (gaussianBlurredTex.rgb);// * cos(u_PushConstant.CosTime);
				//result.a = 1.0;
			}

			imageStore(u_FinalTexture, ivec2(gl_GlobalInvocationID.xy), result);
			break;
		}
	}

	//imageStore(u_FinalTexture, ivec2(gl_GlobalInvocationID.xy), result);
}