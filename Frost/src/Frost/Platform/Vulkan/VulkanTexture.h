#pragma once

#include "Frost/Renderer/Texture.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanBufferAllocator.h"

namespace Frost
{
	class VulkanTexture2D : public Texture2D
	{
	public:
		VulkanTexture2D(const std::string& path, const TextureSpecs& spec);
		virtual ~VulkanTexture2D();

		virtual uint32_t GetWidth() const override { return m_TextureSpecification.Width; }
		virtual uint32_t GetHeight() const override { return m_TextureSpecification.Height; }
		virtual uint32_t GetRendererID() const override { return 0; }
		virtual const TextureSpecs& GetTextureSpecification() const override { return m_TextureSpecification; }

		VkSampler GetVulkanSampler() const { return m_TextureSampler; }
		VkImage GetVulkanImage() const { return m_TextureImage; }
		VkImageView GetVulkanImageView() const { return m_ImageView; }
		VkImageLayout GetVulkanImageLayout() const { return m_Layout; }
		VkDescriptorImageInfo& GetVulkanDescriptorInfo() { return m_DescriptorInfo; }

		virtual void Bind(uint32_t slot = 0) const override {}
		virtual void Unbind(uint32_t slot = 0) const override {}

		virtual void Destroy() const override;

	private:
		void UpdateDescriptor();
	private:
		VkImage m_TextureImage;
		VkImageView m_ImageView;
		VkImageLayout m_Layout;
		VulkanMemoryInfo m_ImageMemory;

		VkDescriptorImageInfo m_DescriptorInfo;

		VkSampler m_TextureSampler;
		TextureSpecs m_TextureSpecification;
	};

	class VulkanImage2D : public Image2D
	{
	public:
		VulkanImage2D(const TextureSpecs& spec);
		virtual ~VulkanImage2D();

		virtual uint32_t GetWidth() const override { return m_TextureSpecification.Width; }
		virtual uint32_t GetHeight() const override { return m_TextureSpecification.Height; }
		virtual uint32_t GetRendererID() const override { return 0; }
		virtual const TextureSpecs& GetTextureSpecification() const override { return m_TextureSpecification; }

		VkSampler GetVulkanSampler() const { return m_TextureSampler; }
		VkImage GetVulkanImage() const { return m_TextureImage; }
		VkImageView GetVulkanImageView() const { return m_ImageView; }
		VkImageLayout GetVulkanImageLayout() const { return m_Layout; }
		VkDescriptorImageInfo& GetVulkanDescriptorInfo() { return m_DescriptorInfo; }

		virtual void Bind(uint32_t slot = 0) const override {}
		virtual void Unbind(uint32_t slot = 0) const override {}

		virtual void Destroy() const override;

		void TransitionLayout(VkCommandBuffer cmdBuf, VkImageLayout newLayout,
			VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

	private:
		void UpdateDescriptor();
	private:
		VkImage m_TextureImage;
		VkImageView m_ImageView;
		VkImageLayout m_Layout;
		VulkanMemoryInfo m_ImageMemory;

		VkDescriptorImageInfo m_DescriptorInfo;

		VkSampler m_TextureSampler;
		TextureSpecs m_TextureSpecification;

		friend class VulkanRenderPass;
	};

	class VulkanTextureCubeMap : public TextureCubeMap
	{
	public:
		VulkanTextureCubeMap(const TextureSpecs& spec);
		virtual ~VulkanTextureCubeMap();

		virtual uint32_t GetWidth() const override { return m_TextureSpecification.Width; }
		virtual uint32_t GetHeight() const override { return m_TextureSpecification.Height; }
		virtual uint32_t GetRendererID() const override { return 0; }
		virtual const TextureSpecs& GetTextureSpecification() const override { return m_TextureSpecification; }

		VkSampler GetVulkanSampler() const { return m_TextureSampler; }
		VkImage GetVulkanImage() const { return m_TextureImage; }
		VkImageView GetVulkanImageView() const { return m_ImageView; }
		VkImageLayout GetVulkanImageLayout() const { return m_Layout; }
		VkDescriptorImageInfo& GetVulkanDescriptorInfo() { return m_DescriptorInfo; }

		virtual void Bind(uint32_t slot = 0) const override {}
		virtual void Unbind(uint32_t slot = 0) const override {}

		virtual void Destroy() const override;

	private:
		void UpdateDescriptor();
	private:
		VkImage m_TextureImage;
		VkImageView m_ImageView;
		VkImageLayout m_Layout;
		VulkanMemoryInfo m_ImageMemory;

		VkDescriptorImageInfo m_DescriptorInfo;

		VkSampler m_TextureSampler;
		TextureSpecs m_TextureSpecification;
	};

	namespace Utils
	{
		VkFormat TextureFormatToVk(TextureSpecs::FormatSpec format);
		VkFilter TextureFilteringToVk(TextureSpecs::FilteringSpec filtering);
		VkImageUsageFlags TextureUsageToVk(Vector<TextureSpecs::UsageSpec> usages);
		VkImageLayout TextureUsageToLayoutVk(TextureSpecs::UsageSpec usages);
		VkAccessFlags AccessFlagsToImageLayout(VkImageLayout layout);
		VkPipelineStageFlags PipelineStageForLayout(VkImageLayout layout);

		void CreateImage(uint32_t width, uint32_t height, uint32_t texureDepth, uint32_t mipLevels,
						 VkFormat format, VkImageType type, VkImageTiling tiling,
						 VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
						 VkImage& image, VulkanMemoryInfo& imageMemory);
		void CreateImageView(VkImageView& imageView, VkImage& image, VkImageUsageFlags imageUsage, VkFormat format, uint32_t mipLevels, uint32_t textureDepth);
		void CreateImageSampler(VkFilter filtering, uint32_t mipLevels, VkSampler& sampler);

		void ChangeImageLayout(VkImage& image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels, uint32_t depthMap);
		void CopyBufferToImage(const VkBuffer& buffer, VkImage& image, uint32_t width, uint32_t height);

		void GenerateMipMaps(VkImage image, VkFormat imageFormat, VkImageLayout newLayout, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);
	}

}