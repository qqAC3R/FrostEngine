#pragma once

namespace Frost
{

	enum class ImageFormat
	{
		None = 0,

		// Color
		RGBA8, RGBA16F, RGBA32F,

		// Depth/Stencil
		Depth24Stencil8, Depth32
	};

	enum class ImageUsage
	{
		None = 0,
		Storage,
		Attachment,
		DepthStencil
	};

	enum class ImageFilter
	{
		Linear, Nearest
	};

	enum class ImageWrap
	{
		Repeat
	};

	struct SamplerProperties
	{
		ImageWrap SamplerWrap = ImageWrap::Repeat;
		ImageFilter SamplerFilter = ImageFilter::Linear;
	};

	struct ImageSpecification
	{
		ImageFormat Format = ImageFormat::RGBA8;
		ImageUsage Usage = ImageUsage::Storage;
		SamplerProperties Sampler{};
		uint32_t Width = 0;
		uint32_t Height = 0;
		uint32_t Mips = 1;
	};

	class Image
	{
	public:
		virtual ~Image() {}

		virtual void Destroy() = 0;

		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;

		virtual ImageSpecification& GetSpecification() = 0;
		virtual const ImageSpecification& GetSpecification() const = 0;
	};

	class Image2D : public Image
	{
	public:
		static Ref<Image2D> Create(const ImageSpecification& specification);
		static Ref<Image2D> Create(const ImageSpecification& specification, const void* data);
	};

}