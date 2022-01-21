#include "frostpch.h"
#include "VulkanTexture.h"

#include "VulkanImage.h"

#include "Frost/Platform/Vulkan/VulkanRenderer.h"
#include "Frost/Platform/Vulkan/VulkanContext.h"

#include <stb_image.h>

namespace Frost
{
	/////////////////////////////////////////////////////
	// VULKAN TEXTURE 2D
	/////////////////////////////////////////////////////
	VulkanTexture2D::VulkanTexture2D(const std::string& filepath, const TextureSpecification& textureSpec)
		: m_TextureSpecification(textureSpec)
	{
		// Loading the texture
		int width, height, channels;
		ImageFormat imageFormat;

		void* textureData = nullptr;
		if (stbi_is_hdr(filepath.c_str()))
		{
			textureData = (void*)stbi_loadf(filepath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
			imageFormat = ImageFormat::RGBA32F;
		}
		else
		{
			stbi_set_flip_vertically_on_load(true);
			textureData = (void*)stbi_load(filepath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
			imageFormat = ImageFormat::RGBA8;
		}

		if (textureData == nullptr)
		{
			FROST_CORE_WARN("Texture with filepath '{0}' hasn't been found", filepath);
			m_IsLoaded = false;
			return;
		}

		m_Width = width;
		m_Height = height;
		m_TextureSpecification.Format = imageFormat;
		if (textureSpec.UseMips)
			m_MipMapLevels = Utils::CalculateMipMapLevels(width, height);

		ImageSpecification imageSpec{};
		imageSpec.Width = width;
		imageSpec.Height = height;
		imageSpec.UseMipChain = textureSpec.UseMips;
		imageSpec.Sampler.SamplerFilter = ImageFilter::Linear;
		imageSpec.Sampler.SamplerWrap = ImageWrap::Repeat;
		imageSpec.Usage = ImageUsage::Storage;
		imageSpec.Format = imageFormat;
		m_Image = Image2D::Create(imageSpec, textureData);

		Ref<VulkanImage2D> vulkanImage = m_Image.As<VulkanImage2D>();
		m_DescriptorInfo[DescriptorImageType::Sampled] = vulkanImage->GetVulkanDescriptorInfo(DescriptorImageType::Sampled);
		m_DescriptorInfo[DescriptorImageType::Storage] = vulkanImage->GetVulkanDescriptorInfo(DescriptorImageType::Storage);

		stbi_image_free(textureData);
	}

	void VulkanTexture2D::GenerateMipMaps()
	{
		// TODO: Im not sure if this works while another command buffer is being recorded
		VkCommandBuffer cmdBuf = VulkanContext::GetCurrentDevice()->AllocateCommandBuffer(RenderQueueType::Graphics, true);
		auto vulkanImage = m_Image.As<VulkanImage2D>();
		vulkanImage->GenerateMipMaps(cmdBuf, vulkanImage->GetVulkanImageLayout());
		VulkanContext::GetCurrentDevice()->FlushCommandBuffer(cmdBuf);
	}

	void VulkanTexture2D::Destroy()
	{
		if (m_IsLoaded)
		{
			m_Image->Destroy();
		}
	}

	VulkanTexture2D::~VulkanTexture2D()
	{
		Destroy();
	}

	/////////////////////////////////////////////////////
	// VULKAN TEXTURE CUBEMAP
	/////////////////////////////////////////////////////
	VulkanTextureCubeMap::VulkanTextureCubeMap(const ImageSpecification& imageSpecification)
		: m_ImageSpecification(imageSpecification), m_ImageLayout(VK_IMAGE_LAYOUT_UNDEFINED)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		VkFormat textureFormat = Utils::GetImageFormat(imageSpecification.Format);
		VkImageUsageFlags usageFlags = Utils::GetImageUsageFlags(imageSpecification.Usage);

		// Calculate the mip chain levels
		if (imageSpecification.UseMipChain)
			m_MipLevelCount = Utils::CalculateMipMapLevels(imageSpecification.Width, imageSpecification.Height);
		else
			m_MipLevelCount = 1;


		Utils::CreateImage(imageSpecification.Width, imageSpecification.Height, 6, m_MipLevelCount,
			VK_IMAGE_TYPE_2D, textureFormat,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | usageFlags,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			m_Image, m_ImageMemory
		);

		VkImageLayout textureLayout = Utils::GetImageLayout(imageSpecification.Usage);

		// Recording a temporary commandbuffer for transitioning
		VkCommandBuffer cmdBuf = VulkanContext::GetCurrentDevice()->AllocateCommandBuffer(RenderQueueType::Graphics, true);

		// Generating mip maps if necessary and changing the layout to SHADER_READ_ONLY_OPTIMAL
		TransitionLayout(cmdBuf, textureLayout, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, Utils::GetPipelineStageFlagsFromLayout(textureLayout));
#if 0
		if (imageSpecification.Mips > 1)
		{
			m_ImageSpecification.Mips = Utils::CalculateMipMapLevels(imageSpecification.Width, imageSpecification.Height);
			TransitionLayout(cmdBuf, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, Utils::GetPipelineStageFlagsFromLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));

			GenerateMipMaps(cmdBuf, textureLayout);
		}
		else
		{
			TransitionLayout(cmdBuf, textureLayout, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, Utils::GetPipelineStageFlagsFromLayout(textureLayout));
		}
#endif

		// Ending the temporary commandbuffer for transitioning
		VulkanContext::GetCurrentDevice()->FlushCommandBuffer(cmdBuf);


		Utils::CreateImageView(m_ImageView, m_Image, usageFlags, textureFormat, m_MipLevelCount, 6);

		// Creating the imageView mips
		for (uint32_t i = 0; i < m_MipLevelCount; i++)
		{
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

			VkImageViewCreateInfo createInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
			createInfo.image = m_Image;
			createInfo.format = textureFormat;
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;

			createInfo.subresourceRange.baseMipLevel = i;
			createInfo.subresourceRange.levelCount = 1;

			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 6;
			switch (textureFormat)
			{
			case VK_FORMAT_D32_SFLOAT:
			case VK_FORMAT_D24_UNORM_S8_UINT:
				createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT; break;
			default:
				createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			}

			FROST_VKCHECK(vkCreateImageView(device, &createInfo, nullptr, &m_Mips[i]));
		}

		// Creating the sampler
		VkFilter filtering = Utils::GetImageFiltering(imageSpecification.Sampler.SamplerFilter);
		VkSamplerAddressMode wrap = Utils::GetImageWrap(imageSpecification.Sampler.SamplerWrap);
		VkSamplerMipmapMode mipMapMode = Utils::GetImageSamplerMipMapMode(imageSpecification.Sampler.SamplerFilter);
		Utils::CreateImageSampler(m_ImageSampler, filtering, wrap, mipMapMode, m_MipLevelCount);

		VulkanContext::SetStructDebugName("TextureCubeMap-Image", VK_OBJECT_TYPE_IMAGE, m_Image);
		VulkanContext::SetStructDebugName("TextureCubeMap-ImageView", VK_OBJECT_TYPE_IMAGE_VIEW, m_ImageView);
		VulkanContext::SetStructDebugName("TextureCubeMap-Sampler", VK_OBJECT_TYPE_SAMPLER, m_ImageSampler);

		UpdateDescriptor();
	}

