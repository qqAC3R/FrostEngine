#pragma once

#include "Frost/Platform/Vulkan/Vulkan.h"

#include "Frost/Renderer/Image.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanBufferAllocator.h"

namespace Frost
{
#if 0
	class VulkanImage2DD : public Image2DD
	{
	public:
		VulkanImage2DD(ImageSpecification specification);
		VulkanImage2DD(ImageSpecification specification, const void* data);
		virtual ~VulkanImage2DD();

		virtual void Invalidate() override;
		virtual void Destroy() override;

		virtual uint32_t GetWidth() const override { return m_ImageSpecification.Width; }
		virtual uint32_t GetHeight() const override { return m_ImageSpecification.Height; }

		virtual ImageSpecification& GetSpecification() override { return m_ImageSpecification; }
		virtual const ImageSpecification& GetSpecification() const override { return m_ImageSpecification; }

		VkSampler GetVulkanSampler() const { return m_Sampler; }
		VkImage GetVulkanImage() const { return m_Image; }
		VkImageView GetVulkanImageView() const { return m_ImageView; }
		VkImageLayout GetVulkanImageLayout() const { return m_ImageLayout; }

		VkDescriptorImageInfo GetDescriptorInfo() const { return m_DescriptorInfo; }

	private:
		VkImage m_Image;
		VkImageView m_ImageView;
		VkImageLayout m_ImageLayout;
		VkSampler m_Sampler;

		VulkanMemoryInfo m_ImageMemory;

		ImageSpecification m_ImageSpecification;
		VkDescriptorImageInfo m_DescriptorInfo;
	};

	namespace Utils
	{
		VkFormat GetVulkanImageFormat(ImageFormat format)
		{
			switch (format)
			{
				case ImageFormat::RGBA8:    return VK_FORMAT_R8G8B8A8_SRGB;
				case ImageFormat::RGBA16F:  return VK_FORMAT_R16G16B16A16_SFLOAT;
				case ImageFormat::DEPTH32:  return VK_FORMAT_D32_SFLOAT;
			}

			FROST_ASSERT(false, "Image format not deteced");
			return VK_FORMAT_UNDEFINED;
		}

		VkImageLayout GetVulkanImageLayout(ImageUsage usage)
		{
			switch (usage)
			{
			case ImageUsage::Attachment:	return VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;
			case ImageUsage::Texture:		return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			case ImageUsage::Storage:		return VK_IMAGE_LAYOUT_GENERAL;
			case ImageUsage::DepthStencil:	return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			}

			return VK_IMAGE_LAYOUT_UNDEFINED;
		}

		VkFilter GetVulkanSamplerFiltering(TextureFilter filter)
		{
			switch (filter)
			{
			case TextureFilter::Nearest: return VK_FILTER_NEAREST;
			case TextureFilter::Linear:	 return VK_FILTER_LINEAR;
			}

			return VkFilter();
		}

		VkAccessFlags AccessFlagsToImageLayout(VkImageLayout layout)
		{
			switch (layout)
			{
				case VK_IMAGE_LAYOUT_PREINITIALIZED:				   return VK_ACCESS_HOST_WRITE_BIT;
				case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:			   return VK_ACCESS_TRANSFER_WRITE_BIT;
				case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:			   return VK_ACCESS_TRANSFER_READ_BIT;
				case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:		   return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:		   return VK_ACCESS_SHADER_READ_BIT;
			}

			return VkAccessFlags();
		}

		VkPipelineStageFlags PipelineStageForLayout(VkImageLayout layout)
		{
			switch (layout)
			{
			case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:			   return VK_PIPELINE_STAGE_TRANSFER_BIT;
			case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:		   return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:		   return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			case VK_IMAGE_LAYOUT_PREINITIALIZED:				   return VK_PIPELINE_STAGE_HOST_BIT;
			case VK_IMAGE_LAYOUT_UNDEFINED:						   return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			}

			return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		}
	}
#endif

}