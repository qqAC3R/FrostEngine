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
	int VoxelDimensions;
	int AxisToShow;
} u_PushConstant;

layout(location = 0) out vData {
    vec2 TexCoord;
    vec4 PositionDepth;
} v_Data;

layout(location = 2) out vec4 v_WorldPos;

layout(location = 3) out flat int v_BufferIndex;
layout(location = 4) out flat int v_TextureIndex;
layout(location = 5) out flat int v_AxisToShow;


void main()
{
	Vertices verticies = Vertices(u_PushConstant.VertexBufferBDA);
	Vertex vertex = verticies.v[gl_VertexIndex];

	v_Data.TexCoord = vertex.TexCoord;
	v_Data.PositionDepth = vec4(0.0f); // TODO: Add the light view matrix to get depth coords

	int meshIndex = int(u_PushConstant.MaterialIndex + vertex.MaterialIndex);
	v_BufferIndex = int(meshIndex);
	v_TextureIndex = int(vertex.MaterialIndex);
	v_AxisToShow = u_PushConstant.AxisToShow;

	// Compute world position
	vec4 worldPos = a_ModelSpaceMatrix * vec4(vertex.Position, 1.0f);

	v_WorldPos = worldPos;
	gl_Position = worldPos;
}

#type geometry
#version 460

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

// Input from vertex shader, stored in an array
layout(location = 0) in vData {
    vec2 TexCoord;
    vec4 PositionDepth;
} v_Data[];

layout(location = 2) in vec4 v_WorldPos[];

layout(location = 3) in flat int v_BufferIndex[];
layout(location = 4) in flat int v_TextureIndex[];
layout(location = 5) in flat int v_AxisToShow[];


// Data that will be sent to fragment shader
//layout(location = 1) out fData {
//    vec2 TexCoord;
//    flat int Axis;
//    mat4 ProjectionMatrix;
//    vec4 PositionDepth;
//} f_Data;

layout(location = 0) out vec2 f_TexCoord;
layout(location = 1) out flat int f_Axis;
layout(location = 2) out mat4 f_ProjectionMatrix;
layout(location = 6) out vec4 f_PositionDepth;
layout(location = 7) out vec4 f_Position;
layout(location = 8) out flat int g_BufferIndex;
layout(location = 9) out flat int g_TextureIndex;


layout(set = 0, binding = 0) uniform VoxelProjections
{
	mat4 AxisX;
	mat4 AxisY;
	mat4 AxisZ;
} m_VoxelProjections;

int GetDominantAxis(vec3 pos0, vec3 pos1, vec3 pos2)
{
	vec3 normal = abs(cross(pos1 - pos0, pos2 - pos0));
	return (normal.x > normal.y && normal.x > normal.z) ? 1 : 
			(normal.y > normal.z) ? 2 : 3;
}

vec2 Project(vec3 vertex, uint axis) 
{
	return axis == 0 ? vertex.yz : (axis == 1 ? vertex.xz : vertex.xy);
}

vec3 BiasAndScale(vec3 vertex)
{
	return (vertex + 1.0) * 0.5;
}

