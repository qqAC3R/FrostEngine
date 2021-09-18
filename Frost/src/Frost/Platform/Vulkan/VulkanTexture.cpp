#include "frostpch.h"
#include "VulkanTexture.h"

#include "Frost/Platform/Vulkan/VulkanContext.h"
#include "Frost/Platform/Vulkan/VulkanShader.h"
#include "Frost/Platform/Vulkan/VulkanRenderer.h"

#include "stb_image.h"

namespace Frost
{

	namespace Utils
	{

		VkFormat TextureFormatToVk(TextureSpecs::FormatSpec format)
		{
			switch (format)
			{
				case TextureSpecs::FormatSpec::RGBA8:	  return VK_FORMAT_R8G8B8A8_SRGB;
				case TextureSpecs::FormatSpec::RGBA16F:	  return VK_FORMAT_R16G16B16A16_SFLOAT;
				case TextureSpecs::FormatSpec::DEPTH32:	  return VK_FORMAT_D32_SFLOAT;
				case TextureSpecs::FormatSpec::SWAPCHAIN: return VulkanContext::GetSwapChain()->GetImageFormat();
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

		VkImageLayout TextureUsageToLayoutVk(Vector<TextureSpecs::UsageSpec> usages)
		{
			for (auto usage : usages)
			{
				switch (usage)
				{
					case TextureSpecs::UsageSpec::ColorAttachment:
					case TextureSpecs::UsageSpec::Storage:				  return VK_IMAGE_LAYOUT_GENERAL;
					case TextureSpecs::UsageSpec::DepthStencilAttachment: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
				}

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

		static void CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels,
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
			imageInfo.arrayLayers = 1;
			imageInfo.format = format;
			imageInfo.tiling = tiling;
			imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageInfo.usage = usage;
			imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS)
			{
				FROST_ASSERT(0, "Failed to create image!");
			}

			VulkanAllocator::AllocateMemoryForImage(image, imageMemory);
		}

		static void ChangeImageLayout(VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels, VkImage& image)
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
			subresourceRange.levelCount = mipLevels;
			subresourceRange.baseArrayLayer = 0;
			subresourceRange.layerCount = 1;

			Utils::SetImageLayout(cmdBuf, image, oldLayout, newLayout, subresourceRange, PipelineStageForLayout(oldLayout), PipelineStageForLayout(newLayout));

			VulkanContext::GetCurrentDevice()->FlushCommandBuffer(cmdBuf);
		}

		static void CopyBufferToImage(const VkBuffer& buffer, VkImage& image, uint32_t width, uint32_t height)
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

		static void GenerateMipMaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels)
		{
			VkPhysicalDevice physicalDevice = VulkanContext::GetCurrentDevice()->GetPhysicalDevice();

			// Check if image format supports linear blitting
			VkFormatProperties formatProperties;
			vkGetPhysicalDeviceFormatProperties(physicalDevice, imageFormat, &formatProperties);

			if (!(formatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
				FROST_ASSERT(false, "Linear blitting not supported!");


			VkCommandBuffer cmdBuf = VulkanContext::GetCurrentDevice()->AllocateCommandBuffer(true);
			
			VkImageMemoryBarrier barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.image = image;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = 1;
			barrier.subresourceRange.levelCount = 1;

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

				barrier.subresourceRange.baseMipLevel = i - 1;
				barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

#if 0
				vkCmdPipelineBarrier(cmdBuf,
					VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
					0, nullptr,
					0, nullptr,
					1, &barrier);
#endif

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

				barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;


				Utils::InsertImageMemoryBarrier(cmdBuf, image,
					VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					subresourceRange
				);
#if 0
				vkCmdPipelineBarrier(cmdBuf,
					VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
					0, nullptr,
					0, nullptr,
					1, &barrier);
#endif

				if (mipWidth > 1) mipWidth /= 2;
				if (mipHeight > 1) mipHeight /= 2;

			}

			barrier.subresourceRange.baseMipLevel = mipLevels - 1;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			vkCmdPipelineBarrier(cmdBuf,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
				0, nullptr,
				0, nullptr,
				1, &barrier);

			VulkanContext::GetCurrentDevice()->FlushCommandBuffer(cmdBuf);

		}

		static void CreateImageView(VkImageView& imageView, VkImage& image, VkFormat format, uint32_t mipLevels)
		{
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

			VkImageViewUsageCreateInfo usageInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO };
			usageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

			VkImageViewCreateInfo createInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
			bool useImageless = format == VulkanContext::GetSwapChain()->GetImageFormat();
			createInfo.pNext = useImageless ? &usageInfo : nullptr;
			createInfo.image = image;
			createInfo.format = format;
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = mipLevels;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;

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

		static void CreateImageSampler(VkFilter filtering, uint32_t mipLevels, VkSampler& sampler)
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

			if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS)
			{
				FROST_ASSERT(0, "Failed to create texture sampler!");
			}
		}

	}


	//////////////////////////////////////////////////////////////////////////////////
	// TEXTURE 2D ////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////

	VulkanTexture2D::VulkanTexture2D(const std::string& path, const TextureSpecs& spec)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		// Loading the texture
		int width, height, channels;
		stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
		if (!pixels)
			FROST_ASSERT(0, "Failed to load image!");
		m_Width = width;
		m_Height = height;


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
		Utils::CreateImage(width, height, mipLevels, TextureFromat, VK_IMAGE_TYPE_2D,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_TextureImage, m_ImageMemory);

		// Changing the layout for copying the staging buffer to the iamge
		Utils::ChangeImageLayout(TextureFromat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels, m_TextureImage);
		Utils::CopyBufferToImage(stagingBuffer, m_TextureImage, static_cast<uint32_t>(width), static_cast<uint32_t>(height));


		// Generating mip maps if necessary and changing the layout to SHADER_READ_ONLY_OPTIMAL
		if (spec.UseMipMaps)
			Utils::GenerateMipMaps(m_TextureImage, TextureFromat, width, height, mipLevels);
		else
			Utils::ChangeImageLayout(TextureFromat, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipLevels, m_TextureImage);


		// Creating the ImageView
		Utils::CreateImageView(m_ImageView, m_TextureImage, TextureFromat, mipLevels);
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

	}

	void VulkanTexture2D::Destroy() const
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		vkDestroySampler(device, m_TextureSampler, nullptr);
		vkDestroyImageView(device, m_ImageView, nullptr);

		VulkanAllocator::DestroyImage(m_TextureImage, m_ImageMemory);
	}

	VulkanTexture2D::~VulkanTexture2D()
	{
	}
	

	//////////////////////////////////////////////////////////////////////////////////
	// TEXTURE STORAGE ///////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////

	VulkanImage2D::VulkanImage2D(const TextureSpecs& spec)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		VkFormat textureFormat = Utils::TextureFormatToVk(spec.Format);
		VkImageUsageFlags usageFlags = Utils::TextureUsageToVk(spec.Usage);

		Utils::CreateImage(spec.Width, spec.Height, 1,
								 textureFormat, VK_IMAGE_TYPE_2D,
								 VK_IMAGE_TILING_OPTIMAL,
								 VK_IMAGE_USAGE_SAMPLED_BIT | usageFlags,
								 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
								 m_TextureImage, m_ImageMemory);

		VkImageLayout textureLayout = Utils::TextureUsageToLayoutVk(spec.Usage);
		Utils::ChangeImageLayout(textureFormat, VK_IMAGE_LAYOUT_UNDEFINED, textureLayout, 1, m_TextureImage);

		Utils::CreateImageView(m_ImageView, m_TextureImage, textureFormat, 1);
		m_Layout = textureLayout;

		// Creating the sampler
		VkFilter filtering = Utils::TextureFilteringToVk(spec.Filtering);
		Utils::CreateImageSampler(filtering, 1, m_TextureSampler);

		m_Width = spec.Width;
		m_Height = spec.Height;

		VulkanContext::SetStructDebugName("Image2D-Image", VK_OBJECT_TYPE_IMAGE, m_TextureImage);
		VulkanContext::SetStructDebugName("Image2D-ImageView", VK_OBJECT_TYPE_IMAGE_VIEW, m_ImageView);
		VulkanContext::SetStructDebugName("Image2D-Sampler", VK_OBJECT_TYPE_SAMPLER, m_TextureSampler);

	}

	VulkanImage2D::VulkanImage2D(void* image, const TextureSpecs& spec)
		: m_TextureImage((VkImage)image)
	{
		m_Width = spec.Width;
		m_Height = spec.Height;
		VkFormat format = Utils::TextureFormatToVk(spec.Format);

		Utils::CreateImageView(m_ImageView, m_TextureImage, format, 1);

		m_UsingSwapChain = true;
	}

	void VulkanImage2D::Destroy() const
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		vkDestroyImageView(device, m_ImageView, nullptr);

		if (!m_UsingSwapChain)
		{
			vkDestroySampler(device, m_TextureSampler, nullptr);
			VulkanAllocator::DestroyImage(m_TextureImage, m_ImageMemory);
		}
	}

	VulkanImage2D::~VulkanImage2D()
	{
	}

}