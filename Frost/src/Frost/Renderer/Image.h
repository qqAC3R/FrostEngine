#pragma once

namespace Frost
{

	enum class ImageFormat
	{
		None = 0,

		// Color
		R8, R32,
		RG32F,
		RGBA8, RGBA16F, RGBA32F,

		// Depth/Stencil
		Depth24Stencil8, Depth32
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
		Linear, Nearest
	};

	enum class ImageWrap
	{
		Repeat, ClampToEdge
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
		static Ref<Image2D> Create(const ImageSpecification& specification, const void* data);
	};

	namespace Utils
	{
		uint32_t CalculateMipMapLevels(uint32_t width, uint32_t height);
	}

}