#pragma once

#include "Frost/Core/Buffer.h"

namespace Frost
{
	enum class ImageFormat
	{
		None = 0,

		// F - float
		// I - int
		R8 = 1, R16F = 2, R32F = 3, R32I = 4,
		RG16F = 5, RG32F = 6,

		// F - float
		// UNORM - float with range [0, 1]
		RGBA8 = 7, RGBA16F = 8, RGBA16UNORM = 9, RGBA32F = 10,

		SRGBA8 = 11,

		// The BC1/BC3 format should used mostly for albedo maps
		RGB_BC1 = 12, RGBA_BC1 = 13, BC2 = 14, BC3 = 15,
		
		// This format is mainly used for tangent space and normals maps.
		// Because of that, it stores the compressed data in a high quality format.
		// The RG compononents are the only channel in the texture,
		// and the B component should be computed independetly in the pixel shader.
		// The formula: B = sqrt(1 - R^2 - G^2);
		BC4 = 16, BC5 = 17,

		// Depth/Stencil
		Depth24Stencil8 = 18, Depth32 = 19
	};

	enum class ImageUsage
	{
		None = 0,
		ReadOnly,
		Storage,
		ColorAttachment,
		DepthStencil
	};

	enum class ImageFilter
	{
		// Hardware interpolation between pixels
		Linear,
		
		// No interpolation
		Nearest
	};

	enum class ImageWrap
	{
		Repeat, ClampToEdge
	};

	enum class ImageTiling
	{
		// Optimal for GPU to acces, but unreadable for CPU
		Optimal,

		// Layout is linear and accesible by the CPU, but for GPU is not that efficient as optimal
		Linear   
	};
	
	enum class ImageMemoryProperties
	{
		// Memory will be mappable on host. It usually means CPU(system) memory.
		CPU_ONLY,

		// Memory that is both mappable on host (guarantees to be `HOST_VISIBLE`) and preferably fast to access by GPU.
		// Usage: Resources written frequently by host (dynamic), read by device. E.g. textures (with LINEAR layout), vertex buffers, uniform buffers updated every frame or every draw call.
		CPU_TO_GPU,
		
		// Memory mappable on host (guarantees to be `HOST_VISIBLE`) and cached.
		// Usage: Resources written by device, read by host - results of some computations, e.g.screen capture, average scene luminance for HDR tone mapping.
		GPU_TO_CPU,

		// Memory will be used on device only, so fast access from the device is preferred.
		GPU_ONLY
	};

	enum class ReductionMode
	{
		None, Min, Max
	};

	struct SamplerProperties
	{
		ImageWrap SamplerWrap = ImageWrap::Repeat;
		ImageFilter SamplerFilter = ImageFilter::Linear;
		ReductionMode ReductionMode_Optional = ReductionMode::None; // Optional
	};

	struct ImageSpecification
	{
		ImageFormat Format = ImageFormat::RGBA8;
		ImageUsage Usage = ImageUsage::Storage;
		SamplerProperties Sampler{};
		uint32_t Width = 0;
		uint32_t Height = 0;
		uint32_t Depth = 1;
		bool UseMipChain = true;
		bool MutableFormat = false; // Optional: Currently works only for 3D textures
		//bool UseCompression = false;

		//  This is mostly used for the compression textures, in which case, the vulkan api needs a pointer to access the necesarry data
		void* AdditionalHandle = nullptr; 

		// Optional (Advanced users), currently supported only by Image2D
		ImageTiling Tiling = ImageTiling::Optimal;
		ImageMemoryProperties MemoryProperties = ImageMemoryProperties::GPU_ONLY;
	};

	class Image
	{
	public:
		virtual ~Image() {}

		virtual void Destroy() = 0;

		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;
		virtual uint32_t GetMipChainLevels() const = 0;
		virtual std::tuple<uint32_t, uint32_t> GetTextureSize() const = 0;

		virtual uint32_t GetMipWidth(uint32_t mip) = 0;
		virtual uint32_t GetMipHeight(uint32_t mip) = 0;
		virtual std::tuple<uint32_t, uint32_t> GetMipSize(uint32_t mip) = 0;

		virtual ImageSpecification& GetSpecification() = 0;
		virtual const ImageSpecification& GetSpecification() const = 0;
	};

	class Image2D : public Image
	{
	public:
		static Ref<Image2D> Create(const ImageSpecification& specification);
		static Ref<Image2D> Create(const ImageSpecification& specification, const Buffer& data);
	};

	namespace Utils
	{
		uint32_t CalculateMipMapLevels(uint32_t width, uint32_t height);
	}

}