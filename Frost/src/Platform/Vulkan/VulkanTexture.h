#pragma once

#include "Frost/Renderer/Texture.h"

#include "Platform/Vulkan/Buffers/VulkanBufferAllocator.h"

namespace Frost
{


	class VulkanTexture2D : public Texture2D
	{
	public:
		VulkanTexture2D(const std::string& path, const TextureSpecs& spec);
		virtual ~VulkanTexture2D();

		virtual uint32_t GetWidth() const override { return m_Width; }
		virtual uint32_t GetHeight() const override { return m_Height; }
		virtual uint32_t GetRendererID() const override { return 0; }

		virtual VkSampler GetVulkanSampler() const override { return m_TextureSampler; }
		virtual VkImage GetVulkanImage() const override { return m_TextureImage; }
		virtual VkImageView GetVulkanImageView() const override { return m_ImageView; }
		virtual VkImageLayout GetVulkanImageLayout() const override { return m_Layout; }

		virtual void Bind(uint32_t slot = 0) const override {}
		virtual void Unbind(uint32_t slot = 0) const override {}

		virtual void Destroy() const override;

	private:
		VkImage m_TextureImage;
		VkImageView m_ImageView;
		VkImageLayout m_Layout;

		VkSampler m_TextureSampler;
		uint32_t m_Width, m_Height;

		VulkanMemoryInfo m_ImageMemory;
	};

	class VulkanImage2D : public Image2D
	{
	public:
		VulkanImage2D(const TextureSpecs& spec);
		VulkanImage2D(void* image, const TextureSpecs& spec);

		virtual ~VulkanImage2D();

		virtual uint32_t GetWidth() const override { return m_Width; }
		virtual uint32_t GetHeight() const override { return m_Height; }
		virtual uint32_t GetRendererID() const override { return 0; }

		virtual VkSampler GetVulkanSampler() const override { return m_TextureSampler; }
		virtual VkImage GetVulkanImage() const override { return m_TextureImage; }
		virtual VkImageView GetVulkanImageView() const override { return m_ImageView; }
		virtual VkImageLayout GetVulkanImageLayout() const override { return m_Layout; }

		virtual void Bind(uint32_t slot = 0) const override {}
		virtual void Unbind(uint32_t slot = 0) const override {}

		virtual void Destroy() const override;

	private:
		VkImage m_TextureImage;
		VkImageView m_ImageView;
		VkImageLayout m_Layout;

		VkSampler m_TextureSampler;
		uint32_t m_Width, m_Height;
		
		VulkanMemoryInfo m_ImageMemory;

		bool m_UsingSwapChain = false;
	};


}