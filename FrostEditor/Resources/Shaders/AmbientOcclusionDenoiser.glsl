#type compute
#version 460

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(binding = 0) uniform sampler2D u_AOTexture;
layout(binding = 1) uniform sampler2D u_DepthTexture;
layout(binding = 2) uniform sampler2D u_FilteredHalfTexture;
layout(binding = 3) uniform sampler2D u_NormalTexture;
layout(binding = 4) restrict writeonly uniform image2D o_Texture;

/* 
    1) Render - AO (at half the resolution + noisy)
    2) Blur - Algorithm (Joined Bilateral Gaussian Blur)
    3) Upsample - Algorithm (Joined Bilateral Upsample)
*/

layout(push_constant) uniform PushConstant {
	uint IsUpsamplePass;
} u_PushConstant;


float Gaussian(float sigma, float x)
{
    return exp(-(x*x) / (2.0 * sigma*sigma));
}

const vec2 c_offset[4] = vec2[](
    vec2(0.0, 0.0),
    vec2(0.0, 1.0),
    vec2(1.0, 0.0),
    vec2(1.0, 1.0)
);

float JoinedBilateralUpsample(vec2 P)
{ // based on: https://johanneskopf.de/publications/jbu/paper/FinalPaper_0185.pdf
  //           https://bartwronski.com/2019/09/22/local-linear-models-guided-filter/
  //		   https://www.shadertoy.com/view/MllSzX
    
    vec2 halfP = P;
    vec2 c_textureSize = vec2(textureSize(u_DepthTexture, 0));
	vec2 c_texelSize = 1.0 / c_textureSize;
    vec2 pixel = halfP * c_textureSize + 0.5;
    vec2 f = fract(pixel);
    pixel = (floor(pixel) / c_textureSize) - vec2(c_texelSize/2.0);
    
    float I = textureLod(u_DepthTexture, P, 0.0).g;
        
    float Z00 = textureLod(u_DepthTexture, pixel + c_texelSize * c_offset[0], 1.0).g;
    float Z01 = textureLod(u_DepthTexture, pixel + c_texelSize * c_offset[1], 1.0).g;
    float Z10 = textureLod(u_DepthTexture, pixel + c_texelSize * c_offset[2], 1.0).g;
    float Z11 = textureLod(u_DepthTexture, pixel + c_texelSize * c_offset[3], 1.0).g;
    
    float tex00 = textureLod(u_FilteredHalfTexture, pixel + c_texelSize * c_offset[0], 0.0).r;
    float tex01 = textureLod(u_FilteredHalfTexture, pixel + c_texelSize * c_offset[1], 0.0).r;
    float tex10 = textureLod(u_FilteredHalfTexture, pixel + c_texelSize * c_offset[2], 0.0).r;
    float tex11 = textureLod(u_FilteredHalfTexture, pixel + c_texelSize * c_offset[3], 0.0).r;
       
    //float sigmaV = 0.005;
    float sigmaV = 1;
    //    wXX = bilateral gaussian weight from depth * bilinear weight
    float w00 = Gaussian(sigmaV, abs(I - Z00) ) * (1.0 - f.x) * (1.0 - f.y);
    float w01 = Gaussian(sigmaV, abs(I - Z01) ) * (1.0 - f.x) *        f.y;
    float w10 = Gaussian(sigmaV, abs(I - Z10) ) *        f.x  * (1.0 - f.y);
    float w11 = Gaussian(sigmaV, abs(I - Z11) ) *        f.x  *        f.y;
        
    //return Z00;
	return ( (w00*tex00 + w01*tex01 + w10*tex10 + w11*tex11) / (w00+w01+w10+w11) );
}

float g_sigmaX = 3.0;
float g_sigmaY = 3.0;
float g_sigmaV = 1.0;

vec2 g_pixelSize = vec2(0.001);
vec2 g_pixelSizeGuide = vec2(0.001);

float JoinedBilateralGaussianBlur(vec2 uv)
{   
    const float c_halfSamplesX = 2.0;
	const float c_halfSamplesY = 2.0;

    float total = 0.0;
    float ret = 0.0;

    float pivot = texture(u_AOTexture, uv).r;
    
    for (float iy = -c_halfSamplesY; iy <= c_halfSamplesY; iy++)
    {
        float fy = Gaussian( g_sigmaY, iy );
        float offsety = iy * g_pixelSize.y;

        for (float ix = -c_halfSamplesX; ix <= c_halfSamplesX; ix++)
        {
            float fx = Gaussian( g_sigmaX, ix );
            float offsetx = ix * g_pixelSize.x;
            
            float value = texture(u_AOTexture, uv + vec2(offsetx, offsety)).r;
                        
            float fv = Gaussian( g_sigmaV, abs(value - pivot) );
            
            float weight = fx*fy*fv;
            total += weight;
            ret += weight * value.r;
        }
    }
        
    return ret / total;
}

void main()
{
	// NOTE: 4x4 filter offsets image
	ivec2 loc = ivec2(gl_GlobalInvocationID.xy);
	float totalao = 0.0;

	vec2 baseSize = vec2(textureSize(u_AOTexture, 0));
	vec2 uv = ((vec2(loc) + 0.5.xx) / vec2(imageSize(o_Texture)));
	//vec2 texelSize = 1.0 / baseSize;

    g_sigmaV      = 0.3;
    g_sigmaV += 0.001;

	//vec4 ao = smartDeNoise(u_AOTexture, uv, 7, 1.29f, 0.195f);
    if(u_PushConstant.IsUpsamplePass == 1)
        totalao = JoinedBilateralUpsample(uv);
    else
        totalao = JoinedBilateralGaussianBlur(uv);

	// NOTE: textureGather requires GL 4
	//totalao += Sample2x2(u_AOTexture, uv + texelSize * vec2(0, 0));
	//totalao += Sample2x2(u_AOTexture, uv + texelSize * vec2(2, 0));
	//totalao += Sample2x2(u_AOTexture, uv + texelSize * vec2(0, 2));
	//totalao += Sample2x2(u_AOTexture, uv + texelSize * vec2(2, 2));

	imageStore(o_Texture, ivec2(gl_GlobalInvocationID.xy), vec4(vec3(totalao), 1.0f));
}