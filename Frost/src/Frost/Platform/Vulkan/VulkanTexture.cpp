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
			stbi_set_flip_vertically_on_load(false);
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
		imageSpec.Usage = textureSpec.Usage;
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
			CalculateMipSizes();
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
			createInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };

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
		subresourceRange.baseMipLevel = 0;
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

	void VulkanTextureCubeMap::CalculateMipSizes()
	{
		m_MipLevelCount = Utils::CalculateMipMapLevels(m_ImageSpecification.Width, m_ImageSpecification.Height);

		uint32_t mipWidth = m_ImageSpecification.Width;
		uint32_t mipHeight = m_ImageSpecification.Height;

		for (uint32_t mip = 0; mip < m_MipLevelCount; mip++)
		{
			if (mip != 0)
			{
				mipWidth /= 2;
				mipHeight /= 2;
			}
			auto mipSize = std::make_tuple(mipWidth, mipHeight);
			m_MipSizes[mip] = mipSize;
		}
	}


	/////////////////////////////////////////////////////
	// VULKAN TEXTURE 3D 
	/////////////////////////////////////////////////////

	static uint32_t GetMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties, VkPhysicalDeviceMemoryProperties memoryProperties, VkBool32* memTypeFound = nullptr)
	{
		for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
		{
			if ((typeBits & 1) == 1)
			{
				if ((memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
				{
					if (memTypeFound)
					{
						*memTypeFound = true;
					}
					return i;
				}
			}
			typeBits >>= 1;
		}

		if (memTypeFound)
		{
			*memTypeFound = false;
			return 0;
		}
		else
		{
			throw std::runtime_error("Could not find a matching memory type");
		}
	}

	VulkanTexture3D::VulkanTexture3D(const ImageSpecification& imageSpecification)
		: m_ImageSpecification(imageSpecification), m_ImageLayout(VK_IMAGE_LAYOUT_UNDEFINED)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
#if 0
		VkFormat textureFormat = Utils::GetImageFormat(imageSpecification.Format);
		VkImageLayout textureLayout = Utils::GetImageLayout(imageSpecification.Usage);

		VkPhysicalDeviceMemoryProperties memoryProperties;
		VkPhysicalDevice physicalDevice = VulkanContext::GetCurrentDevice()->GetPhysicalDevice();
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

		m_MipLevelCount = 1;

		VkImageCreateInfo imageCreateInfo { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		imageCreateInfo.imageType = VK_IMAGE_TYPE_3D;
		imageCreateInfo.format = textureFormat;
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.extent.width = imageSpecification.Width;
		imageCreateInfo.extent.height = imageSpecification.Height;
		imageCreateInfo.extent.depth = imageSpecification.Depth;
		// Set initial layout of the image to undefined
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
		vkCreateImage(device, &imageCreateInfo, nullptr, &m_Image);

		// Device local memory to back up image
		VkMemoryAllocateInfo memAllocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		VkMemoryRequirements memReqs = {};
		vkGetImageMemoryRequirements(device, m_Image, &memReqs);
		memAllocInfo.allocationSize = memReqs.size;
		memAllocInfo.memoryTypeIndex = GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memoryProperties);
		vkAllocateMemory(device, &memAllocInfo, nullptr, &m_DeviceMemory);
		vkBindImageMemory(device, m_Image, m_DeviceMemory, 0);


		// Create sampler
		VkSamplerCreateInfo sampler{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
		sampler.magFilter = VK_FILTER_LINEAR;
		sampler.minFilter = VK_FILTER_LINEAR;
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.mipLodBias = 0.0f;
		sampler.compareOp = VK_COMPARE_OP_NEVER;
		sampler.minLod = 0.0f;
		sampler.maxLod = 0.0f;
		sampler.maxAnisotropy = 1.0f;
		sampler.anisotropyEnable = VK_FALSE;
		sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		vkCreateSampler(device, &sampler, nullptr, &m_ImageSampler);

		// Create image view
		VkImageViewCreateInfo view{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		view.image = m_Image;
		view.viewType = VK_IMAGE_VIEW_TYPE_3D;
		view.format = textureFormat;
		view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view.subresourceRange.baseMipLevel = 0;
		view.subresourceRange.baseArrayLayer = 0;
		view.subresourceRange.layerCount = 1;
		view.subresourceRange.levelCount = 1;
		vkCreateImageView(device, &view, nullptr, &m_ImageView);



		// Recording a temporary commandbuffer for transitioning
		VkCommandBuffer cmdBuf = VulkanContext::GetCurrentDevice()->AllocateCommandBuffer(RenderQueueType::Graphics, true);

		// Generating mip maps if necessary and changing the layout to SHADER_READ_ONLY_OPTIMAL
		TransitionLayout(cmdBuf, textureLayout, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, Utils::GetPipelineStageFlagsFromLayout(textureLayout));

		// Ending the temporary commandbuffer for transitioning
		VulkanContext::GetCurrentDevice()->FlushCommandBuffer(cmdBuf);

#if 0
		// Device local memory to back up image
		VkMemoryAllocateInfo memAllocInfo { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		VkMemoryRequirements memReqs = {};
		vkGetImageMemoryRequirements(device, m_Image, &memReqs);
		memAllocInfo.allocationSize = memReqs.size;
		memAllocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAllocInfo, nullptr, &texture.deviceMemory));
		VK_CHECK_RESULT(vkBindImageMemory(device, texture.image, texture.deviceMemory, 0));
#endif
#endif


#if 1
		VkFormat textureFormat = Utils::GetImageFormat(imageSpecification.Format);
		VkImageUsageFlags usageFlags = Utils::GetImageUsageFlags(imageSpecification.Usage);

		// Calculate the mip chain levels
		if (imageSpecification.UseMipChain)
			CalculateMipSizes();
		else
			m_MipLevelCount = 1;

		
		VkImageCreateFlags optionalFlags = 0;

		if (imageSpecification.MutableFormat)
			optionalFlags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

		Utils::CreateImage(imageSpecification.Width, imageSpecification.Height, imageSpecification.Depth, m_MipLevelCount,
			VK_IMAGE_TYPE_3D, textureFormat,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | usageFlags,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			m_Image, m_ImageMemory,
			optionalFlags
		);

		VkImageLayout textureLayout = Utils::GetImageLayout(imageSpecification.Usage);


		// Recording a temporary commandbuffer for transitioning
		VkCommandBuffer cmdBuf = VulkanContext::GetCurrentDevice()->AllocateCommandBuffer(RenderQueueType::Graphics, true);

		// Generating mip maps if necessary and changing the layout to SHADER_READ_ONLY_OPTIMAL
		TransitionLayout(cmdBuf, textureLayout, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, Utils::GetPipelineStageFlagsFromLayout(textureLayout));

		// Ending the temporary commandbuffer for transitioning
		VulkanContext::GetCurrentDevice()->FlushCommandBuffer(cmdBuf);


		// Create image view
		Utils::CreateImageView(m_ImageView, m_Image, usageFlags, textureFormat, m_MipLevelCount, imageSpecification.Depth);

		// Creating the imageView mips
		if (m_MipLevelCount != 1)
		{
			for (uint32_t i = 0; i < m_MipLevelCount; i++)
			{
				VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

				VkImageViewCreateInfo createInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
				createInfo.image = m_Image;
				createInfo.format = textureFormat;
				createInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
				createInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };

				createInfo.subresourceRange.baseMipLevel = i;
				createInfo.subresourceRange.levelCount = 1;

				createInfo.subresourceRange.baseArrayLayer = 0;
				createInfo.subresourceRange.layerCount = 1;
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
		}

		// Creating the sampler
		VkFilter filtering = Utils::GetImageFiltering(imageSpecification.Sampler.SamplerFilter);
		VkSamplerAddressMode wrap = Utils::GetImageWrap(imageSpecification.Sampler.SamplerWrap);
		VkSamplerMipmapMode mipMapMode = Utils::GetImageSamplerMipMapMode(imageSpecification.Sampler.SamplerFilter);
		Utils::CreateImageSampler(m_ImageSampler, filtering, wrap, mipMapMode, m_MipLevelCount);


		VulkanContext::SetStructDebugName("Texture3D-Image", VK_OBJECT_TYPE_IMAGE, m_Image);
		VulkanContext::SetStructDebugName("Texture3D-ImageView", VK_OBJECT_TYPE_IMAGE_VIEW, m_ImageView);
		VulkanContext::SetStructDebugName("Texture3D-Sampler", VK_OBJECT_TYPE_SAMPLER, m_ImageSampler);

#endif
		UpdateDescriptor();
	}

	VulkanTexture3D::~VulkanTexture3D()
	{
		Destroy();
	}

	void VulkanTexture3D::Destroy()
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

	void VulkanTexture3D::TransitionLayout(VkCommandBuffer cmdBuf, VkImageLayout newImageLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask)
	{
		VkImageSubresourceRange subresourceRange{};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = m_MipLevelCount;
		subresourceRange.layerCount = 1;
		Utils::SetImageLayout(
			cmdBuf, m_Image, m_ImageLayout, newImageLayout, subresourceRange, srcStageMask, dstStageMask
		);

		m_ImageLayout = newImageLayout;
	}

	void VulkanTexture3D::GenerateMipMaps(VkCommandBuffer cmdBuffer, VkImageLayout newImageLayout)
	{
		int32_t mipWidth = m_ImageSpecification.Width;
		int32_t mipHeight = m_ImageSpecification.Height;

		TransitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			Utils::GetPipelineStageFlagsFromLayout(m_ImageLayout),
			Utils::GetPipelineStageFlagsFromLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		);

		for (uint32_t i = 1; i < m_MipLevelCount; i++)
		{
			VkImageSubresourceRange subresourceRange{};
			subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			subresourceRange.baseArrayLayer = 0;
			subresourceRange.layerCount = 1;
			subresourceRange.levelCount = 1;
			subresourceRange.baseMipLevel = i - 1;


			VkImageBlit blit{};

			// Src
			blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.srcOffsets[0] = { 0, 0, 0 };
			blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
			blit.srcSubresource.baseArrayLayer = 0;
			blit.srcSubresource.layerCount = 1;
			blit.srcSubresource.mipLevel = i - 1;

			// Dst
			blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.dstOffsets[0] = { 0, 0, 0 };
			blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
			blit.dstSubresource.baseArrayLayer = 0;
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

			if (mipWidth > 1) mipWidth /= 2;
			if (mipHeight > 1) mipHeight /= 2;
		}

		VkImageSubresourceRange subresourceRange{};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = m_MipLevelCount - 1;
		subresourceRange.levelCount = 1;

		subresourceRange.baseArrayLayer = 0;
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

		m_ImageLayout = newImageLayout;
	}

	void VulkanTexture3D::CalculateMipSizes()
	{
		m_MipLevelCount = Utils::CalculateMipMapLevels(m_ImageSpecification.Width, m_ImageSpecification.Height);

		uint32_t mipWidth = m_ImageSpecification.Width;
		uint32_t mipHeight = m_ImageSpecification.Height;
		uint32_t mipDepth = m_ImageSpecification.Depth;

		for (uint32_t mip = 0; mip < m_MipLevelCount; mip++)
		{
			if (mip != 0)
			{
				mipWidth /= 2;
				mipHeight /= 2;
			}
			glm::vec3 mipSize = { mipWidth, mipHeight, mipDepth };
			m_MipSizes[mip] = mipSize;
		}
	}

	void VulkanTexture3D::UpdateDescriptor()
	{
		m_DescriptorInfo[DescriptorImageType::Sampled].imageView = m_ImageView;
		m_DescriptorInfo[DescriptorImageType::Sampled].imageLayout = m_ImageLayout;
		m_DescriptorInfo[DescriptorImageType::Sampled].sampler = m_ImageSampler;

		m_DescriptorInfo[DescriptorImageType::Storage].imageView = m_ImageView;
		m_DescriptorInfo[DescriptorImageType::Storage].imageLayout = m_ImageLayout;
		m_DescriptorInfo[DescriptorImageType::Storage].sampler = nullptr;
	}
}