void main()
{
	//uint axis =   GetDominantAxis(gl_in[0].gl_Position.xyz,
	//						      gl_in[1].gl_Position.xyz,
	//						      gl_in[2].gl_Position.xyz);

	// calculate triangle normal
	//vec3 tNorm         = normalize(cross( gl_in[1].gl_Position.xyz-gl_in[0].gl_Position.xyz, gl_in[2].gl_Position.xyz-gl_in[0].gl_Position.xyz));
	//vec3 tn            = abs( tNorm );
	//
	//uint maxIndex = (tn.y>tn.x) ? ( (tn.z>tn.y) ? 3 : 2 ) : ( (tn.z>tn.x) ? 3 : 1 );
	//
	//f_Axis = int(maxIndex);



	/*
	vec3 p1 = v_WorldPos[1].xyz - v_WorldPos[0].xyz;
    vec3 p2 = v_WorldPos[2].xyz - v_WorldPos[0].xyz;
    vec3 normal = normalize(cross(p1,p2));
	
	float NdotX = abs(normal.x);
    float NdotY = abs(normal.y);
    float NdotZ = abs(normal.z);
	
	// 1 = x axis dominant, 2 = y axis dominant, 3 = z axis dominant
    f_Axis = (NdotX >= NdotY && NdotX >= NdotZ) ? 1 : (NdotY >= NdotX && NdotY >= NdotZ) ? 2 : 3;
    f_Axis = 1;
    f_ProjectionMatrix = f_Axis == 1 ? m_VoxelProjections.AxisX : f_Axis == 2 ? m_VoxelProjections.AxisY : m_VoxelProjections.AxisZ;
    */

	//Find the axis for the maximize the projected area of this triangle

	vec3 faceNormal = abs( normalize( cross( gl_in[1].gl_Position.xyz - gl_in[0].gl_Position.xyz, gl_in[2].gl_Position.xyz -gl_in[0].gl_Position.xyz) ) );
	
	float maxi = max(faceNormal.x, max(faceNormal.y, faceNormal.z));

	if( maxi == faceNormal.x )
    {
	    f_ProjectionMatrix = m_VoxelProjections.AxisX;	
		f_Axis = 0;	
	}
	else if( maxi == faceNormal.y )
    {
	    f_ProjectionMatrix = m_VoxelProjections.AxisY;	
		f_Axis = 1;	
    }
	else
    {
	     f_ProjectionMatrix = m_VoxelProjections.AxisZ;	
		 f_Axis = 2;
	} 



	// Manual
	//f_Axis = v_AxisToShow[0];
	//if(f_Axis == 0)
    //{
	//    f_ProjectionMatrix = m_VoxelProjections.AxisX;	
	//}
	//else if(f_Axis == 1)
    //{
	//    f_ProjectionMatrix = m_VoxelProjections.AxisY;	
    //}
	//else
    //{
	//     f_ProjectionMatrix = m_VoxelProjections.AxisZ;	
	//}   


	// For every vertex sent in vertices
    for(int i = 0; i < 3; i++)
	{
		//gl_Position = vec4(Project(gl_in[i].gl_Position.xyz, axis), 1.0, 1.0); // uViewProj[axis] * gl_in[i].gl_Position;
		//gl_Position = position;
		//f_Position = vec4(BiasAndScale(position.xyz), position.w);

        f_TexCoord = v_Data[i].TexCoord;
        f_PositionDepth = v_Data[i].PositionDepth;

		g_BufferIndex = v_BufferIndex[i];
		g_TextureIndex = v_TextureIndex[i];

		vec4 position = f_ProjectionMatrix * v_WorldPos[i];
		//vec4 position = f_ProjectionMatrix * gl_in[i].gl_Position;
        gl_Position = position;

		// https://github.com/cpvrlab/SLProject
		//if(f_Axis == 1)
		//	gl_Position = vec4(worldPos.z, worldPos.y, 0, 1);
		//else if (f_Axis == 2)
		//	gl_Position = vec4(worldPos.x, worldPos.z, 0, 1);
		//else
		//	gl_Position = vec4(worldPos.x, worldPos.y, 0, 1);

        EmitVertex();
    }

	// Finished creating vertices
    EndPrimitive();
}

#type fragment(Frost_Bindless)
#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

layout(location = 0) out vec4 o_UselessAttachment;
//layout(pixel_center_integer) in vec4 gl_FragCoord;


//layout(location = 1) in fData {
//    vec2 TexCoord;
//    mat4 ProjectionMatrix;
//    flat int Axis;
//    vec4 PositionDepth;
//} f_Data;

layout(location = 0) in vec2 f_TexCoord;
layout(location = 1) in flat int f_Axis;
layout(location = 2) in mat4 f_ProjectionMatrix;
layout(location = 6) in vec4 f_PositionDepth;
layout(location = 7) in vec4 f_Position;
layout(location = 8) in flat int g_BufferIndex;
layout(location = 9) in flat int g_TextureIndex;


layout(set = 0, binding = 1) writeonly uniform image3D u_VoxelTexture;

layout(set = 0, binding = 2) buffer DebugBuffer
{
	vec3 Values[];
} u_DebugBuffer;


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
layout(set = 0, binding = 3) readonly buffer u_MaterialUniform
{
	MaterialData Data[];
} MaterialUniform;

// Bindless
layout(set = 1, binding = 0) uniform sampler2D u_Textures[];
vec4 SampleTexture(uint textureId)
{
	return texture(u_Textures[nonuniformEXT(textureId)], f_TexCoord);
}

layout(push_constant) uniform Constants
{
	mat4 ViewMatrix;
	uint MaterialIndex;
	uint64_t VertexBufferBDA;
	int VoxelDimensions;
	int AxisToShow;
	vec3 VoxelOffsets;
} u_PushConstant;


