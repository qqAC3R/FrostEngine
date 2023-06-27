#pragma once

#include "Frost/Core/Buffer.h"
#include "Frost/Asset/Asset.h"
#include "Frost/Renderer/Image.h"

namespace Frost
{
	struct TextureSpecification
	{
		ImageFormat Format = ImageFormat::RGBA8;
		ImageUsage Usage = ImageUsage::Storage;
		SamplerProperties Sampler{};
		bool FlipTexture = false;
		bool UseMips = false;
	};

	class Texture : public Asset
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

		virtual Buffer GetWritableBuffer() = 0; // CPU Buffer
		virtual void SetToWriteableBuffer(void* data) = 0; // CPU Buffer
		virtual void SubmitDataToGPU() = 0;

		static AssetType GetStaticType() { return AssetType::Texture; }
		virtual AssetType GetAssetType() const override { return AssetType::Texture; }

		static Ref<Texture2D> Create(const std::string& filepath, TextureSpecification textureSpec = {});
		static Ref<Texture2D> Create(uint32_t width, uint32_t height, TextureSpecification textureSpec = {}, const void* data = nullptr);
	};

	class TextureCubeMap : public Image
	{
	public:
		static Ref<TextureCubeMap> Create(ImageSpecification imageSpec = {});
	};

	class Texture3D : public Image
	{
	public:
		virtual uint32_t GetDepth() const = 0;
		static Ref<Texture3D> Create(ImageSpecification imageSpec = {});
	};
}