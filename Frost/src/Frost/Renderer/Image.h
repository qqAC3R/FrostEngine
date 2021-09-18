#pragma once

namespace Frost
{

	enum class ImageFormat
	{
		None = 0,

		// Color
		RGBA8, RGBA16F,

		// Depth/Stencil
		DEPTH32
	};

	enum class ImageUsage
	{
		None = 0,
		Texture, // VK_GENERAL
		Attachment, // VK_ATTACHEMNT
		Storage, // VK_STORAGE
		DepthStencil // VK_STORAGE
	};

	enum class ShaderUsage
	{
		None = 0,

		// Rasterization
		Fragment,

		// Compute
		Compute,

		// Ray Tracing
		RayGen, AnyHit, ClosestHit, Miss, Intersection
	};

	enum class TextureFilter
	{
		Linear, Nearest
	};

	enum class TextureWrap
	{
		Repeat
	};

	struct SamplerProperties
	{
		TextureWrap SamplerWrap = TextureWrap::Repeat;
		TextureFilter SamplerFilter = TextureFilter::Linear;
	};

	struct ImageSpecification
	{
		ImageFormat Format = ImageFormat::RGBA8;
		ImageUsage Usage = ImageUsage::Texture;
		SamplerProperties Sampler{};
		Vector<ShaderUsage> ShaderUsage;
		uint32_t Width = 0;
		uint32_t Height = 0;
		uint32_t Mips = 1;
	};

	class Image
	{
	public:
		virtual ~Image() {}

		virtual void Invalidate() = 0;
		virtual void Destroy() = 0;

		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;

		virtual ImageSpecification& GetSpecification() = 0;
		virtual const ImageSpecification& GetSpecification() const = 0;
	};

	class Image2DD : public Image
	{
	public:
		static Ref<Image2DD> Create(ImageSpecification specification);
		static Ref<Image2DD> Create(ImageSpecification specification, const void* data = nullptr);
	};

}