	VulkanTextureCubeMap::~VulkanTextureCubeMap()
	{
		Destroy();
	}

	void VulkanTextureCubeMap::Destroy()
	{
		if (m_Image == VK_NULL_HANDLE) return;

		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		VulkanAllocator::DestroyImage(m_Image, m_ImageMemory);
		vkDestroyImageView(device, m_ImageView, nullptr);
		vkDestroySampler(device, m_ImageSampler, nullptr);

		for (auto& [mipLevel, imageView] : m_Mips)
			vkDestroyImageView(device, imageView, nullptr);

		m_Image = VK_NULL_HANDLE;
	}

	void VulkanTextureCubeMap::TransitionLayout(VkCommandBuffer cmdBuf, VkImageLayout newImageLayout,
		VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask)
	{
		VkImageSubresourceRange subresourceRange{};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.levelCount = m_MipLevelCount;
		subresourceRange.layerCount = 6; // 6 for cubemaps
		Utils::SetImageLayout(cmdBuf, m_Image, m_ImageLayout, newImageLayout, subresourceRange, srcStageMask, dstStageMask);

		m_ImageLayout = newImageLayout;
	}

	void VulkanTextureCubeMap::GenerateMipMaps(VkCommandBuffer cmdBuffer, VkImageLayout newImageLayout)
	{
		int32_t mipWidth = m_ImageSpecification.Width;
		int32_t mipHeight = m_ImageSpecification.Height;

		TransitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			Utils::GetPipelineStageFlagsFromLayout(m_ImageLayout),
			Utils::GetPipelineStageFlagsFromLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		);

