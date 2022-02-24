#type vertex
#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

// Instanced vertex buffer
layout(location = 0) in mat4 a_ModelSpaceMatrix;
layout(location = 4) in mat4 a_WorldSpaceMatrix;

struct Vertex
{
	 vec3  Position;
	 vec2  TexCoord;
	 vec3  Normal;
	 vec3  Tangent;
	 vec3  Bitangent;
	 float MaterialIndex;
};

// Using buffer references instead of typical attributes
layout(buffer_reference, scalar) buffer Vertices { Vertex v[]; }; // Positions of an object


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
};
layout(set = 0, binding = 0, scalar) readonly buffer u_MaterialUniform
{
	MaterialData Data[];
} MaterialUniform;

layout(push_constant) uniform Constants
{
	mat4 ViewMatrix;
	uint MaterialIndex;
	uint64_t VertexBufferBDA;
	uint Padding;
} u_PushConstant;

void main()
{
	Vertices verticies = Vertices(u_PushConstant.VertexBufferBDA);
	Vertex vertex = verticies.v[gl_VertexIndex];

	int meshIndex = int(u_PushConstant.MaterialIndex + vertex.MaterialIndex);
	

	//mat4 ModelMatrix = MaterialUniform.Data[nonuniformEXT(meshIndex)].ModelMatrix;
	//mat4 WorldSpaceMatrix = MaterialUniform.Data[nonuniformEXT(meshIndex)].WorldSpaceMatrix;

	// Compute world position
	vec4 worldPos = a_WorldSpaceMatrix * vec4(vertex.Position, 1.0f);

	// Calculating the normals with the model matrix
	mat3 normalMatrix = transpose(inverse(mat3(a_ModelSpaceMatrix)));
	v_Normal = normalMatrix * vertex.Normal;


	// Texture Coords
	v_TexCoord = vertex.TexCoord;

	// World position
	v_FragmentPos = vec3(a_ModelSpaceMatrix * vec4(vertex.Position, 1.0f));
	v_ViewPosition = vec3(u_PushConstant.ViewMatrix * a_ModelSpaceMatrix * vec4(vertex.Position, 1.0f));

	//v_BufferIndex = int(u_PushConstant.MaterialIndex + a_MaterialIndex);
	v_BufferIndex = int(meshIndex);
	v_TextureIndex = int(vertex.MaterialIndex);

	// Calculating the TBN matrix for normal maps
	vec3 N = normalize(v_Normal);
	vec3 T = normalize(vertex.Tangent);
	vec3 B = normalize(vertex.Bitangent);
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
layout(location = 3) out vec4 o_ViewPos;

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

// https://aras-p.info/texts/CompactNormalStorage.html
vec2 EncodeNormals(vec3 n)
{
    float scale = 1.7777f;
    vec2 enc = n.xy / (n.z+1);

    enc /= scale;
    enc = enc*0.5+0.5;
    
	return enc;
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
	
	// View space Position
	o_ViewPos = vec4(v_ViewPosition, 1.0f);

	// Normals
	if(useNormalMap == 1)
		o_Normals = vec4(GetVec3FromNormalMap(u_Textures[nonuniformEXT(normalTextureID)]), 1.0f);
	else
		o_Normals = vec4(v_Normal, 1.0f);

	// Encode normals (from a vec3 to vec2)
	vec2 encodedNormals = EncodeNormals(normalize(vec3(o_Normals)));
	o_Normals = vec4(encodedNormals, 0.0f, 0.0f);


	// Albedo color
	vec3 albedoTextureColor = SampleTexture(albedoTextureID).rgb;
	o_Albedo = vec4(albedoTextureColor * albedoFactor, 1.0f);
	
	// Composite (roughness and metalness)
	float metalness = metalnessFactor * SampleTexture(metalnessTextureID).r;
	float roughness = roughnessFactor * SampleTexture(roughnessTextureID).r;

	o_Normals.z = metalness;
	o_Normals.w = roughness;
}