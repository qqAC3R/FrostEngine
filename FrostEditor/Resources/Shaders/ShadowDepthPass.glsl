#type vertex
#version 460
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

layout(push_constant) uniform Constants
{
	mat4 LightViewProjectionMatrix;
	uint64_t VertexBufferBDA;
	uint64_t BoneInformationBDA;
	uint MaterialIndex;
	uint IsAnimated;
} u_PushConstant;

void main()
{
	vec3 position;

	// If the mesh is animated, then compute the bone transform matrix
	mat4 boneTransform = mat4(1.0);
	
	if(u_PushConstant.IsAnimated  == 1)
	{
		AnimatedVertices animatedVerticies = AnimatedVertices(u_PushConstant.VertexBufferBDA);
		MeshBoneInformation boneInfo = MeshBoneInformation(u_PushConstant.BoneInformationBDA);
		AnimatedVertex vertex = animatedVerticies.v[gl_VertexIndex];

		position = vertex.Position;

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
	}

	vec4 worldPos = u_PushConstant.LightViewProjectionMatrix * a_ModelSpaceMatrix * boneTransform * vec4(position, 1.0f);
	gl_Position = worldPos;
}

#type fragment
#version 460

void main()
{
	// Nothing
}