		for (uint32_t i = 1; i < m_MipLevelCount; i++)
		{
			for (uint32_t face = 0; face < 6; face++)
			{
				VkImageSubresourceRange subresourceRange{};
				subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				subresourceRange.baseArrayLayer = face;
				subresourceRange.layerCount = 1;
				subresourceRange.levelCount = 1;
				subresourceRange.baseMipLevel = i - 1;


				VkImageBlit blit{};

				// Src
				blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				blit.srcOffsets[0] = { 0, 0, 0 };
				blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
				blit.srcSubresource.baseArrayLayer = face;
				blit.srcSubresource.layerCount = 1;
				blit.srcSubresource.mipLevel = i - 1;

				// Dst
				blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				blit.dstOffsets[0] = { 0, 0, 0 };
				blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
				blit.dstSubresource.baseArrayLayer = face;
				blit.dstSubresource.layerCount = 1;
				blit.dstSubresource.mipLevel = i;

				// Transition from `VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL` to `VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL`
				Utils::InsertImageMemoryBarrier(cmdBuffer, m_Image,
					Utils::GetAccessFlagsFromLayout(m_ImageLayout),
					Utils::GetAccessFlagsFromLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
					m_ImageLayout,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					Utils::GetPipelineStageFlagsFromLayout(m_ImageLayout),
					Utils::GetPipelineStageFlagsFromLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
					subresourceRange
				);

				vkCmdBlitImage(cmdBuffer,
					m_Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					m_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					1, &blit,
					VK_FILTER_LINEAR
				);

				// Transition from `VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL` to "newImageLayout"
				Utils::InsertImageMemoryBarrier(cmdBuffer, m_Image,
					Utils::GetAccessFlagsFromLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
					Utils::GetAccessFlagsFromLayout(newImageLayout),
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					newImageLayout,
					Utils::GetPipelineStageFlagsFromLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
					Utils::GetPipelineStageFlagsFromLayout(newImageLayout),
					subresourceRange
				);

			}

			if (mipWidth > 1) mipWidth /= 2;
			if (mipHeight > 1) mipHeight /= 2;
		}

		
		for (uint32_t face = 0; face < 6; face++)
		{
			VkImageSubresourceRange subresourceRange{};
			subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			subresourceRange.baseMipLevel = m_MipLevelCount - 1;
			subresourceRange.levelCount = 1;

			subresourceRange.baseArrayLayer = face;
			subresourceRange.layerCount = 1;

			Utils::InsertImageMemoryBarrier(cmdBuffer, m_Image,
				Utils::GetAccessFlagsFromLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
				Utils::GetAccessFlagsFromLayout(newImageLayout),
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				newImageLayout,
				Utils::GetPipelineStageFlagsFromLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
				Utils::GetPipelineStageFlagsFromLayout(newImageLayout),
				subresourceRange
			);
		}

		m_ImageLayout = newImageLayout;
	}

	void VulkanTextureCubeMap::UpdateDescriptor()
	{
		m_DescriptorInfo[DescriptorImageType::Sampled].imageView = m_ImageView;
		m_DescriptorInfo[DescriptorImageType::Sampled].imageLayout = m_ImageLayout;
		m_DescriptorInfo[DescriptorImageType::Sampled].sampler = m_ImageSampler;

		m_DescriptorInfo[DescriptorImageType::Storage].imageView = m_ImageView;
		m_DescriptorInfo[DescriptorImageType::Storage].imageLayout = m_ImageLayout;
		m_DescriptorInfo[DescriptorImageType::Storage].sampler = nullptr;
	}

}