#type compute
#version 450 core

#define WORK_GROUP_SIZE 1024

layout(local_size_x = WORK_GROUP_SIZE, local_size_y = 1, local_size_z = 1) in;

layout(binding = 0) uniform sampler2D u_InputImage;
layout(binding = 1) uniform sampler2D u_KernelImage;
layout(rgba32f, binding = 2) uniform image2D u_FFTImage;

#define PI 3.14159265359
#define RADIX 4

layout(push_constant) uniform PushConstant
{
	float VerticalTransform;
	float IsInverse;
	float GenerateKernel;
} u_PushConstant;

shared float s_Data_Real[WORK_GROUP_SIZE * RADIX];
shared float s_Data_Imaginary[WORK_GROUP_SIZE * RADIX];

vec2 MultiplyComplex(vec2 a, vec2 b)
{
	return vec2(a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x);
}

void ButterflyRADIX2(vec2 data[2], vec2 twiddle, out vec2 result[2])
{
	result[0] = data[0] + data[1];
	result[1] = MultiplyComplex(twiddle, data[0] - data[1]);
}

void ButterflyRADIX4(vec2 data[4], vec2 twiddle, out vec2 result[4])
{
	vec2 twiddle2 = MultiplyComplex(twiddle, twiddle);
	vec2 twiddle3 = MultiplyComplex(twiddle, twiddle2);

	const vec2 jc = vec2(0.0, 1.0);

	vec2 ac   = data[0] + data[2];
	vec2 ac_n = data[0] - data[2];

	vec2 bd   = data[1] + data[3];
	vec2 bd_n = data[1] - data[3];

	vec2 jcbdn = MultiplyComplex(jc, bd_n);

	result[0] = ac + bd;
	result[2] = MultiplyComplex(twiddle2, ac - bd);

	if(u_PushConstant.IsInverse == 1.0)
	{
		result[1] = MultiplyComplex(twiddle, ac_n + jcbdn);
		result[3] = MultiplyComplex(twiddle3, ac_n - jcbdn);
	}
	else
	{
		result[1] = MultiplyComplex(twiddle, ac_n - jcbdn);
		result[3] = MultiplyComplex(twiddle3, ac_n + jcbdn);
	}
}


void ComputeButterfly(vec2 data[RADIX], vec2 twiddle, out vec2 result[RADIX]) 
{
#if RADIX == 2
	ButterflyRADIX2(data, twiddle, result);
#else
	ButterflyRADIX4(data, twiddle, result);
#endif
}

void PlaceAndReceiveData(uint fixedIndex, uint stride, uint workGroupSize, vec2 dataProcessed[RADIX], out vec2 data[RADIX])
{
	for (uint r = 0; r < RADIX; r++)
	{
		s_Data_Real[fixedIndex + r * stride]      = dataProcessed[r].x;
		s_Data_Imaginary[fixedIndex + r * stride] = dataProcessed[r].y;
	}

	memoryBarrierShared();
	barrier();

	for (int r = 0; r < RADIX; r++)
	{
		data[r].x = s_Data_Real[gl_LocalInvocationID.x + r * workGroupSize];
		data[r].y = s_Data_Imaginary[gl_LocalInvocationID.x + r * workGroupSize];
	}
}

