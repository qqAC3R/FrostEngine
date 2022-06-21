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

// Using buffer references instead of typical attributes
layout(buffer_reference, scalar) buffer Vertices { Vertex v[]; }; // Positions of an object

layout(push_constant) uniform Constants
{
	mat4 LightViewProjectionMatrix;
	uint64_t VertexBufferBDA;
	uint CascadeIndex;
} u_PushConstant;

void main()
{
	mat4 lightViewProj;
	
	Vertices verticies = Vertices(u_PushConstant.VertexBufferBDA);
	Vertex vertex = verticies.v[gl_VertexIndex];

	vec4 worldPos = u_PushConstant.LightViewProjectionMatrix * a_ModelSpaceMatrix * vec4(vertex.Position, 1.0f);
	gl_Position = worldPos;
}

#type fragment
#version 460

void main()
{
	// Nothing
}