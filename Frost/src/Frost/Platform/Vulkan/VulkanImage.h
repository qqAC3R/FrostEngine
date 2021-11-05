#pragma once

#include "Frost/Platform/Vulkan/Vulkan.h"
#include "Frost/Renderer/Image.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanBufferAllocator.h"

namespace Frost
{
	enum class DescriptorImageType {
		Sampled, Storage
	};

	class VulkanImage2D : public Image2D
	{
	public:
		VulkanImage2D(const ImageSpecification& specification);
		VulkanImage2D(const ImageSpecification& specification, const void* data);
		virtual ~VulkanImage2D();

		virtual void Destroy() override;

		void TransitionLayout(VkCommandBuffer cmdBuf, VkImageLayout newImageLayout,
			VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

		void GenerateMipMaps(VkCommandBuffer cmdBuffer, VkImageLayout newImageLayout);
		void BlitImage(VkCommandBuffer cmdBuf, const Ref<Image2D>& srcImage, uint32_t mipLevel = 0);

		virtual uint32_t GetWidth() const override { return m_ImageSpecification.Width; }
		virtual uint32_t GetHeight() const override { return m_ImageSpecification.Height; }

		virtual ImageSpecification& GetSpecification() override { return m_ImageSpecification; }
		virtual const ImageSpecification& GetSpecification() const override { return m_ImageSpecification; }

		VkSampler GetVulkanSampler() const { return m_ImageSampler; }
		VkImage GetVulkanImage() const { return m_Image; }
		VkImageView GetVulkanImageView() const { return m_ImageView; }
		VkImageLayout GetVulkanImageLayout() const { return m_ImageLayout; }

		VkDescriptorImageInfo& GetVulkanDescriptorInfo(DescriptorImageType imageType) {
			FROST_ASSERT_INTERNAL((m_DescriptorInfo.find(imageType) != m_DescriptorInfo.end()));
			return m_DescriptorInfo[imageType];
		}
	private:
		void UpdateDescriptor();
	private:
		VkImage m_Image = VK_NULL_HANDLE;
		VulkanMemoryInfo m_ImageMemory;

		VkImageView m_ImageView;
		VkSampler m_ImageSampler;
		VkImageLayout m_ImageLayout;

		ImageSpecification m_ImageSpecification;
		HashMap<DescriptorImageType, VkDescriptorImageInfo> m_DescriptorInfo;

		friend class VulkanRenderPass;
	};

	namespace Utils
	{
		void CreateImage(uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevels,
			VkImageType type, VkFormat format, VkImageTiling tiling,
			VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
			VkImage& image, VulkanMemoryInfo& imageMemory
		);
		void CreateImageView(VkImageView& imageView, VkImage image, VkImageUsageFlags imageUsage, VkFormat format, uint32_t mipLevels, uint32_t textureDepth);
		void CreateImageSampler(VkSampler& sampler, VkFilter filtering, VkSamplerAddressMode samplerAdressMode, uint32_t mipLevels);

		void CopyBufferToImage(VkCommandBuffer cmdBuf, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t depth);

		VkFormat GetImageFormat(ImageFormat imageFormat);
		VkFilter GetImageFiltering(ImageFilter imageFiltering);
		VkSamplerAddressMode GetImageWrap(ImageWrap imageWrap);
		VkImageUsageFlags GetImageUsageFlags(ImageUsage imageUsage);
		VkImageLayout GetImageLayout(ImageUsage imageUsage);
		VkAccessFlags GetAccessFlagsFromLayout(VkImageLayout imageLayout);
		VkPipelineStageFlags GetPipelineStageFlagsFromLayout(VkImageLayout imageLayout);
		VkDeviceSize CalculateImageBufferSize(uint32_t width, uint32_t height, ImageFormat imageFormat);
	}
}