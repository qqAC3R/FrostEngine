#type compute
#version 460

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(binding = 0) uniform sampler2D u_CurrentColorBuffer;
layout(binding = 1) uniform sampler2D u_CurrentDepthBuffer;
layout(binding = 2) uniform sampler2D u_VelocityBuffer;

layout(binding = 3) uniform sampler2D u_AccumulationColorBuffer;

layout(binding = 4) uniform sampler2D u_PreviouDepthBuffer;

layout(binding = 5) writeonly uniform image2D o_FinalTexture;


layout(push_constant) uniform Constants
{
	float RateOfChange;
	float CameraNearPlane;
} u_PushConstant;


//const float FLT_EPS = 0.00000001;
const float FLT_EPS = 0.0001;


vec3 YCoCgFromRGB(vec3 rgb)
{
	const float y = dot(vec3(0.25, 0.5, 0.25), rgb);
	const float co = dot(vec3(0.5, 0, -0.5), rgb);
	const float cg = dot(vec3(-0.25, 0.5, -0.25), rgb);
	return vec3(y, co, cg);
}
vec3 RGBFromYCoCg(vec3 yCoCg) {
	const float r = dot(vec3(1, 1,-1), yCoCg);
	const float g = dot(vec3(1, 0, 1), yCoCg);
	const float b = dot(vec3(1,-1,-1), yCoCg);
	return vec3(r, g, b);
}


struct MinMaxAvg
{
	vec3 minimum;
	vec3 maximum;
	vec3 average;
};

MinMaxAvg NeighbourhoodClamp(vec2 uv, vec2 scaling, vec3 cmc, sampler2D Color)
{
	const vec2 du = vec2(scaling.x, 0);
	const vec2 dv = vec2(0, scaling.y);

	const vec3 cul = YCoCgFromRGB(texture(Color, uv + dv - du).xyz);
	const vec3 cuc = YCoCgFromRGB(texture(Color, uv + dv).xyz);
	const vec3 cur = YCoCgFromRGB(texture(Color, uv + dv + du).xyz);
	const vec3 cml = YCoCgFromRGB(texture(Color, uv - du).xyz);	
	const vec3 cmr = YCoCgFromRGB(texture(Color, uv + du).xyz);
	const vec3 cbl = YCoCgFromRGB(texture(Color, uv - dv - du).xyz);
	const vec3 cbc = YCoCgFromRGB(texture(Color, uv - dv).xyz);
	const vec3 cbr = YCoCgFromRGB(texture(Color, uv - dv + du).xyz);
	const vec3 cmin = min(cul, min(cuc, min(cur, min(cml, min(cmc, min(cmr, min(cbl, min(cbc, cbr))))))));
	const vec3 cmax = max(cul, max(cuc, max(cur, max(cml, max(cmc, max(cmr, max(cbl, max(cbc, cbr))))))));
	const vec3 cavg = (cul + cuc + cur + cml + cmc + cmr + cbl + cbc + cbr) / 9.0;
	return MinMaxAvg(cmin, cmax, cavg);
}

vec3 ClosestUVZ(vec2 uv, vec2 scaling, sampler2D Depth)
{
	const vec2 du = vec2(scaling.x, 0);
	const vec2 dv = vec2(0, scaling.y);

	const float dul = texture(Depth, uv + dv - du).x;
	const float duc = texture(Depth, uv + dv).x;
	const float dur = texture(Depth, uv + dv + du).x;
	const float dml = texture(Depth, uv - du).x;
	const float dmc = texture(Depth, uv).x;
	const float dmr = texture(Depth, uv + du).x;
	const float dbl = texture(Depth, uv - dv - du).x;
	const float dbc = texture(Depth, uv - dv).x;
	const float dbr = texture(Depth, uv - dv + du).x;
	
	vec3 closest				 = vec3(-1,  1, dul);
	if (duc > closest.z) closest = vec3( 0,  1, duc);
	if (dur > closest.z) closest = vec3( 1,  1, dur);
	if (dml > closest.z) closest = vec3(-1,  0, dml);
	if (dmc > closest.z) closest = vec3( 0,  0, dmc);
	if (dmr > closest.z) closest = vec3( 1,  0, dmr);
	if (dbl > closest.z) closest = vec3(-1, -1, dbl);
	if (dbc > closest.z) closest = vec3( 0, -1, dbc);
	if (dbr > closest.z) closest = vec3( 1, -1, dbr);

	return vec3(uv + scaling * closest.xy, closest.z);
}

