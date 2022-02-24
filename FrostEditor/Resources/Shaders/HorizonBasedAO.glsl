//#type compute
#version 460

const float PI = 3.14159265;

vec2 AORes = vec2(1024.0, 768.0);
vec2 NoiseScale = vec2(1024.0, 768.0) / 4.0;

float AOStrength = 1.9;
float R = 0.3;
float R2 = 0.3*0.3;
float NegInvR2 = - 1.0 / (0.3*0.3);
float TanBias = tan(30.0 * PI / 180.0);
float MaxRadiusPixels = 100.0;

int NumDirections = 6;
int NumSamples = 4;

uvec2 s_TeexCoord; // Determined by `gl_GlobalInvocationID`


layout(push_constant) uniform PushConstant
{
    vec2 ScreenRes;
	vec2 LinNF; // near flar plane
	vec2 UVToViewA;
	vec2 UVToViewB;
	vec2 InvAORes // vec2(1.0f / width, 1.0f / height);
} u_PushConstant;

layout(binding = 0) uniform sampler2D u_DepthTexture;


float ViewSpaceZFromDepth(float d)
{
	// [0,1] -> [-1,1] clip space
	d = d * 2.0f - 1.0f;

	// Get view space Z
	return -1.0f / (u_PushConstant.LinNF.x * d + u_PushConstant.LinNF.y);
}

vec3 UVToViewSpace(vec2 uv, float z)
{
	uv = u_PushConstant.UVToViewA * uv + u_PushConstant.UVToViewB;
	return vec3(uv * z, z);
}

vec3 GetViewPos(vec2 uv)
{
	float z = ViewSpaceZFromDepth(texture(u_DepthTexture, uv).r);
	//float z = texture(texture0, uv).r;
	return UVToViewSpace(uv, z);
}

void main()
{
	uvec2 s_TeexCoord = gl_GlobalInvocationID.xy;

	vec3 P, Pr, Pl, Pt, Pb; // pixel right, left, top, bottom
	P = GetViewPos(s_TeexCoord);

	// Sample neighboring pixels
    Pr 	= GetViewPos(s_TeexCoord + vec2( u_PushConstant.InvAORes.x, 0));
    Pl 	= GetViewPos(s_TeexCoord + vec2(-u_PushConstant.InvAORes.x, 0));
    Pt 	= GetViewPos(s_TeexCoord + vec2( 0, u_PushConstant.InvAORes.y));
    Pb 	= GetViewPos(s_TeexCoord + vec2( 0,-u_PushConstant.InvAORes.y));



}