#type vertex
#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3  a_Position;
layout(location = 1) in vec2  a_TexCoord;
layout(location = 2) in vec3  a_Normal;
layout(location = 3) in vec3  a_Tangent;
layout(location = 4) in vec3  a_Bitangent;
layout(location = 5) in float a_MaterialIndex;

layout(location = 0) out vec3 v_FragmentPos;
layout(location = 1) out vec3 v_Normal;
layout(location = 2) out vec2 v_TexCoord;
layout(location = 3) out mat3 v_TBN;

layout(push_constant) uniform Constants
{
	mat4 TransformMatrix;
	mat4 ModelMatrix;
} u_PushConstant;

void main()
{
	vec4 worldPos = u_PushConstant.TransformMatrix * vec4(a_Position, 1.0);

	// Calculating the normals with the model matrix
	mat3 normalMatrix = transpose(inverse(mat3(u_PushConstant.ModelMatrix)));
	v_Normal = normalMatrix * a_Normal;

	// Texture Coords
	v_TexCoord = a_TexCoord;

	// World position
	v_FragmentPos = vec3(u_PushConstant.ModelMatrix * vec4(a_Position, 1.0f));

	// Calculating the TBN matrix for normal maps
	vec3 N = normalize(v_Normal);
	vec3 T = normalize(a_Tangent);
	vec3 B = normalize(a_Bitangent);
	v_TBN = mat3(T, B, N);
	
	gl_Position = worldPos;
}

#type fragment
#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 o_WoldPos;
layout(location = 1) out vec4 o_Normals;
layout(location = 2) out vec4 o_Albedo;
layout(location = 3) out vec4 o_Composite;
layout(location = 4) out vec4 o_EntitiyID;

layout(location = 0) in vec3 v_FragmentPos;
layout(location = 1) in vec3 v_Normal;
layout(location = 2) in vec2 v_TexCoord;
layout(location = 3) in mat3 v_TBN;

layout(set = 0, binding = 0) uniform u_MaterialUniform
{
	vec3 AlbedoColor;
	float Emission;
	float Roughness;
	float Metalness;
	bool UseNormalMap;
} MaterialUniform;

layout(set = 0, binding = 1) uniform sampler2D u_AlbedoTexture;
layout(set = 0, binding = 2) uniform sampler2D u_NormalTexture;
layout(set = 0, binding = 3) uniform sampler2D u_MetalnessTexture;
layout(set = 0, binding = 4) uniform sampler2D u_RoughnessTexture;

vec3 GetVec3FromNormalMap(sampler2D normalMap, vec3 value)
{
	// If the value is 1.0f, then we sample the texture,
	// else we just return the value that was inputted
	if(value.x == 1.0f && value.y == 1.0f && value.z == 1.0f)
	{
		vec3 tangentNormal = texture(normalMap, v_TexCoord).rgb * 2.0 - 1.0;
		return vec3(v_TBN * tangentNormal);
	}
	else
	{
		return value;
	}
}

vec3 GetVec3ValueFromTexture(sampler2D textureSample, vec3 value)
{
	// If the value is 1.0f, then we sample the texture,
	// else we just return the value that was inputted
	if(value.x == 1.0f && value.y == 1.0f && value.z == 1.0f)
		return vec3(texture(textureSample, v_TexCoord));
	else
		return value;
}

float GetFloatValueFromTexture(sampler2D textureSample, float value)
{
	// If the value is 1.0f, then we sample the texture,
	// else we just return the value that was inputted
	if(value == 1.0f)
		return texture(textureSample, v_TexCoord).r;
	else
		return value;
}

void main()
{
	// World Pos
	o_WoldPos = vec4(v_FragmentPos, 1.0f);

	// Normals
	o_Normals = vec4((MaterialUniform.UseNormalMap ? GetVec3FromNormalMap(u_NormalTexture, v_Normal) : v_Normal), 1.0f);

	// Albedo color
	o_Albedo = vec4((texture(u_AlbedoTexture, v_TexCoord).rgb * MaterialUniform.AlbedoColor), 1.0f);
	//o_Albedo = vec4(GetVec3ValueFromTexture(u_AlbedoTexture, MaterialUniform.AlbedoColor), 1.0f);

	// Composite (roughness and metalness)
	float metalness = GetFloatValueFromTexture(u_MetalnessTexture, MaterialUniform.Metalness);
	float roughness = GetFloatValueFromTexture(u_RoughnessTexture, MaterialUniform.Roughness);
	o_Composite = vec4(metalness, roughness, 1.0f, 1.0f);

	//TODO:
	o_EntitiyID = vec4(1.0f);
}