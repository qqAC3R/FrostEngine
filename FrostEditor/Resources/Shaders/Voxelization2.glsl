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

layout(push_constant) uniform Constants
{
	mat4 ViewMatrix;
	uint MaterialIndex;
	uint64_t VertexBufferBDA;
	uint VoxelDimensions;
} u_PushConstant;

layout(location = 0) out vData {
    vec2 TexCoord;
    vec4 PositionDepth;
} v_Data;

layout(location = 2) out vec3 v_WorldPos;

void main()
{
	Vertices verticies = Vertices(u_PushConstant.VertexBufferBDA);
	Vertex vertex = verticies.v[gl_VertexIndex];

	v_Data.TexCoord = vertex.TexCoord;
	v_Data.PositionDepth = vec4(0.0f); // TODO: Add the light view matrix to get depth coords

	// Compute world position
	vec4 worldPos = a_ModelSpaceMatrix * vec4(vertex.Position, 1.0f);
	v_WorldPos = worldPos.xyz;

	gl_Position = worldPos;
}

#type geometry
#version 460

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

#define VERTEX_COUNT 3
#define VOLUME_SIZE 32

// Input from vertex shader, stored in an array
layout(location = 0) in vData {
    vec2 TexCoord;
    vec4 PositionDepth;
} v_Data[];

layout(location = 2) in vec3 v_WorldPos[];

// Data that will be sent to fragment shader
//layout(location = 1) out fData {
//    vec2 TexCoord;
//    flat int Axis;
//    mat4 ProjectionMatrix;
//    vec4 PositionDepth;
//} f_Data;

layout(location = 0) out mat3 f_SwizzleMatrixInv;
layout(location = 3) out vec4 f_GeometryAABB;

layout(set = 0, binding = 0) uniform VoxelProjections
{
	mat4 AxisX;
	mat4 AxisY;
	mat4 AxisZ;
} m_VoxelProjections;

float GetInterpolatedComp (float comp, float minValue, float maxValue)
{
	return ((comp - minValue) / (maxValue - minValue));
}

vec4 GetScreenNormalizedPosition (vec4 position)
{
	vec4 screenPos = vec4 (1.0);

	//screenPos.x = GetInterpolatedComp (position.x, minVertex.x, maxVertex.x);
	//screenPos.y = GetInterpolatedComp (position.y, minVertex.y, maxVertex.y);
	//screenPos.z = GetInterpolatedComp (position.z, minVertex.z, maxVertex.z);

	screenPos.xyz = (vec3 (screenPos) * vec3 (2.0)) - vec3 (1.0);

	return screenPos;
}


mat3 GetSwizzleMatrix ()
{
	vec3 worldSpaceV1 = v_WorldPos[1].xyz - v_WorldPos[0].xyz;
	vec3 worldSpaceV2 = v_WorldPos[2].xyz - v_WorldPos[0].xyz;
	vec3 worldSpaceNormal = abs(cross(worldSpaceV1, worldSpaceV2));

	if (worldSpaceNormal.x >= worldSpaceNormal.y && worldSpaceNormal.x >= worldSpaceNormal.z) {
		return mat3(vec3(0.0, 0.0, 1.0),
					vec3(0.0, 1.0, 0.0),
					vec3(1.0, 0.0, 0.0));
	}
	else if (worldSpaceNormal.y >= worldSpaceNormal.z) {
		return mat3(vec3(1.0, 0.0, 0.0),
					vec3(0.0, 0.0, 1.0),
					vec3(0.0, 1.0, 0.0));
	}

	return mat3(vec3(1.0, 0.0, 0.0),
				vec3(0.0, 1.0, 0.0),
				vec3(0.0, 0.0, 1.0));
}

float GetPixelDiagonal()
{
	float pixelSize = 1.0 / VOLUME_SIZE;

	return 1.41421f * pixelSize;
}



void main()
{

	mat3 swizzleMatrix = GetSwizzleMatrix ();
	f_SwizzleMatrixInv = inverse(swizzleMatrix);

	vec4 screenPos[VERTEX_COUNT];

	for (int index = 0;index < VERTEX_COUNT; ++index) {
		screenPos[index] = vec4(v_WorldPos[index], 1.0);
		screenPos[index] = GetScreenNormalizedPosition(screenPos[index]);
		screenPos[index] = vec4(swizzleMatrix * screenPos[index].xyz, 1.0f);
	}

	float pixelDiagonal = GetPixelDiagonal();

	// Calculate screen space bounding box to be used for clipping in the fragment shader.
    f_GeometryAABB.xy = min(screenPos[0].xy, min(screenPos[1].xy, screenPos[2].xy));
    f_GeometryAABB.zw = max(screenPos[0].xy, max(screenPos[1].xy, screenPos[2].xy));
    f_GeometryAABB.xy -= vec2(pixelDiagonal);
    f_GeometryAABB.zw += vec2(pixelDiagonal);



	for (int index = 0; index < VERTEX_COUNT; ++index) {
		gl_Position = screenPos[index];
		EmitVertex();
	}

	EndPrimitive ();
}

#type fragment
#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

//layout(location = 1) in fData {
//    vec2 TexCoord;
//    mat4 ProjectionMatrix;
//    flat int Axis;
//    vec4 PositionDepth;
//} f_Data;

layout(location = 0) in mat3 f_SwizzleMatrixInv;
layout(location = 3) in vec4 f_GeometryAABB;


layout(set = 0, binding = 1) writeonly uniform image3D u_VoxelTexture;

layout(set = 0, binding = 2) buffer DebugBuffer
{
	vec3 Values;
} u_DebugBuffer;

//layout(set = 0, binding = 2) readonly buffer u_MaterialUniform
//{
//	MaterialData Data[];
//} MaterialUniform;

// Bindless
//layout(set = 1, binding = 0) uniform sampler2D u_Textures[];
//vec4 SampleTexture(uint textureId)
//{
//	return texture(u_Textures[nonuniformEXT(textureId)], f_Data.TexCoord);
//}

layout(push_constant) uniform Constants
{
	mat4 ViewMatrix;
	uint MaterialIndex;
	uint64_t VertexBufferBDA;
	int VoxelDimensions;
} u_PushConstant;

layout(location = 0) out vec4 o_UselessAttachment;

void main()
{
	
	vec2 bboxMin = floor((f_GeometryAABB.xy * 0.5 + 0.5) * vec2(u_PushConstant.VoxelDimensions));
	vec2 bboxMax = ceil((f_GeometryAABB.zw * 0.5 + 0.5) * vec2(u_PushConstant.VoxelDimensions));
	if (!all(greaterThanEqual(gl_FragCoord.xy, bboxMin)) || !all(lessThanEqual(gl_FragCoord.xy, bboxMax))) {
		discard;
	}

	vec3 coords = f_SwizzleMatrixInv * vec3(gl_FragCoord.xy, gl_FragCoord.z * u_PushConstant.VoxelDimensions);

	
	// Overwrite currently stored value.
	// TODO: Atomic operations to get an averaged value, described in OpenGL insights about voxelization
	// Required to avoid flickering when voxelizing every frame
	imageStore(u_VoxelTexture, ivec3(coords), vec4(vec3(1.0f), 1.0f));
	//imageStore(u_VoxelTexture, ivec3(voxelPos), vec4(vec3(1.0f), 1.0f));
	//imageStore(u_VoxelTexture, ivec3(int(u_DebugBuffer.Values.x), 0, 0), vec4(vec3(float(f_Axis) / 10.0f), 1.0f));

}