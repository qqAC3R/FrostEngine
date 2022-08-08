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

struct AnimatedVertex
{
	vec3  Position;
	vec2  TexCoord;
	vec3  Normal;
	vec3  Tangent;
	vec3  Bitangent;
	
	float MaterialIndex;

	ivec4 IDs;
	vec4  Weights;
};

// Using buffer references instead of typical attributes
layout(buffer_reference, scalar) buffer Vertices { Vertex v[]; }; // Vertex information of an submesh
layout(buffer_reference, scalar) buffer AnimatedVertices { AnimatedVertex v[]; }; // Animated vertex information of an submesh
layout(buffer_reference, scalar) buffer MeshBoneInformation { mat4 BoneTransforms[]; }; // Animated vertex information of an submesh

layout(location = 0) out vec3 v_FragmentPos;
layout(location = 1) out vec2 v_TexCoord;
layout(location = 2) out vec3 v_Normal;
layout(location = 3) out vec3 v_Tangent;
layout(location = 4) out vec3 v_ViewPosition;
layout(location = 5) out flat int v_BufferIndex;
layout(location = 6) out flat int v_TextureIndex;

layout(push_constant) uniform Constants
{
	mat4 ViewMatrix;
	uint64_t VertexBufferBDA;
	uint64_t BoneInformationBDA;
	uint MaterialIndex;
	uint IsAnimated;
} u_PushConstant;

void main()
{
	vec3 position, normal, tangent;
	vec2 texCoord;
	float materialIndex;

	// If the mesh is animated, then compute the bone transform matrix
	mat4 boneTransform = mat4(1.0);

	if(u_PushConstant.IsAnimated == 1)
	{
		AnimatedVertices animatedVerticies = AnimatedVertices(u_PushConstant.VertexBufferBDA);
		MeshBoneInformation boneInfo = MeshBoneInformation(u_PushConstant.BoneInformationBDA);
		AnimatedVertex vertex = animatedVerticies.v[gl_VertexIndex];

		position = vertex.Position;
		normal = vertex.Normal;
		tangent = vertex.Tangent;
		texCoord = vertex.TexCoord;
		materialIndex = vertex.MaterialIndex;

		boneTransform  = boneInfo.BoneTransforms[vertex.IDs[0]] * vertex.Weights[0];
		boneTransform += boneInfo.BoneTransforms[vertex.IDs[1]] * vertex.Weights[1];
		boneTransform += boneInfo.BoneTransforms[vertex.IDs[2]] * vertex.Weights[2];
		boneTransform += boneInfo.BoneTransforms[vertex.IDs[3]] * vertex.Weights[3];
	}
	else
	{
		Vertices verticies = Vertices(u_PushConstant.VertexBufferBDA);
		Vertex vertex = verticies.v[gl_VertexIndex];

		position = vertex.Position;
		normal = vertex.Normal;
		tangent = vertex.Tangent;
		texCoord = vertex.TexCoord;
		materialIndex = vertex.MaterialIndex;
	}
	
	// Calculating the normals with the model matrix
	mat3 normalMatrix = mat3(boneTransform * a_ModelSpaceMatrix);
	v_Normal = normalMatrix * normalize(normal);
	v_Tangent = normalMatrix * normalize(tangent);
	//mat3 normalMatrix = transpose(inverse(mat3(a_ModelSpaceMatrix)));


	// Texture Coords
	v_TexCoord = texCoord;


	// World position
	v_FragmentPos = vec3(a_ModelSpaceMatrix * boneTransform * vec4(position, 1.0f));
	v_ViewPosition = vec3(u_PushConstant.ViewMatrix * a_ModelSpaceMatrix * boneTransform * vec4(position, 1.0f));

	// Material indices
	int meshIndex = int(u_PushConstant.MaterialIndex + materialIndex);
	v_BufferIndex = int(meshIndex);
	v_TextureIndex = int(materialIndex);

	// Compute world position
	vec4 worldPos = a_WorldSpaceMatrix * boneTransform * vec4(position, 1.0f);
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
layout(location = 1) in vec2 v_TexCoord;
layout(location = 2) in vec3 v_Normal;
layout(location = 3) in vec3 v_Tangent;
layout(location = 4) in vec3 v_ViewPosition;
layout(location = 5) in flat int v_BufferIndex;
layout(location = 6) in flat int v_TextureIndex;

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
layout(set = 0, binding = 0) readonly buffer u_MaterialUniform
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
	vec3 tangentNormal = normalize(texture(normalMap, v_TexCoord).xyz * 2.0 - 1.0);

	vec3 T = normalize(v_Tangent);
	vec3 N = normalize(v_Normal);
	vec3 B = normalize(cross(N, T));

	mat3 TBN = mat3(T, B, N);
	vec3 normal = normalize(vec3(TBN * tangentNormal));

	normal = clamp(normal, vec3(-1.0f), vec3(1.0f));

	return normal;
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

// Fast octahedron normal vector encoding.
// https://jcgt.org/published/0003/02/01/
vec2 SignNotZero(vec2 v)
{
	return vec2((v.x >= 0.0) ? +1.0 : -1.0, (v.y >= 0.0) ? +1.0 : -1.0);
}
// Assume normalized input. Output is on [-1, 1] for each component.
vec2 EncodeNormal(vec3 v)
{
	// Project the sphere onto the octahedron, and then onto the xy plane
	vec2 p = v.xy * (1.0 / (abs(v.x) + abs(v.y) + abs(v.z)));
	// Reflect the folds of the lower hemisphere over the diagonals
	return (v.z <= 0.0) ? ((1.0 - abs(p.yx)) * SignNotZero(p)) : p;
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
	vec2 encodedNormals = EncodeNormal(normalize(vec3(o_Normals)));
	o_Normals = vec4(encodedNormals, 0.0f, 0.0f);
	

	// Albedo color
	vec3 albedoTextureColor = SampleTexture(albedoTextureID).rgb;
	o_Albedo = vec4(albedoTextureColor * albedoFactor + (emissionFactor), 1.0f);
	
	// Composite (roughness and metalness)
	float metalness = metalnessFactor * SampleTexture(metalnessTextureID).r;
	float roughness = roughnessFactor * SampleTexture(roughnessTextureID).r;

	o_Normals.z = metalness;
	o_Normals.w = roughness;
}