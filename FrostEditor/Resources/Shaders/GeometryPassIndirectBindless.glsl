#type vertex
#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

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
layout(location = 6) out vec3 v_ViewPosition;
layout(location = 7) out flat int v_BufferIndex;
layout(location = 8) out flat int v_TextureIndex;

struct MaterialData
{
	// PBR values
	vec4 AlbedoColor;
	float Emission;
	float Roughness;
	float Metalness;
	uint UseNormalMap;
			
	// Texture IDs
	uint AlbedoTextureID;
	uint RoughessTextureID;
	uint MetalnessTextureID;
	uint NormalTextureID;

	// Matricies
	mat4 WorldSpaceMatrix;
	mat4 ModelMatrix;
};
layout(set = 0, binding = 0, scalar) readonly buffer u_MaterialUniform
{
	MaterialData Data[16384];
} MaterialUniform;

layout(push_constant) uniform Constants
{
	uint MaterialIndex;
} u_PushConstant;

void main()
{
	int meshIndex = int(u_PushConstant.MaterialIndex + a_MaterialIndex);

	mat4 ModelMatrix = MaterialUniform.Data[nonuniformEXT(meshIndex)].ModelMatrix;
	mat4 WorldSpaceMatrix = MaterialUniform.Data[nonuniformEXT(meshIndex)].WorldSpaceMatrix;


	vec4 worldPos = WorldSpaceMatrix * vec4(a_Position, 1.0);

	// Calculating the normals with the model matrix
	mat3 normalMatrix = transpose(inverse(mat3(ModelMatrix)));
	v_Normal = normalMatrix * a_Normal;


	// Texture Coords
	v_TexCoord = a_TexCoord;

	// World position
	v_FragmentPos = vec3(ModelMatrix * vec4(a_Position, 1.0f));

	//v_BufferIndex = int(u_PushConstant.MaterialIndex + a_MaterialIndex);
	v_BufferIndex = int(meshIndex);
	v_TextureIndex = int(a_MaterialIndex);


	// Calculating the TBN matrix for normal maps
	vec3 N = normalize(v_Normal);
	vec3 T = normalize(a_Tangent);
	vec3 B = normalize(a_Bitangent);
	v_TBN = mat3(T, B, N);
	
	gl_Position = worldPos;
}


#type fragment(Frost_Bindless)
#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) out vec4 o_WoldPos;
layout(location = 1) out vec4 o_Normals;
layout(location = 2) out vec4 o_Albedo;
layout(location = 3) out vec4 o_Composite;
layout(location = 4) out vec4 o_ViewPosition;

layout(location = 0) in vec3 v_FragmentPos;
layout(location = 1) in vec3 v_Normal;
layout(location = 2) in vec2 v_TexCoord;
layout(location = 3) in mat3 v_TBN;
layout(location = 6) in vec3 v_ViewPosition;
layout(location = 7) in flat int v_BufferIndex;
layout(location = 8) in flat int v_TextureIndex;

struct MaterialData
{
	// PBR values
	vec4 AlbedoColor;
	float Emission;
	float Roughness;
	float Metalness;
	uint UseNormalMap;
			
	// Texture IDs
	uint AlbedoTextureID;
	uint RoughessTextureID;
	uint MetalnessTextureID;
	uint NormalTextureID;

	// Matricies
	mat4 WorldSpaceMatrix;
	mat4 ModelMatrix;
};
layout(set = 0, binding = 0, scalar) readonly buffer u_MaterialUniform
{
	MaterialData Data[];
} MaterialUniform;

// Bindless
layout(set = 1, binding = 0) uniform sampler2D u_Textures[];

vec4 SampleTexture(uint textureId)
{
	return texture(u_Textures[nonuniformEXT(textureId)], v_TexCoord);
}

vec3 GetVec3FromNormalMap(sampler2D normalMap)
{
	vec3 tangentNormal = texture(normalMap, v_TexCoord).rgb * 2.0 - 1.0;
	return vec3(v_TBN * tangentNormal);
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
	uint materialIndex = uint(v_BufferIndex);
	
	// PBR textures
	uint albedoTextureID = MaterialUniform.Data[nonuniformEXT(materialIndex)].AlbedoTextureID;
	uint roughnessTextureID = MaterialUniform.Data[nonuniformEXT(materialIndex)].RoughessTextureID;
	uint metalnessTextureID = MaterialUniform.Data[nonuniformEXT(materialIndex)].MetalnessTextureID;

	// Normal map
	uint useNormalMap = MaterialUniform.Data[nonuniformEXT(materialIndex)].UseNormalMap;
	uint normalTextureID = MaterialUniform.Data[nonuniformEXT(materialIndex)].NormalTextureID;

	// PBR values
	vec3 albedoFactor = vec3(MaterialUniform.Data[nonuniformEXT(materialIndex)].AlbedoColor);
	float metalnessFactor = MaterialUniform.Data[nonuniformEXT(materialIndex)].Metalness;
	float roughnessFactor = MaterialUniform.Data[nonuniformEXT(materialIndex)].Roughness;
	float emissionFactor = MaterialUniform.Data[nonuniformEXT(materialIndex)].Emission;

	// World Pos
	o_WoldPos = vec4(v_FragmentPos, 1.0f);
	
	// Normals
	if(useNormalMap == 1)
		o_Normals = vec4(GetVec3FromNormalMap(u_Textures[nonuniformEXT(normalTextureID)]), 1.0f);
	else
		o_Normals = vec4(v_Normal, 1.0f);

	// Albedo color
	vec3 albedoTextureColor = SampleTexture(albedoTextureID).rgb;
	o_Albedo = vec4(albedoTextureColor * albedoFactor, 1.0f);
	
	// Composite (roughness and metalness)
	float metalness = metalnessFactor * SampleTexture(metalnessTextureID).r;
	float roughness = roughnessFactor * SampleTexture(roughnessTextureID).r;
	o_Composite = vec4(metalness, roughness, 1.0f, 1.0f);
	
	o_ViewPosition = vec4(v_ViewPosition, 1.0f);
}