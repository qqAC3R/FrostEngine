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
layout(location = 8) in mat4 a_PreviousWorldSpaceMatrix;
layout(location = 12) in uint64_t a_BoneInformationBDA;
layout(location = 13) in uint a_MaterialIndexGlobalOffset;
layout(location = 14) in uint a_EntityID;

//layout(location = 14) in uint a_IsMeshVisible;
//layout(location = 15) in uint a_Padding0;
//
//layout(location = 16) in uint a_Padding1;
//layout(location = 17) in uint a_Padding2;
//layout(location = 18) in uint64_t a_BoneInformationBDA;

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

//layout(location = 0) out vec3 v_FragmentPos;
layout(location = 0) out vec2 v_TexCoord;
layout(location = 1) out vec3 v_Normal;
layout(location = 2) out vec3 v_Tangent;
layout(location = 3) out vec3 v_ViewPosition;
layout(location = 4) out vec3 v_CurrentPosition;
layout(location = 5) out vec3 v_PreviousPosition;
layout(location = 6) out flat int v_BufferIndex;
layout(location = 7) out flat int v_TextureIndex;
layout(location = 8) out flat uint v_EntityID;
layout(location = 9) out vec3 v_Color1;

layout(push_constant) uniform Constants
{
	mat4 ViewMatrix;
	vec2 JitterCurrent;
	vec2 JitterPrevious;
	uint64_t VertexBufferBDA;
	uint IsAnimated;
} u_PushConstant;

