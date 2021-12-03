#type vertex

////////////////////////////////////////////////////////////////////////////////
//// WARNING!!!: This works only if the device supports multi-draw feature  ////
////////////////////////////////////////////////////////////////////////////////

#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_nonuniform_qualifier : enable

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

// Using a storage buffer where we store the buffer pointers of the vertex buffers is more efficent
// because now we can batch together more meshes into a single draw call, without needing to copy the entire vertex buffer into a single one,
// only the index buffer is needed to copy, which is takes less more memory (index buffer uses 1 uint32_t per index
// while vertex buffer uses 1 'Vertex' struct per vertex (which is like 60 bytes, unlike the index buffer which is only 4 bytes).
// Making things into a larger scale, we'd approximately want around ~ 8-10 mil. vertices/indices into a draw call (this number
// can be changed depending on the performance) until we flush the buffer for the next draw call (if needed)
// NOTE: Now we dont need to bind a vertex buffer. Only the index buffer is needed in the pipeline
layout(set = 0, binding = 5, scalar) readonly buffer VertexPointer
{
	uint64_t pointer;
} u_VertexPointer;

layout(location = 0) out vec3 v_FragmentPos;
layout(location = 1) out vec3 v_Normal;
layout(location = 2) out vec2 v_TexCoord;
layout(location = 3) out mat3 v_TBN;
layout(location = 6) out vec3 v_ViewPosition;

layout(push_constant) uniform Constants
{
	mat4 TransformMatrix;
	mat4 ModelMatrix;
} u_PushConstant;

void main()
{
	Vertices verticies = Vertices(u_VertexPointer.pointer);
	Vertex vertex = verticies.v[gl_VertexIndex];

	vec4 worldPos = u_PushConstant.TransformMatrix * vec4(vertex.Position, 1.0);

	// Calculating the normals with the model matrix
	mat3 normalMatrix = transpose(inverse(mat3(u_PushConstant.ModelMatrix)));
	v_Normal = normalMatrix * vertex.Normal;

	// Texture Coords
	v_TexCoord = vertex.TexCoord;

	// World position
	v_FragmentPos = vec3(u_PushConstant.ModelMatrix * vec4(vertex.Position, 1.0f));

	// View position
	//vec4 viewPosition = u_PushConstant.ViewMatrix * u_PushConstant.ModelMatrix * vec4(a_Position, 1.0f);
	//v_ViewPosition = viewPosition.xyz;

	// Calculating the TBN matrix for normal maps
	vec3 N = normalize(vertex.Normal);
	vec3 T = normalize(vertex.Tangent);
	vec3 B = normalize(vertex.Bitangent);
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
layout(location = 4) out vec4 o_ViewPosition;

layout(location = 0) in vec3 v_FragmentPos;
layout(location = 1) in vec3 v_Normal;
layout(location = 2) in vec2 v_TexCoord;
layout(location = 3) in mat3 v_TBN;
layout(location = 6) in vec3 v_ViewPosition;

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

	// Composite (roughness and metalness)
	float metalness = GetFloatValueFromTexture(u_MetalnessTexture, MaterialUniform.Metalness);
	float roughness = GetFloatValueFromTexture(u_RoughnessTexture, MaterialUniform.Roughness);
	o_Composite = vec4(metalness, roughness, 1.0f, 1.0f);

	o_ViewPosition = vec4(v_ViewPosition, 1.0f);
}