void ComputeParallelFFT(bool useRealNumbers, ivec2 coords[RADIX], inout vec2 data[RADIX])
{
	const uint NDivRadix = WORK_GROUP_SIZE;
	const uint N = RADIX * WORK_GROUP_SIZE;

	const uint j = gl_LocalInvocationID.x;

	if(u_PushConstant.IsInverse == 1.0 || u_PushConstant.VerticalTransform == 1.0)
	{
		if(useRealNumbers)
		{
			for (int r = 0; r < RADIX; r++)
				data[r] = imageLoad(u_FFTImage, coords[r]).rg;
		}
		else
		{
			for (int r = 0; r < RADIX; r++)
				data[r] = imageLoad(u_FFTImage, coords[r]).ba;
		}
	}
	else
	{
		if (useRealNumbers)
		{
			for (int r = 0; r < RADIX; r++)
				data[r] = texelFetch(u_InputImage, coords[r], 0).rg;
		}
		else
		{
			for (int r = 0; r < RADIX; r++)
				data[r] = texelFetch(u_InputImage, coords[r], 0).ba;
		}
	}

	for (uint stride = 1; stride < N; stride *= RADIX)
	{
		uint strideFactor = stride * uint(j / stride);
		float angle = 2 * PI * strideFactor / N;

		if(u_PushConstant.IsInverse == 1.0)
			angle *= -1.0;

		vec2 twiddle = vec2(cos(angle), -sin(angle));
		vec2 dataProcessed[RADIX];
		ComputeButterfly(data, twiddle, dataProcessed);
		
		// idk wtf does it do
		uint fixedIndex = j % stride + strideFactor * RADIX;
		
		memoryBarrierShared();
		barrier();

		PlaceAndReceiveData(fixedIndex, stride, NDivRadix, dataProcessed, data);
	}

	if (u_PushConstant.IsInverse == 1.0)
	{
		for (int r = 0; r < RADIX; r++)
			data[r] /= N;
	}
	
}


void main()
{
	ivec2 coords[RADIX];
	ivec2 coords_old[RADIX];

	const uint NDivRadix = WORK_GROUP_SIZE;
	const uint N = RADIX * WORK_GROUP_SIZE;

	ivec2 imageDimensions;


	// Determining the coordonates for sampling the image based on the radix size
	if (u_PushConstant.VerticalTransform == 1.0)
	{
		imageDimensions = ivec2(gl_NumWorkGroups.x , N);
		for (int r = 0; r < RADIX; r++)
			coords[r] = ivec2(gl_GlobalInvocationID.x / NDivRadix, gl_GlobalInvocationID.x % NDivRadix + r * NDivRadix);
	}
	else
	{
		imageDimensions = ivec2(N, gl_NumWorkGroups.x);
		for (int r = 0; r < RADIX; r++)
			coords[r] = ivec2(gl_GlobalInvocationID.x % NDivRadix + r * NDivRadix, gl_GlobalInvocationID.x / NDivRadix);

		if(u_PushConstant.GenerateKernel == 1.0)
		{
			coords_old = coords;
			for (int r = 0; r < RADIX; r++)
				coords[r] = (coords[r] - ivec2(imageDimensions / 2 - 1) + ivec2(imageDimensions - 1)) % imageDimensions;
		}
	}

	// TODO: Inaccuracy in the naming convetion, vec2 - vec2(real, imaginary part)
	vec2 dataReal[RADIX], dataImaginary[RADIX];
	ComputeParallelFFT(true,  coords, dataReal);
	ComputeParallelFFT(false, coords, dataImaginary);

	if(u_PushConstant.GenerateKernel == 1.0)
	{
		if(u_PushConstant.VerticalTransform == 1.0)
		{
			// Set imaginary parts to 0
			for (int r = 0; r < RADIX; r++)
			{
				dataReal[r].y = 0.0;
				dataImaginary[r].y = 0.0;
			}
		}
		else
		{
			for (int r = 0; r < RADIX; r++)
				imageStore(u_FFTImage, coords_old[r], vec4(dataReal[r], dataImaginary[r]));
			return;
		}
	}
	else
	{
		if (u_PushConstant.VerticalTransform == 1.0 && u_PushConstant.IsInverse == 0.0)
		{
			vec4 kernelValues[RADIX];
			for (int r = 0; r < RADIX; r++)
				kernelValues[r] = texelFetch(u_KernelImage, coords[r], 0).rgba;

			for (int r = 0; r < RADIX; r++)
			{
				dataReal[r] *= kernelValues[r].x;
				dataImaginary[r] *= kernelValues[r].z;
			}
		}
	}

	for (int r = 0; r < RADIX; r++)
		imageStore(u_FFTImage, coords[r], vec4(dataReal[r], dataImaginary[r]));
}