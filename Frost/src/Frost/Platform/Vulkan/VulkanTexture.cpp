#include "frostpch.h"
#include "VulkanTexture.h"

#include "Frost/Platform/Vulkan/VulkanContext.h"
#include "Frost/Platform/Vulkan/VulkanShader.h"
#include "Frost/Platform/Vulkan/VulkanRenderer.h"

#include "stb_image.h"

namespace Frost
{

	//////////////////////////////////////////////////////////////////////////////////
	// Vulkan Texture2D
	//////////////////////////////////////////////////////////////////////////////////

	VulkanTexture2D::VulkanTexture2D(const std::string& path, const TextureSpecs& spec)
		: m_TextureSpecification(spec)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		// Loading the texture
		int width, height, channels;
		stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
		if (!pixels)
			FROST_ASSERT(0, "Failed to load image!");
		m_TextureSpecification.Width = width;
		m_TextureSpecification.Height = height;


		// Calculating the size of the buffer and the number of mip levels needed
		VkDeviceSize imageSize = width * height * 4;
		int mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
		
		// Making a staging buffer to copy the data
		VkBuffer stagingBuffer;
		VulkanMemoryInfo stagingBufferMemory;
		VulkanAllocator::AllocateBuffer(imageSize, { BufferType::TransferSrc }, MemoryUsage::CPU_AND_GPU, stagingBuffer, stagingBufferMemory);

		// Copying the data
		void* data;
		VulkanAllocator::BindBuffer(stagingBuffer, stagingBufferMemory, &data);
		memcpy(data, pixels, static_cast<size_t>(imageSize));
		VulkanAllocator::UnbindBuffer(stagingBufferMemory);
		

		// Checks if the user inputed to use mip maps or not
		if (!spec.UseMipMaps) mipLevels = 1;


		VkFormat TextureFromat = Utils::TextureFormatToVk(spec.Format);

		// Creation of the image
		Utils::CreateImage(width, height, 1, mipLevels, TextureFromat, VK_IMAGE_TYPE_2D,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			m_TextureImage, m_ImageMemory);

		// Changing the layout for copying the staging buffer to the iamge
 		Utils::ChangeImageLayout(m_TextureImage, TextureFromat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels, 1);
		Utils::CopyBufferToImage(stagingBuffer, m_TextureImage, static_cast<uint32_t>(width), static_cast<uint32_t>(height));


		// Generating mip maps if necessary and changing the layout to SHADER_READ_ONLY_OPTIMAL
		if (spec.UseMipMaps)
			Utils::GenerateMipMaps(m_TextureImage, TextureFromat, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, width, height, mipLevels);
		else
			Utils::ChangeImageLayout(m_TextureImage, TextureFromat, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipLevels, 1);


		// Creating the ImageView
		Utils::CreateImageView(m_ImageView, m_TextureImage, VK_IMAGE_USAGE_SAMPLED_BIT, TextureFromat, mipLevels, 1);
		m_Layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		// Creating the sampler
		VkFilter filtering = Utils::TextureFilteringToVk(spec.Filtering);
		Utils::CreateImageSampler(filtering, mipLevels, m_TextureSampler);


		// Clean up the staging buffer and the stbi_image
		VulkanAllocator::DeleteBuffer(stagingBuffer, stagingBufferMemory);

		stbi_image_free(pixels);


		VulkanContext::SetStructDebugName("Texture2D-Image", VK_OBJECT_TYPE_IMAGE, m_TextureImage);
		VulkanContext::SetStructDebugName("Texture2D-ImageView", VK_OBJECT_TYPE_IMAGE_VIEW, m_ImageView);
		VulkanContext::SetStructDebugName("Texture2D-Sampler", VK_OBJECT_TYPE_SAMPLER, m_TextureSampler);

