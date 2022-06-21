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
	int AtomicOperation;
} u_PushConstant;


layout(location = 0) out vData {
    vec2 TexCoord;
    vec4 PositionDepth;
} v_Data;

layout(location = 2) out flat int v_BufferIndex;

void main()
{
	Vertices verticies = Vertices(u_PushConstant.VertexBufferBDA);
	Vertex vertex = verticies.v[gl_VertexIndex];

	v_Data.TexCoord = vertex.TexCoord;
	v_Data.PositionDepth = vec4(0.0f); // TODO: Add the light view matrix to get depth coords

	int meshIndex = int(u_PushConstant.MaterialIndex + vertex.MaterialIndex);
	v_BufferIndex = int(meshIndex);

	// Compute world position
	vec4 worldPos = a_ModelSpaceMatrix * vec4(vertex.Position, 1.0f);
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

layout(location = 2) in flat int v_BufferIndex[];

layout(location = 0) out vec2 f_TexCoord;
layout(location = 1) out flat int g_BufferIndex;
layout(location = 2) out flat int f_Axis;
layout(location = 3) out mat4 f_ProjectionMatrix;
layout(location = 7) out vec4 f_PositionDepth;


// Computation of an extended triangle in clip space based on 
// "Conservative Rasterization", GPU Gems 2 Chapter 42 by Jon Hasselgren, Tomas Akenine-Möller and Lennart Ohlsson:
// http://http.developer.nvidia.com/GPUGems2/gpugems2_chapter42.html
void ComputeExtendedTriangle(vec2 halfPixelSize, vec3 triangleNormalClip, inout vec4 trianglePositionsClip[3], out vec4 triangleAABBClip)
{
	float trianglePlaneD = dot(trianglePositionsClip[0].xyz, triangleNormalClip); 
    float nSign = sign(triangleNormalClip.z);
        
    // Compute plane equations
    vec3 plane[3];
    plane[0] = cross(trianglePositionsClip[0].xyw - trianglePositionsClip[2].xyw, trianglePositionsClip[2].xyw);
    plane[1] = cross(trianglePositionsClip[1].xyw - trianglePositionsClip[0].xyw, trianglePositionsClip[0].xyw);
    plane[2] = cross(trianglePositionsClip[2].xyw - trianglePositionsClip[1].xyw, trianglePositionsClip[1].xyw);
    
    // Move the planes by the appropriate semidiagonal
    plane[0].z -= nSign * dot(halfPixelSize, abs(plane[0].xy));
    plane[1].z -= nSign * dot(halfPixelSize, abs(plane[1].xy));
    plane[2].z -= nSign * dot(halfPixelSize, abs(plane[2].xy));
	
	// Compute triangle AABB in clip space
    triangleAABBClip.xy = min(trianglePositionsClip[0].xy, min(trianglePositionsClip[1].xy, trianglePositionsClip[2].xy));
	triangleAABBClip.zw = max(trianglePositionsClip[0].xy, max(trianglePositionsClip[1].xy, trianglePositionsClip[2].xy));
    
    triangleAABBClip.xy -= halfPixelSize;
    triangleAABBClip.zw += halfPixelSize;
	
	for (int i = 0; i < 3; ++i)
    {
        // Compute intersection of the planes
        trianglePositionsClip[i].xyw = cross(plane[i], plane[(i + 1) % 3]);
        trianglePositionsClip[i].xyw /= trianglePositionsClip[i].w;
        trianglePositionsClip[i].z = -(trianglePositionsClip[i].x * triangleNormalClip.x + trianglePositionsClip[i].y * triangleNormalClip.y - trianglePlaneD) / triangleNormalClip.z;
    }
}

layout(set = 0, binding = 0) uniform VoxelProjections
{
	mat4 AxisX;
	mat4 AxisY;
	mat4 AxisZ;
} m_VoxelProjections;

void main()
{
	//Find the axis for the maximize the projected area of this triangle

	vec3 faceNormal = abs( normalize( cross( gl_in[1].gl_Position.xyz - gl_in[0].gl_Position.xyz, gl_in[2].gl_Position.xyz -gl_in[0].gl_Position.xyz) ) );
	
	// 0 = x axis dominant, 1 = y axis dominant, 2 = z axis dominant
	float maxi = max(faceNormal.x, max(faceNormal.y, faceNormal.z));

	if( maxi == faceNormal.x )
    {
		f_Axis = 0;	
	    f_ProjectionMatrix = m_VoxelProjections.AxisX;	
	}
	else if( maxi == faceNormal.y )
    {
		f_Axis = 1;	
	    f_ProjectionMatrix = m_VoxelProjections.AxisY;	
    }
	else
    {
		 f_Axis = 2;
	     f_ProjectionMatrix = m_VoxelProjections.AxisZ;	
	} 

	vec4 pos[3];
	for(int i = 0; i < 3; i++)
	{
		pos[i] = f_ProjectionMatrix * gl_in[i].gl_Position;
	}

	// For every vertex sent in vertices
    for(int i = 0; i < 3; i++)
	{
        f_TexCoord = v_Data[i].TexCoord;
        //f_TexCoord = texCoords[i];
        f_PositionDepth = v_Data[i].PositionDepth;

		g_BufferIndex = v_BufferIndex[i];

		//vec4 position = f_ProjectionMatrix * gl_in[i].gl_Position;
		vec4 position = pos[i];
        gl_Position = position;

        EmitVertex();
    }

	// Finished creating vertices
    EndPrimitive();
}

#type fragment(Frost_Bindless)
#version 460
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

// Output
layout(location = 0) out vec4 o_UselessAttachment;

// From geometry shader
layout(location = 0) in vec2 f_TexCoord;
layout(location = 1) in flat int g_BufferIndex;
layout(location = 2) in flat int f_Axis;
layout(location = 3) in mat4 f_ProjectionMatrix;
layout(location = 7) in vec4 f_PositionDepth;

// The same texture but with different formats (for atomic operations)
layout(set = 0, binding = 1) writeonly uniform image3D u_VoxelTexture_NonAtomic;
layout(set = 0, binding = 2, r32ui) uniform volatile coherent uimage3D u_VoxelTexture;

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


layout(push_constant) uniform Constants
{
	mat4 ViewMatrix;
	uint MaterialIndex;
	uint64_t VertexBufferBDA;
	int VoxelDimensions;
	int AtomicOperation;
} u_PushConstant;

// Bindless
layout(set = 1, binding = 0) uniform sampler2D u_Textures[];
vec4 SampleTexture(uint textureId)
{
	return texture(u_Textures[nonuniformEXT(textureId)], f_TexCoord);
}


vec4 convRGBA8ToVec4(uint val)
{
    return vec4(float((val & 0x000000FF)), 
    float((val & 0x0000FF00) >> 8U), 
    float((val & 0x00FF0000) >> 16U), 
    float((val & 0xFF000000) >> 24U));
}

uint convVec4ToRGBA8(vec4 val)
{
    return (uint(val.w) & 0x000000FF) << 24U | 
    (uint(val.z) & 0x000000FF) << 16U | 
    (uint(val.y) & 0x000000FF) << 8U | 
    (uint(val.x) & 0x000000FF);
}

// https://rauwendaal.net/2013/02/07/glslrunningaverage/ (for understanding the algorithm)
void imageAtomicRGBA8Avg(ivec3 coords, vec4 value)
{
    value.rgb *= 255.0;                 // optimize following calculations
    uint newVal = convVec4ToRGBA8(value);
    uint prevStoredVal = 0;
    uint curStoredVal;
    uint numIterations = 0;

	//"Spin" while threads are trying to change the voxel
    while((curStoredVal = imageAtomicCompSwap(u_VoxelTexture, coords, prevStoredVal, newVal)) 
            != prevStoredVal
            && numIterations < 10) // iterations are used to limit the time for waiting
			                      // (because otherwise the hit into the performance will be huge)
    {
        prevStoredVal = curStoredVal; //store packed rgb average
        vec4 rval = convRGBA8ToVec4(curStoredVal); //unpack stored rgb average

        rval.rgb = (rval.rgb * rval.a); // Denormalize

        vec4 curValF = rval + value;    // Add
        curValF.rgb /= curValF.a;       // Renormalize

		//curValF.a = 1.0f;

        newVal = convVec4ToRGBA8(curValF); // pack the rgb average

        ++numIterations;
    }
}



// https://rauwendaal.net/2013/02/07/glslrunningaverage/ (for understanding the algorithm)
void imageAtomicRGBA8Avg_2(ivec3 coords, vec4 value)
{
    uint nextUint = packUnorm4x8(vec4(value.xyz,1.0f/255.0f));
    uint prevUint = 0;
    uint currUint;
 
    vec4 currVec4;
 
    vec3 average;
    uint count;
 
    //"Spin" while threads are trying to change the voxel
    while((currUint = imageAtomicCompSwap(u_VoxelTexture, coords, prevUint, nextUint)) != prevUint)
    {
        prevUint = currUint;                    //store packed rgb average and count
        currVec4 = unpackUnorm4x8(currUint);    //unpack stored rgb average and count
 
        average =      currVec4.rgb;        //extract rgb average
        count   = uint(currVec4.a*255.0f);  //extract count
 
        //Compute the running average
        average = (average*count + value.xyz) / (count+1);
 
		//average.a = 1.0f;

        //Pack new average and incremented count back into a uint
        nextUint = packUnorm4x8(vec4(average, (count+1)/255.0f));
    }
}



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

	switch(f_Axis)
	{
		case 0:
		{
			texcoord.x = voxelDimensions - temp.z;
			texcoord.y = temp.y;
			texcoord.z = temp.x;
			break;
		}
		case 1:
		{
			texcoord.x = temp.x;
			texcoord.y = voxelDimensions - temp.z;
		    texcoord.z = temp.y;
			break;
		}
		case 2:
		{
			texcoord.x = temp.x;
			texcoord.y = temp.y;
			texcoord.z = temp.z;
			break;
		}
	}

	// Flip the Z
	texcoord.z = u_PushConstant.VoxelDimensions - texcoord.z - 1;


	// Atomic operations to get an averaged value, described in OpenGL insights about voxelization
	// Required to avoid flickering when voxelizing every frame
	if(u_PushConstant.AtomicOperation == 0)
		imageStore(u_VoxelTexture_NonAtomic, ivec3(texcoord.xyz), vec4(vec3(o_Albedo.xyz), 1.0f));
	else
		imageAtomicRGBA8Avg(ivec3(texcoord.xyz), vec4(vec3(o_Albedo.xyz), 1.0));

}