// Flimic SMAA version distorts image (with any sharpness settings and even with additional 4 taps), hence vanilla version
// https://vec3.ca/bicubic-filtering-in-fewer-taps/
vec3 CatmullRom5Tap(vec2 uv, vec4 scaling, sampler2D Color)
{
    const vec2 samplePos = uv * scaling.xy;
    const vec2 tc1 = floor(samplePos - 0.5) + 0.5;
    const vec2 f = samplePos - tc1;
    const vec2 f2 = f * f;
    const vec2 f3 = f * f2;

    const vec2 w0 = f2 - 0.5 * (f3 + f);
    const vec2 w1 = 1.5 * f3 - 2.5 * f2 + 1;
    const vec2 w3 = 0.5 * (f3 - f2);
    const vec2 w2 = 1 - w0 - w1 - w3;

    const vec2 w12 = w1 + w2;
    
    const vec2 tc0  = (tc1 - 1)			* scaling.zw;
    const vec2 tc12 = (tc1 + w2 / w12)	* scaling.zw;
    const vec2 tc3  = (tc1 + 2)			* scaling.zw;

    vec4 result = vec4(0);
    result += vec4(texture(Color, vec2(tc0.x,  tc12.y)).rgb, 1) * (w0.x  * w12.y);
    result += vec4(texture(Color, vec2(tc12.x, tc0.y )).rgb, 1) * (w12.x * w0.y );
    result += vec4(texture(Color, vec2(tc12.x, tc12.y)).rgb, 1) * (w12.x * w12.y);
    result += vec4(texture(Color, vec2(tc12.x, tc3.y )).rgb, 1) * (w12.x * w3.y );
    result += vec4(texture(Color, vec2(tc3.x,  tc12.y)).rgb, 1) * (w3.x  * w12.y);

    return result.rgb / result.a;
}

// https://github.com/playdeadgames/temporal/blob/master/Assets/Shaders/TemporalReprojection.shader
vec3 ClipAABB(vec3 aabb_min, vec3 aabb_max, vec3 p, vec3 q) 
{
	vec3 r = q - p;
	const vec3 rmax = aabb_max - p.xyz;
	const vec3 rmin = aabb_min - p.xyz;
	
	const float eps = FLT_EPS;


	if (r.x > rmax.x + eps)
		r *= (rmax.x / r.x);
	if (r.y > rmax.y + eps)
		r *= (rmax.y / r.y);
	if (r.z > rmax.z + eps)
		r *= (rmax.z / r.z);
	
	if (r.x < rmin.x - eps)
		r *= (rmin.x / r.x);
	if (r.y < rmin.y - eps)
		r *= (rmin.y / r.y);
	if (r.z < rmin.z - eps)
		r *= (rmin.z / r.z);
	
	return p + r;
}


float VsDepthFromCsDepth(float clipSpaceDepth, float near) {
	return -near / clipSpaceDepth;
}

void main()
{
    vec2 fragCoord = vec2(gl_GlobalInvocationID.xy) + 0.5.xx;
    vec2 texResolution = vec2(imageSize(o_FinalTexture).xy);
	vec2 uv = fragCoord / texResolution;

	vec4 scaling = vec4(texResolution.x, texResolution.y, 1.0 / texResolution.x, 1.0 / texResolution.y);

    const vec3 currentColor = YCoCgFromRGB(texture(u_CurrentColorBuffer, uv).rgb);
    const vec3 closestUVZ = ClosestUVZ(uv, scaling.zw, u_CurrentDepthBuffer);
	const vec2 velocity = texture(u_VelocityBuffer, closestUVZ.xy).xy;
	vec3 colorAccumulation = YCoCgFromRGB(CatmullRom5Tap(uv - velocity, scaling, u_AccumulationColorBuffer));


	MinMaxAvg minMaxAvg = NeighbourhoodClamp(uv, scaling.zw, currentColor, u_CurrentColorBuffer);

	const vec3 colorMin = minMaxAvg.minimum;
    const vec3 colorMax = minMaxAvg.maximum;    
	const vec3 colorAvg = minMaxAvg.average;
	
	// cliping towards colorAvg (instead of colorCurr) produses less flickering
	// but also produse strange artifacts on my "skybox" at certain angles with very high exposure
	// but I don't care about that "sky"
	colorAccumulation = ClipAABB(colorMin, colorMax, colorAvg, colorAccumulation);
	
	const float distToClamp = min(abs(colorMin.x - colorAccumulation.x), abs(colorMax.x - colorAccumulation.x));
	float rateOfChange = clamp((u_PushConstant.RateOfChange * distToClamp) / (distToClamp + colorMax.x - colorMin.x), 0, 1);
	
	const vec2 uvPrevDistanceToMiddle = abs((uv - velocity) - vec2(0.5));
	if (uvPrevDistanceToMiddle.x > 0.5 || uvPrevDistanceToMiddle.y > 0.5)
		rateOfChange = 1;

	// taking single sample produces less flickering than taking closest depth in 3x3 from DepthPrev
	// BUT closest depth in 3x3 from DepthCurrent is better
	const float depthPrev = texture(u_PreviouDepthBuffer, uv - velocity).r;
	if ((-VsDepthFromCsDepth(closestUVZ.z, u_PushConstant.CameraNearPlane) * 0.9) > -VsDepthFromCsDepth(depthPrev, u_PushConstant.CameraNearPlane))
		rateOfChange = 1;

    vec3 result = RGBFromYCoCg(mix(colorAccumulation, currentColor, rateOfChange));

    imageStore(o_FinalTexture, ivec2(gl_GlobalInvocationID.xy), vec4(result, 1.0));
}