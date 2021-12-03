#pragma once

#include "Frost/Renderer/Image.h"

namespace Frost
{
	struct TextureSpecification
	{
		ImageFormat Format = ImageFormat::RGBA8;
		ImageUsage Usage = ImageUsage::Storage;
		SamplerProperties Sampler{};
		bool UseMips = false;
	};

	class Texture
	{
	public:
		virtual ~Texture() {}

		virtual void Destroy() = 0;

		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;
		virtual uint32_t GetMipChainLevels() const = 0;

		virtual TextureSpecification& GetSpecification() = 0;
		virtual const TextureSpecification& GetSpecification() const = 0;

		virtual void GenerateMipMaps() = 0;

		virtual bool Loaded() const = 0;
	};

	class Texture2D : public Texture
	{
	public:
		virtual const Ref<Image2D> GetImage2D() const = 0;
		static Ref<Texture2D> Create(const std::string& filepath, TextureSpecification textureSpec = {});
	};

	class TextureCubeMap : public Image
	{
	public:
		static Ref<TextureCubeMap> Create(ImageSpecification imageSpec = {});
	};
}