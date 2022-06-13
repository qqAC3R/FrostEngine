#type vertex
#version 460

layout(binding = 0) uniform sampler3D u_VoxelTexture;

layout(location = 0) out vec4 v_Color;

void main()
{
	int Dimensions = 64;

	vec3 pos; // Center of voxel
	pos.x = gl_VertexIndex % Dimensions;
	pos.z = (gl_VertexIndex / Dimensions) % Dimensions;
	pos.y = gl_VertexIndex / (Dimensions*Dimensions);

	v_Color = texture(u_VoxelTexture, pos/Dimensions);
	gl_Position = vec4(pos - Dimensions * 0.5 , 1);
}

#type geometry
#version 460

layout(points) in;
layout(triangle_strip, max_vertices = 36) out;

layout(push_constant) uniform PushConstant
{
	mat4 ModelViewMatrix;
	mat4 ProjectionMatrix;
} u_PushConstant;

layout(location = 0) in vec4 v_Color[];

layout(location = 0) out vec4 g_Color;
layout(location = 1) out vec3 g_Normal;


void main()
{
	g_Color = v_Color[0];

	vec4 v1 = u_PushConstant.ProjectionMatrix * u_PushConstant.ModelViewMatrix * (gl_in[0].gl_Position + vec4(-0.5, 0.5, 0.5, 0));
	vec4 v2 = u_PushConstant.ProjectionMatrix * u_PushConstant.ModelViewMatrix * (gl_in[0].gl_Position + vec4(0.5, 0.5, 0.5, 0));
	vec4 v3 = u_PushConstant.ProjectionMatrix * u_PushConstant.ModelViewMatrix * (gl_in[0].gl_Position + vec4(-0.5, -0.5, 0.5, 0));
	vec4 v4 = u_PushConstant.ProjectionMatrix * u_PushConstant.ModelViewMatrix * (gl_in[0].gl_Position + vec4(0.5, -0.5, 0.5, 0));
	vec4 v5 = u_PushConstant.ProjectionMatrix * u_PushConstant.ModelViewMatrix * (gl_in[0].gl_Position + vec4(-0.5, 0.5, -0.5, 0));
	vec4 v6 = u_PushConstant.ProjectionMatrix * u_PushConstant.ModelViewMatrix * (gl_in[0].gl_Position + vec4(0.5, 0.5, -0.5, 0));
	vec4 v7 = u_PushConstant.ProjectionMatrix * u_PushConstant.ModelViewMatrix * (gl_in[0].gl_Position + vec4(-0.5, -0.5, -0.5, 0));
	vec4 v8 = u_PushConstant.ProjectionMatrix * u_PushConstant.ModelViewMatrix * (gl_in[0].gl_Position + vec4(0.5, -0.5, -0.5, 0));

	//
	//      v5 _____________ v6
	//        /|           /|
	//       / |          / |
	//      /  |         /  |
	//     /   |        /   |
	// v1 /____|_______/ v2 |
	//    |    |       |    |
	//    |    |_v7____|____| v8
	//    |   /        |   /
	//    |  /         |  /  
	//    | /          | /  
	// v3 |/___________|/ v4
	//

	// TODO: Optimize
	// +Z
    g_Normal = vec3(0, 0, 1);
    gl_Position = v1;
    EmitVertex();
    gl_Position = v3;
    EmitVertex();
	gl_Position = v4;
    EmitVertex();
    EndPrimitive();
    gl_Position = v1;
    EmitVertex();
    gl_Position = v4;
    EmitVertex();
	gl_Position = v2;
    EmitVertex();
    EndPrimitive();

    // -Z
    g_Normal = vec3(0, 0, -1);
    gl_Position = v6;
    EmitVertex();
    gl_Position = v8;
    EmitVertex();
	gl_Position = v7;
    EmitVertex();
    EndPrimitive();
    gl_Position = v6;
    EmitVertex();
    gl_Position = v7;
    EmitVertex();
	gl_Position = v5;
    EmitVertex();
    EndPrimitive();

    // +X
    g_Normal = vec3(1, 0, 0);
    gl_Position = v2;
    EmitVertex();
    gl_Position = v4;
    EmitVertex();
	gl_Position = v8;
    EmitVertex();
    EndPrimitive();
    gl_Position = v2;
    EmitVertex();
    gl_Position = v8;
    EmitVertex();
	gl_Position = v6;
    EmitVertex();
    EndPrimitive();

    // -X
    g_Normal = vec3(-1, 0, 0);
    gl_Position = v5;
    EmitVertex();
    gl_Position = v7;
    EmitVertex();
	gl_Position = v3;
    EmitVertex();
    EndPrimitive();
    gl_Position = v5;
    EmitVertex();
    gl_Position = v3;
    EmitVertex();
	gl_Position = v1;
    EmitVertex();
    EndPrimitive();

    // +Y
    g_Normal = vec3(0, 1, 0);
    gl_Position = v5;
    EmitVertex();
    gl_Position = v1;
    EmitVertex();
	gl_Position = v2;
    EmitVertex();
    EndPrimitive();
    gl_Position = v5;
    EmitVertex();
    gl_Position = v2;
    EmitVertex();
	gl_Position = v6;
    EmitVertex();
    EndPrimitive();

    // -Y
    g_Normal = vec3(0, -1, 0);
    gl_Position = v3;
    EmitVertex();
    gl_Position = v7;
    EmitVertex();
	gl_Position = v8;
    EmitVertex();
    EndPrimitive();
    gl_Position = v3;
    EmitVertex();
    gl_Position = v8;
    EmitVertex();
	gl_Position = v4;
    EmitVertex();
    EndPrimitive();
}

#type fragment
#version 460

layout(location = 0) in vec4 g_Color;
layout(location = 1) in vec3 g_Normal;

layout(location = 0) out vec4 o_Color;

void main()
{
    if(g_Color.a < 0.5f)
        discard;

    o_Color = vec4(g_Color);
}