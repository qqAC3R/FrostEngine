#type vertex
#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 a_Position;

layout(push_constant) uniform Constants
{
	mat4 ProjectionMatrix;
	mat4 ViewMatrix;
} u_PushConstant;

layout(location = 0) out vec3 v_FragmentPos;

void main()
{
	v_FragmentPos = a_Position;


	// Remove the translation from the view matrix
	mat4 vkProjectionMatrix = u_PushConstant.ProjectionMatrix;
	vkProjectionMatrix[1][1] *= -1;

	mat4 modViewProjection = mat4(mat3(u_PushConstant.ViewMatrix));
	vec4 clipPosition = vkProjectionMatrix * modViewProjection * vec4(a_Position, 1.0f);
	
	gl_Position = clipPosition.xyww;
}

#type fragment
#version 450
#extension GL_ARB_separate_shader_objects : enable

// Output
layout(location = 0) out vec4 o_Color;

// Input
layout(location = 0) in vec3 v_FragmentPos;

// Uniforms
layout(binding = 0) uniform samplerCube u_EnvTexture;
layout(binding = 1) uniform CameraData {
	float Gamma;
	float Exposure;
	float Lod;
} m_CameraData;

// From http://filmicworlds.com/blog/filmic-tonemapping-operators/
vec3 Uncharted2Tonemap(vec3 color)
{
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	float W = 11.2;
	return ((color*(A*color+C*B)+D*E)/(color*(A*color+B)+D*F))-E/F;
}

// From https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
vec3 AcesApprox(vec3 v)
{
    v *= 0.6f;
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((v*(a*v+b))/(v*(c*v+d)+e), 0.0f, 1.0f);
}

void main()
{
	// Getting the color from the cubemap	
	vec3 envColor = textureLod(u_EnvTexture, v_FragmentPos, m_CameraData.Lod).rgb;

	// Tone mapping
	vec3 color = AcesApprox(envColor * m_CameraData.Exposure);
	//vec3 color = Uncharted2Tonemap(envColor * m_CameraData.Exposure);
	//color = color * (1.0f / Uncharted2Tonemap(vec3(11.2f)));	
	
	// Gamma correction
	color = pow(color, vec3(1.0f / m_CameraData.Gamma));
	
	// Outputting the color
	o_Color = vec4(color, 1.0);
}