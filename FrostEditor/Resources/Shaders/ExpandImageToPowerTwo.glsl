#type compute
#version 450 core

layout(binding = 0) uniform sampler2D u_InputImage;
layout(rgba32f, binding = 1) uniform image2D u_OutputImage;

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(push_constant) uniform PushConstant
{
	float UsePrefiltering;
	float Threshold;
	float Knee;
} u_PushConstant;

const float Epsilon = 1.0e-4;

// Quadratic color thresholding
// curve = (threshold - knee, knee * 2, 0.25 / knee)
vec4 QuadraticThreshold(vec4 color, float threshold, vec3 curve)
{
    // Maximum pixel brightness
    float brightness = max(max(color.r, color.g), color.b);
    // Quadratic curve
    float rq = clamp(brightness - curve.x, 0.0, curve.y);
    rq = (rq * rq) * curve.z;
    color *= max(rq, brightness - threshold) / max(brightness, Epsilon);
    return color;
}

vec3 Prefilter(vec4 color)
{
    float threshold = u_PushConstant.Threshold;
    float knee = u_PushConstant.Knee;
    vec3 curve = vec3(threshold - knee, knee * 2.0, 0.25 / knee);

    float clampValue = 20.0f;
    color = min(vec4(clampValue), color);
    color = QuadraticThreshold(color, threshold, curve);

    return color.xyz;
}

void main() {
    const ivec2 inputImageSize = textureSize(u_InputImage, 0);
    if(gl_GlobalInvocationID.x >= inputImageSize.x || gl_GlobalInvocationID.y >= inputImageSize.y)
        return;

    const ivec2 outputImageSize = imageSize(u_OutputImage);

    vec4 color = texelFetch(u_InputImage, ivec2(gl_GlobalInvocationID.xy), 0);

    if(u_PushConstant.UsePrefiltering == 1.0)
        color.rgb = Prefilter(color);
    
    vec2 uv = (vec2(gl_GlobalInvocationID.xy) + 0.5) / inputImageSize;

    ivec2 offsets = (outputImageSize - inputImageSize) / 2;
    imageStore(u_OutputImage, offsets + ivec2(gl_GlobalInvocationID).xy, vec4(color.rgb, 0.0));
}