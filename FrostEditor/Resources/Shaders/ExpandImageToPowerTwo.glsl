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
    float ClearImage;
} u_PushConstant;

const float Epsilon = 1.0e-4;
const vec2 BLOOM_CONV_OFFSET = vec2(128.0f); // Offset required in order to add clamping

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

vec3 SampleDownsampledTexture(ivec2 coordinate)
{
    const vec2 inputImageSize = vec2(textureSize(u_InputImage, 0));
    const vec2 outputImageSize = vec2(imageSize(u_OutputImage));

    float ratioInputImg = inputImageSize.x / inputImageSize.y;
    vec2 adjustedInputSize = vec2(outputImageSize.x, outputImageSize.y * (1.0f / ratioInputImg));
    adjustedInputSize = adjustedInputSize - BLOOM_CONV_OFFSET;

    vec2 stepCoords = inputImageSize / vec2(adjustedInputSize);
    ivec2 sampleCoord = ivec2(stepCoords * vec2(coordinate));
    return texelFetch(u_InputImage, sampleCoord, 0).xyz;
}

vec3 SampleTexture(ivec2 coordinate)
{
    return texelFetch(u_InputImage, ivec2(coordinate), 0).xyz;
}

void main() {
    const ivec2 inputImageSize = textureSize(u_InputImage, 0);
    const ivec2 outputImageSize = imageSize(u_OutputImage);
    if(gl_GlobalInvocationID.x >= inputImageSize.x || gl_GlobalInvocationID.y >= inputImageSize.y)
        return;

    if (u_PushConstant.ClearImage == 1.0f)
    {
        imageStore(u_OutputImage, ivec2(gl_GlobalInvocationID).xy, vec4(0.0f));
        return;
    }


    const ivec2 diffInputOutputImageSize = outputImageSize - inputImageSize;
    bool isTextureBigger = diffInputOutputImageSize.x < 0 || diffInputOutputImageSize.y < 0;
    vec3 color = isTextureBigger ? SampleDownsampledTexture(ivec2(gl_GlobalInvocationID.xy))
                                 : SampleTexture(ivec2(gl_GlobalInvocationID.xy));
    //texelFetch(u_InputImage, ivec2(gl_GlobalInvocationID.xy), 0);

    if(u_PushConstant.UsePrefiltering == 1.0)
        color.rgb = Prefilter(vec4(color, 0.0f));
    
    vec2 uv = (vec2(gl_GlobalInvocationID.xy) + 0.5) / inputImageSize;

    ivec2 offsets = (outputImageSize - inputImageSize) / 2;
    if (isTextureBigger == true)
    {
        float ratioInputImg = vec2(inputImageSize).x / vec2(inputImageSize).y;
        vec2 adjustedInputSize = vec2(vec2(outputImageSize).x, vec2(outputImageSize).y * (1.0f / ratioInputImg));
        adjustedInputSize = adjustedInputSize - BLOOM_CONV_OFFSET;
        offsets = (outputImageSize - ivec2(adjustedInputSize)) / 2;
    }
    imageStore(u_OutputImage, offsets + ivec2(gl_GlobalInvocationID).xy, vec4(color.rgb, 0.0));
}