void main()
{
	
	// Albedo color
	uint materialIndex = uint(g_BufferIndex);
	
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
	vec3 albedoTextureColor = SampleTexture(albedoTextureID).rgb;
	vec4 o_Albedo = vec4(albedoTextureColor * albedoFactor + (emissionFactor), 1.0f);
	

	int voxelDimensions = u_PushConstant.VoxelDimensions;
	uvec4 temp = uvec4(gl_FragCoord.x, voxelDimensions - gl_FragCoord.y, float(voxelDimensions) * gl_FragCoord.z, 0);
	uvec4 texcoord;

	if(f_Axis == 0 )
	{
	    //texcoord.x = temp.z;
	    //texcoord.x = temp.z - (voxelDimensions / 2);
		texcoord.x = voxelDimensions - temp.z;
		//texcoord.x = temp.z;
		texcoord.y = temp.y;
		texcoord.z = temp.x;
		
	}
	else if(f_Axis == 1)
	{
		texcoord.x = temp.x;
		texcoord.y = voxelDimensions - temp.z;
	    texcoord.z = temp.y;
	}
	else
	{
		//texcoord.x = temp.x - (voxelDimensions / 2);
		texcoord.x = temp.x;
		texcoord.y = temp.y;
		texcoord.z = temp.z;
		//texcoord.z = temp.z - (voxelDimensions / 2);
	}

	texcoord.z = u_PushConstant.VoxelDimensions - texcoord.z - 1;

	imageStore(u_VoxelTexture, ivec3(texcoord.xyz), vec4(vec3(o_Albedo.xyz), 1.0f));


	/*
	int voxelDimensions = u_PushConstant.VoxelDimensions;

	ivec3 camPos = ivec3(gl_FragCoord.x, gl_FragCoord.y, voxelDimensions * gl_FragCoord.z);
	ivec3 texPos;
	
	if(f_Axis == 1) {
	    texPos.x = voxelDimensions - camPos.z;
		texPos.y = camPos.y;
		texPos.z = camPos.x;
	}
	else if(f_Axis == 2) {
		texPos.x = camPos.x;
		texPos.y = voxelDimensions - camPos.z;
	    texPos.z = camPos.y;
	}
	else {
	    texPos = camPos;
	}

	// Flip it!
	texPos.z = u_PushConstant.VoxelDimensions - texPos.z - 1;

	imageStore(u_VoxelTexture, texPos, vec4(vec3(o_Albedo.xyz), 1.0f));
	*/



	//u_DebugBuffer.Values[0].x += 1.0f;
	//
	//if(gl_FragCoord.x != 0.0f || gl_FragCoord.y != 0.0f || gl_FragCoord.z != 0.0f)
	//	u_DebugBuffer.Values[int(u_DebugBuffer.Values[0].x)] = vec3(texPos);
	
	//uvec3 voxelPos = clamp(uvec3(f_Position.xyz * float(voxelDimensions)),  uvec3(0u),  uvec3(voxelDimensions));
	//
	//u_DebugBuffer.Values = vec3(5.0f);
	//if(voxelPos.y != 0)
	//	u_DebugBuffer.Values.y = float(voxelPos.y);
	//if(voxelPos.z != 0)
	//	u_DebugBuffer.Values.z = float(voxelPos.z);
	
	//vec3 voxelPosition = f_Position.xyz / f_Position.w;
	//vec3 voxelPosition = f_Position.xyz;
	//vec3 address3D = voxelPosition.xyz * 0.5f + vec3(0.5f);
	
	//if(texPos.x != 0)
	

	// Overwrite currently stored value.
	// TODO: Atomic operations to get an averaged value, described in OpenGL insights about voxelization
	// Required to avoid flickering when voxelizing every frame

	// Overwrite currently stored value.
	// TODO: Atomic operations to get an averaged value, described in OpenGL insights about voxelization
	// Required to avoid flickering when voxelizing every frame
	//imageStore(u_VoxelTexture, ivec3(imageSize(u_VoxelTexture) * address3D), vec4(vec3(f_Position), 1.0f));

	
	//imageStore(u_VoxelTexture, ivec3(int(u_DebugBuffer.Values.x), 0, 0), vec4(vec3(float(f_Axis) / 10.0f), 1.0f));

}