		UpdateDescriptor();
	}

	void VulkanTexture2D::Destroy() const
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		vkDestroySampler(device, m_TextureSampler, nullptr);
		vkDestroyImageView(device, m_ImageView, nullptr);

		VulkanAllocator::DestroyImage(m_TextureImage, m_ImageMemory);
	}

	void VulkanTexture2D::UpdateDescriptor()
	{
		m_DescriptorInfo.imageLayout = m_Layout;
		m_DescriptorInfo.imageView = m_ImageView;
		m_DescriptorInfo.sampler = m_TextureSampler;
	}

	VulkanTexture2D::~VulkanTexture2D()
	{
	}
	


	//////////////////////////////////////////////////////////////////////////////////
	// Vulkan Image2D
	//////////////////////////////////////////////////////////////////////////////////

	VulkanImage2D::VulkanImage2D(const TextureSpecs& spec)
		: m_TextureSpecification(spec)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		VkFormat textureFormat = Utils::TextureFormatToVk(spec.Format);
		VkImageUsageFlags usageFlags = Utils::TextureUsageToVk(spec.Usage);

		Utils::CreateImage(spec.Width, spec.Height, 1, 1,
								 textureFormat, VK_IMAGE_TYPE_2D,
								 VK_IMAGE_TILING_OPTIMAL,
								 VK_IMAGE_USAGE_SAMPLED_BIT | usageFlags,
								 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
								 m_TextureImage, m_ImageMemory);

		VkImageLayout textureLayout = Utils::TextureUsageToLayoutVk(spec.Usage[0]);
		m_Layout = textureLayout;
		Utils::ChangeImageLayout(m_TextureImage, textureFormat, VK_IMAGE_LAYOUT_UNDEFINED, textureLayout, 1, 1);

		Utils::CreateImageView(m_ImageView, m_TextureImage, usageFlags, textureFormat, 1, 1);

		// Creating the sampler
		VkFilter filtering = Utils::TextureFilteringToVk(spec.Filtering);
		Utils::CreateImageSampler(filtering, 1, m_TextureSampler);

		VulkanContext::SetStructDebugName("Image2D-Image", VK_OBJECT_TYPE_IMAGE, m_TextureImage);
		VulkanContext::SetStructDebugName("Image2D-ImageView", VK_OBJECT_TYPE_IMAGE_VIEW, m_ImageView);
		VulkanContext::SetStructDebugName("Image2D-Sampler", VK_OBJECT_TYPE_SAMPLER, m_TextureSampler);

		UpdateDescriptor();
	}

	void VulkanImage2D::TransitionLayout(VkCommandBuffer cmdBuf, VkImageLayout newLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask)
	{
		Utils::SetImageLayout(cmdBuf, m_TextureImage,
			VK_IMAGE_ASPECT_COLOR_BIT,
			m_Layout, newLayout
		);
		m_Layout = newLayout;
	}

	void VulkanImage2D::UpdateDescriptor()
	{
		m_DescriptorInfo.imageLayout = m_Layout;
		m_DescriptorInfo.imageView = m_ImageView;
		m_DescriptorInfo.sampler = m_TextureSampler;
	}

	VulkanImage2D::~VulkanImage2D()
	{
	}

	void VulkanImage2D::Destroy() const
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		vkDestroyImageView(device, m_ImageView, nullptr);
		vkDestroySampler(device, m_TextureSampler, nullptr);
		VulkanAllocator::DestroyImage(m_TextureImage, m_ImageMemory);
	}

	///////////////////////////////////////////////
	// Vulkan CubeMap
	///////////////////////////////////////////////

	VulkanTextureCubeMap::VulkanTextureCubeMap(const TextureSpecs& spec)
		: m_TextureSpecification(spec)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		VkFormat textureFormat = Utils::TextureFormatToVk(spec.Format);
		VkImageUsageFlags usageFlags = Utils::TextureUsageToVk(spec.Usage);
		Utils::CreateImage(spec.Width, spec.Height, 6, 1,
			textureFormat, VK_IMAGE_TYPE_2D,
							VK_IMAGE_TILING_OPTIMAL,
							VK_IMAGE_USAGE_SAMPLED_BIT | usageFlags,
							VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
							m_TextureImage, m_ImageMemory);


		VkImageLayout textureLayout = Utils::TextureUsageToLayoutVk(spec.Usage[0]);
		m_Layout = textureLayout;

		// Generating mip maps if necessary and changing the layout to SHADER_READ_ONLY_OPTIMAL
		if (spec.UseMipMaps)
		{
			m_MipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(spec.Width, spec.Height)))) + 1;
			Utils::ChangeImageLayout(m_TextureImage, textureFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, 1);
			Utils::GenerateMipMaps(m_TextureImage, textureFormat, textureLayout, spec.Width, spec.Width, m_MipLevels);
		}
		else
		{
			m_MipLevels = 1;
			Utils::ChangeImageLayout(m_TextureImage, textureFormat, VK_IMAGE_LAYOUT_UNDEFINED, textureLayout, 1, 6);
		}

		Utils::CreateImageView(m_ImageView, m_TextureImage, usageFlags, textureFormat, 1, 6);

		// Creating the sampler
		VkFilter filtering = Utils::TextureFilteringToVk(spec.Filtering);
		Utils::CreateImageSampler(filtering, m_MipLevels, m_TextureSampler);

		VulkanContext::SetStructDebugName("TextureCubeMap-Image", VK_OBJECT_TYPE_IMAGE, m_TextureImage);
		VulkanContext::SetStructDebugName("TextureCubeMap-ImageView", VK_OBJECT_TYPE_IMAGE_VIEW, m_ImageView);
		VulkanContext::SetStructDebugName("TextureCubeMap-Sampler", VK_OBJECT_TYPE_SAMPLER, m_TextureSampler);

		UpdateDescriptor();
	}

	VulkanTextureCubeMap::~VulkanTextureCubeMap()
	{

	}

	void VulkanTextureCubeMap::Destroy() const
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		vkDestroyImageView(device, m_ImageView, nullptr);
		vkDestroySampler(device, m_TextureSampler, nullptr);
		VulkanAllocator::DestroyImage(m_TextureImage, m_ImageMemory);
	}

	void VulkanTextureCubeMap::TransitionLayout(VkCommandBuffer cmdBuf, VkImageLayout newLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask)
	{
		Utils::SetImageLayout(cmdBuf, m_TextureImage,
			VK_IMAGE_ASPECT_COLOR_BIT,
			m_Layout, newLayout
		);

		VkImageSubresourceRange subresourceRange{};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = m_MipLevels;
		subresourceRange.layerCount = 6;
		Utils::SetImageLayout(cmdBuf, m_TextureImage, m_Layout, newLayout, subresourceRange, srcStageMask, dstStageMask);

		m_Layout = newLayout;
	}

	void VulkanTextureCubeMap::UpdateDescriptor()
	{
		m_DescriptorInfo.imageLayout = m_Layout;
		m_DescriptorInfo.imageView = m_ImageView;
		m_DescriptorInfo.sampler = m_TextureSampler;
	}

	///////////////////////////////////////////////
	// UTILS
	///////////////////////////////////////////////

	namespace Utils
	{

		VkFormat TextureFormatToVk(TextureSpecs::FormatSpec format)
		{
			switch (format)
			{
			case TextureSpecs::FormatSpec::RGBA8:	  return VK_FORMAT_R8G8B8A8_SRGB;
			case TextureSpecs::FormatSpec::RGBA16F:	  return VK_FORMAT_R16G16B16A16_SFLOAT;
			case TextureSpecs::FormatSpec::DEPTH32:	  return VK_FORMAT_D32_SFLOAT;
			}

			FROST_ASSERT(false, "");
			return VkFormat();
		}

		VkFilter TextureFilteringToVk(TextureSpecs::FilteringSpec filtering)
		{
			switch (filtering)
			{
			case TextureSpecs::FilteringSpec::NEAREST: return VK_FILTER_NEAREST;
			case TextureSpecs::FilteringSpec::LINEAR:  return VK_FILTER_LINEAR;
			}

			FROST_ASSERT(false, "");
			return VkFilter();
		}

		VkImageUsageFlags TextureUsageToVk(Vector<TextureSpecs::UsageSpec> usages)
		{
			VkImageUsageFlags usageFlags{};

			for (auto usage : usages)
			{
				VkImageUsageFlagBits flagBit{};

				switch (usage)
				{
				case TextureSpecs::UsageSpec::ColorAttachment:		  flagBit = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; break;
				case TextureSpecs::UsageSpec::DepthStencilAttachment: flagBit = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT; break;
				case TextureSpecs::UsageSpec::Storage:				  flagBit = VK_IMAGE_USAGE_STORAGE_BIT; break;
				default:											  flagBit = VK_IMAGE_USAGE_SAMPLED_BIT; break;
				}

				usageFlags |= flagBit;
			}

			return usageFlags;
		}

		VkImageLayout TextureUsageToLayoutVk(TextureSpecs::UsageSpec usage)
		{
			switch (usage)
			{
				case TextureSpecs::UsageSpec::ColorAttachment:        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				case TextureSpecs::UsageSpec::Storage:				  return VK_IMAGE_LAYOUT_GENERAL;
				case TextureSpecs::UsageSpec::DepthStencilAttachment: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			}
			return VK_IMAGE_LAYOUT_UNDEFINED;
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

		void CreateImage(uint32_t width, uint32_t height, uint32_t texureDepth, uint32_t mipLevels,
						VkFormat format, VkImageType type, VkImageTiling tiling,
						VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
						VkImage& image, VulkanMemoryInfo& imageMemory)
		{

			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

			VkImageCreateInfo imageInfo{};
			imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			imageInfo.imageType = type;
			imageInfo.extent.width = width;
			imageInfo.extent.height = height;
			imageInfo.extent.depth = 1;
			imageInfo.mipLevels = mipLevels;
			imageInfo.arrayLayers = texureDepth;
			imageInfo.format = format;
			imageInfo.tiling = tiling;
			imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageInfo.usage = usage;
			imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			imageInfo.flags = texureDepth == 6 ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0; // If the image has 6 faces, then it is a cubemap

			FROST_VKCHECK(vkCreateImage(device, &imageInfo, nullptr, &image));
			VulkanAllocator::AllocateMemoryForImage(image, imageMemory);
		}

		void ChangeImageLayout(VkImage& image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels, uint32_t depthMap)
		{
			VkCommandBuffer cmdBuf = VulkanContext::GetCurrentDevice()->AllocateCommandBuffer(true);

			VkImageAspectFlags aspectMask;
			if (format == VK_FORMAT_D32_SFLOAT)
				aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			else
				aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

			VkImageSubresourceRange subresourceRange{};
			subresourceRange.aspectMask = aspectMask;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.baseArrayLayer = 0;
			subresourceRange.levelCount = mipLevels;
			subresourceRange.layerCount = depthMap;

			Utils::SetImageLayout(cmdBuf, image, oldLayout, newLayout, subresourceRange, PipelineStageForLayout(oldLayout), PipelineStageForLayout(newLayout));

			VulkanContext::GetCurrentDevice()->FlushCommandBuffer(cmdBuf);
		}

		void CopyBufferToImage(const VkBuffer& buffer, VkImage& image, uint32_t width, uint32_t height)
		{
			VkCommandBuffer cmdBuf = VulkanContext::GetCurrentDevice()->AllocateCommandBuffer(true);

			VkBufferImageCopy region{};
			region.bufferOffset = 0;
			region.bufferRowLength = 0;
			region.bufferImageHeight = 0;

			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.mipLevel = 0;
			region.imageSubresource.baseArrayLayer = 0;
			region.imageSubresource.layerCount = 1;

			region.imageOffset = { 0, 0, 0 };
			region.imageExtent = {
				width,
				height,
				1
			};

			vkCmdCopyBufferToImage(
				cmdBuf,
				buffer,
				image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1,
				&region
			);

			VulkanContext::GetCurrentDevice()->FlushCommandBuffer(cmdBuf);
		}

		void GenerateMipMaps(VkImage image, VkFormat imageFormat, VkImageLayout newLayout, int32_t texWidth, int32_t texHeight, uint32_t mipLevels)
		{
			VkPhysicalDevice physicalDevice = VulkanContext::GetCurrentDevice()->GetPhysicalDevice();

			// Check if image format supports linear blitting
			VkFormatProperties formatProperties;
			vkGetPhysicalDeviceFormatProperties(physicalDevice, imageFormat, &formatProperties);

			if (!(formatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
				FROST_ASSERT(false, "Linear blitting not supported!");


			VkCommandBuffer cmdBuf = VulkanContext::GetCurrentDevice()->AllocateCommandBuffer(true);

			//VkImageMemoryBarrier barrier{};
			//barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			//barrier.image = image;
			//barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			//barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			//barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			//barrier.subresourceRange.baseArrayLayer = 0;
			//barrier.subresourceRange.layerCount = 1;
			//barrier.subresourceRange.levelCount = 1;

			VkImageSubresourceRange subresourceRange{};
			subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			subresourceRange.baseArrayLayer = 0;
			subresourceRange.layerCount = 1;
			subresourceRange.levelCount = 1;



			int32_t mipWidth = texWidth;
			int32_t mipHeight = texHeight;

			for (uint32_t i = 1; i < mipLevels; i++)
			{
				subresourceRange.baseMipLevel = i - 1;

				//barrier.subresourceRange.baseMipLevel = i - 1;
				//barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				//barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				//barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				//barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

				Utils::InsertImageMemoryBarrier(cmdBuf, image,
					VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
					subresourceRange
				);

				VkImageBlit blit{};
				blit.srcOffsets[0] = { 0, 0, 0 };
				blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
				blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				blit.srcSubresource.mipLevel = i - 1;
				blit.srcSubresource.baseArrayLayer = 0;
				blit.srcSubresource.layerCount = 1;
				blit.dstOffsets[0] = { 0, 0, 0 };
				blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
				blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				blit.dstSubresource.mipLevel = i;
				blit.dstSubresource.baseArrayLayer = 0;
				blit.dstSubresource.layerCount = 1;

				vkCmdBlitImage(cmdBuf,
					image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					1, &blit,
					VK_FILTER_LINEAR
				);

				//barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				//barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				//barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				//barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;


				Utils::InsertImageMemoryBarrier(cmdBuf, image,
					VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, newLayout,
					VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					subresourceRange
				);

				if (mipWidth > 1) mipWidth /= 2;
				if (mipHeight > 1) mipHeight /= 2;

			}

			subresourceRange.baseMipLevel = mipLevels - 1;
			//barrier.subresourceRange.baseMipLevel = mipLevels - 1;
			//barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			//barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			//barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			//barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			Utils::InsertImageMemoryBarrier(cmdBuf, image,
				VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, newLayout,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				subresourceRange
			);

			VulkanContext::GetCurrentDevice()->FlushCommandBuffer(cmdBuf);

		}

		void CreateImageView(VkImageView& imageView, VkImage& image, VkImageUsageFlags imageUsage, VkFormat format, uint32_t mipLevels, uint32_t textureDepth)
		{
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

			VkImageViewCreateInfo createInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
			createInfo.image = image;
			createInfo.format = format;
			createInfo.viewType = textureDepth == 6 ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;

			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = mipLevels;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = textureDepth;
			switch (format)
			{
			case VK_FORMAT_D32_SFLOAT:
			case VK_FORMAT_D24_UNORM_S8_UINT:
				createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT; break;
			default:
				createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			}

			FROST_VKCHECK(vkCreateImageView(device, &createInfo, nullptr, &imageView));
		}

		void CreateImageSampler(VkFilter filtering, uint32_t mipLevels, VkSampler& sampler)
		{
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
			VkPhysicalDevice physicalDevice = VulkanContext::GetCurrentDevice()->GetPhysicalDevice();

			VkSamplerCreateInfo samplerInfo{};
			samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			samplerInfo.magFilter = filtering;
			samplerInfo.minFilter = filtering;
			samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerInfo.anisotropyEnable = VK_FALSE;
			samplerInfo.maxAnisotropy = 1.0f;

			VkPhysicalDeviceProperties properties{};
			vkGetPhysicalDeviceProperties(physicalDevice, &properties);

			samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
			samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
			samplerInfo.unnormalizedCoordinates = VK_FALSE;
			samplerInfo.compareEnable = VK_FALSE;
			samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

			samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			samplerInfo.mipLodBias = 0.0f;
			samplerInfo.minLod = 0.0f;
			samplerInfo.maxLod = static_cast<float>(mipLevels);

			FROST_VKCHECK(vkCreateSampler(device, &samplerInfo, nullptr, &sampler));
		}
	}
}