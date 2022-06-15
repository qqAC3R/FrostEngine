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
		virtual uint32_t GetMipChainLevels() const override { return m_MipLevelCount; }
		virtual std::tuple<uint32_t, uint32_t> GetTextureSize() const override {
			return std::make_tuple(m_ImageSpecification.Width, m_ImageSpecification.Height);
		}

		virtual uint32_t GetMipWidth(uint32_t mip) override { return std::get<0>(m_MipSizes[mip]); }
		virtual uint32_t GetMipHeight(uint32_t mip) override { return std::get<1>(m_MipSizes[mip]); }
		virtual std::tuple<uint32_t, uint32_t> GetMipSize(uint32_t mip) override { return m_MipSizes[mip]; }


		virtual ImageSpecification& GetSpecification() override { return m_ImageSpecification; }
		virtual const ImageSpecification& GetSpecification() const override { return m_ImageSpecification; }

		VkSampler GetVulkanSampler() const { return m_ImageSampler; }
		VkImage GetVulkanImage() const { return m_Image; }
		VkImageView GetVulkanImageView() const { return m_ImageView; }
		VkImageLayout GetVulkanImageLayout() const { return m_ImageLayout; }

		VkImageView GetVulkanImageViewMip(uint32_t mipLevel) { return m_Mips[mipLevel]; }

		VkDescriptorImageInfo& GetVulkanDescriptorInfo(DescriptorImageType imageType) {
			FROST_ASSERT_INTERNAL((m_DescriptorInfo.find(imageType) != m_DescriptorInfo.end()));
			return m_DescriptorInfo[imageType];
		}
	private:
		void UpdateDescriptor();
		void CalculateMipSizes();
	private:
		VkImage m_Image = VK_NULL_HANDLE;
		VulkanMemoryInfo m_ImageMemory;

		VkImageView m_ImageView;
		VkSampler m_ImageSampler;
		VkImageLayout m_ImageLayout;

		uint32_t m_MipLevelCount;
		std::unordered_map<uint32_t, VkImageView> m_Mips;
		HashMap<uint32_t, std::tuple<uint32_t, uint32_t>> m_MipSizes;

		ImageSpecification m_ImageSpecification;
		HashMap<DescriptorImageType, VkDescriptorImageInfo> m_DescriptorInfo;

		friend class VulkanRenderPass;
	};

	namespace Utils
	{
		void CreateImage(uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevels,
			VkImageType type, VkFormat format, VkImageTiling tiling,
			VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
			VkImage& image, VulkanMemoryInfo& imageMemory,
			VkImageCreateFlags optionalFlags = 0
		);
		void CreateImageView(VkImageView& imageView, VkImage image, VkImageUsageFlags imageUsage, VkFormat format, uint32_t mipLevels, uint32_t textureDepth);
		void CreateImageSampler(VkSampler& sampler, VkFilter filtering, VkSamplerAddressMode samplerAdressMode, VkSamplerMipmapMode samplerMipMapMode, uint32_t mipLevels, VkSamplerReductionMode reductionMode = VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE);

		void CopyBufferToImage(VkCommandBuffer cmdBuf, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t depth);

		VkFormat GetImageFormat(ImageFormat imageFormat);
		VkFilter GetImageFiltering(ImageFilter imageFiltering);
		VkSamplerAddressMode GetImageWrap(ImageWrap imageWrap);
		VkSamplerMipmapMode GetImageSamplerMipMapMode(ImageFilter imageFiltering);
		VkImageUsageFlags GetImageUsageFlags(ImageUsage imageUsage);
		VkImageLayout GetImageLayout(ImageUsage imageUsage);
		VkAccessFlags GetAccessFlagsFromLayout(VkImageLayout imageLayout);
		VkSamplerReductionMode GetSamplerReductionMode(ReductionMode reductionMode);
		VkPipelineStageFlags GetPipelineStageFlagsFromLayout(VkImageLayout imageLayout);
		VkDeviceSize CalculateImageBufferSize(uint32_t width, uint32_t height, ImageFormat imageFormat);
	}
}