void main()
{
	v_Color1 = vec3(1.0);
	if(a_ModelSpaceMatrix[3][3] == 0.0)
	{
		gl_Position = vec4(1.0, 1.0, 1.0, 0.0);
		return;
		//v_Color1 = vec3(0.8, 0.2, 0.2);
	}
	else
	{


		mat4 modelSpaceMatrix = a_ModelSpaceMatrix;
		modelSpaceMatrix[3][3] = 1.0;

		vec3 position, normal, tangent;
		vec2 texCoord;
		float materialIndex;

		// If the mesh is animated, then compute the bone transform matrix
		mat4 boneTransform = mat4(1.0);

		if(u_PushConstant.IsAnimated == 1)
		{
			AnimatedVertices animatedVerticies = AnimatedVertices(u_PushConstant.VertexBufferBDA);
			MeshBoneInformation boneInfo = MeshBoneInformation(a_BoneInformationBDA);
			AnimatedVertex vertex = animatedVerticies.v[gl_VertexIndex];

			position = vertex.Position;
			normal = vertex.Normal;
			tangent = vertex.Tangent;
			texCoord = vertex.TexCoord;
			materialIndex = vertex.MaterialIndex;

			if(boneInfo.BoneTransforms[vertex.IDs[0]] [0][0] != 3.402823466e+38f)
			{
				boneTransform  = boneInfo.BoneTransforms[vertex.IDs[0]] * vertex.Weights[0];
				boneTransform += boneInfo.BoneTransforms[vertex.IDs[1]] * vertex.Weights[1];
				boneTransform += boneInfo.BoneTransforms[vertex.IDs[2]] * vertex.Weights[2];
				boneTransform += boneInfo.BoneTransforms[vertex.IDs[3]] * vertex.Weights[3];
			}
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
		mat3 normalMatrix = mat3(boneTransform * modelSpaceMatrix);
		v_Normal = normalMatrix * normalize(normal);
		v_Tangent = normalMatrix * normalize(tangent);

		// Texture Coords
		v_TexCoord = texCoord;


		// World position
		vec3 positionWithBoneTransform = vec3(boneTransform * vec4(position, 1.0f));
		v_ViewPosition = vec3(u_PushConstant.ViewMatrix * modelSpaceMatrix * vec4(positionWithBoneTransform, 1.0f));

		// Material indices
		int meshIndex = int(a_MaterialIndexGlobalOffset + materialIndex);
		v_BufferIndex = int(meshIndex);
		v_TextureIndex = int(materialIndex);
		v_EntityID = a_EntityID;

		// Compute last frame's world position
		v_PreviousPosition = (a_PreviousWorldSpaceMatrix * vec4(positionWithBoneTransform, 1.0f)).xyw;

		// Compute world position
		vec4 worldPos = a_WorldSpaceMatrix * vec4(positionWithBoneTransform, 1.0f);
		gl_Position = worldPos;
		v_CurrentPosition = gl_Position.xyw;
	}
}


#type fragment(Frost_Bindless)
#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_scalar_block_layout : enable

//layout(location = 0) out vec4 o_WoldPos;
layout(location = 0) out vec4 o_Normals;
layout(location = 1) out vec4 o_Albedo;
layout(location = 2) out vec4 o_ViewPos;
layout(location = 3) out uint o_MeshID;
layout(location = 4) out vec2 o_Velocity;

//layout(location = 0) in vec3 v_FragmentPos;
layout(location = 0) in vec2 v_TexCoord;
layout(location = 1) in vec3 v_Normal;
layout(location = 2) in vec3 v_Tangent;
layout(location = 3) in vec3 v_ViewPosition;
layout(location = 4) in vec3 v_CurrentPosition;
layout(location = 5) in vec3 v_PreviousPosition;
layout(location = 6) in flat int v_BufferIndex;
layout(location = 7) in flat int v_TextureIndex;
layout(location = 8) in flat uint v_EntityID;
layout(location = 9) in vec3 v_Color1;


struct MaterialData
{
	// PBR values
	vec4 AlbedoColor;
	float Emission;
	float Roughness;
	float Metalness;
	uint UseNormalMap; // 0 - does not use normal map; 1 - use normal maps; 2 - use normal maps with compression and blue channel should be computed
			
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

layout(push_constant) uniform Constants
{
	mat4 ViewMatrix;
	vec2 JitterCurrent;
	vec2 JitterPrevious;
	uint64_t VertexBufferBDA;
	uint IsAnimated;
} u_PushConstant;


vec4 SampleTexture(uint textureId)
{
	return texture(u_Textures[nonuniformEXT(textureId)], vec2(v_TexCoord.x, 1.0 - v_TexCoord.y));
}

vec3 GetVec3FromNormalMap(sampler2D normalMap, uint useNormalMapCompression)
{
	vec3 tangentNormal = (texture(normalMap, v_TexCoord).xyz * 2.0 - 1.0);

	if(useNormalMapCompression == 2)
		tangentNormal.z = clamp(sqrt(max(1.0 - tangentNormal.x * tangentNormal.x - tangentNormal.y * tangentNormal.y, 0.0)), 0.0, 1.0);

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
	if(value == vec3(1.0))
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

	// Albedo color
	vec4 albedoTextureColor = SampleTexture(albedoTextureID).rgba;
	o_Albedo = vec4(albedoTextureColor.rgb * albedoFactor, 1.0);
	o_Albedo.a = emissionFactor;
	o_Albedo.rgb *= v_Color1;

	// https://stackoverflow.com/questions/8509051/is-discard-bad-for-program-performance-in-opengl
	if(albedoTextureColor.a < 0.5)
		discard;

	
	// View space Position
	o_ViewPos = vec4(v_ViewPosition, 1.0f);

	// Normals
	if(useNormalMap >= 1)
		o_Normals = vec4(GetVec3FromNormalMap(u_Textures[nonuniformEXT(normalTextureID)], useNormalMap), 1.0f);
	else
		o_Normals = vec4(v_Normal, 1.0f);


	//o_Normals = clamp(vec4(o_Normals.x * 2.0 - 1.0, o_Normals.y * 2.0 - 1.0, 0.0f, 1.0f), 0.0, 1.0);
	//o_Normals.z = clamp(sqrt(1.0 - (o_Normals.x * o_Normals.x) - (o_Normals.y * o_Normals.y)), 0.0, 1.0);

	

	// Encode normals (from a vec3 to vec2)
	vec2 encodedNormals = EncodeNormal(normalize(vec3(o_Normals)));
	o_Normals = vec4(encodedNormals, 0.0f, 0.0f);

	
	// Composite (roughness and metalness)
	float metalness = metalnessFactor * SampleTexture(metalnessTextureID).r;
	float roughness = roughnessFactor * SampleTexture(roughnessTextureID).r;

	o_Normals.z = metalness;
	o_Normals.w = roughness;

	o_MeshID = v_EntityID;

	o_Velocity = (((v_CurrentPosition.xy / v_CurrentPosition.z) - u_PushConstant.JitterCurrent)
				- ((v_PreviousPosition.xy / v_PreviousPosition.z) - u_PushConstant.JitterPrevious)) * 0.5;
}