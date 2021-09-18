#include "frostpch.h"
#include "VulkanImage.h"

#include "VulkanContext.h"
#include "Frost/Platform/Vulkan/VulkanCommandBuffer.h"

namespace Frost
{
	namespace Utils
	{
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

			FROST_VKCHECK(vkCreateImage(device, &imageInfo, nullptr, &image), "Failed to create image!");

			VkMemoryRequirements memRequirements;
			vkGetImageMemoryRequirements(device, image, &memRequirements);

			VkMemoryAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.allocationSize = memRequirements.size;
			allocInfo.memoryTypeIndex = VulkanBufferAllocator::FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);


			VulkanBufferAllocator::CreateMemoryForImage(image, imageMemory);
		}

		static void ChangeImageLayout(VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels, VkImage& image)
		{
			VulkanCommandPool cmdPool;
			auto cmdBuf = VulkanCommandBuffer::BeginSingleTimeCommands(cmdPool);

			VkImageMemoryBarrier barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.oldLayout = oldLayout;
			barrier.newLayout = newLayout;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image = image;
			barrier.subresourceRange.baseMipLevel = 0;
			barrier.subresourceRange.levelCount = mipLevels;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = 1;
			barrier.subresourceRange.aspectMask = format == VK_FORMAT_D32_SFLOAT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.srcAccessMask = AccessFlagsToImageLayout(oldLayout);
			barrier.dstAccessMask = AccessFlagsToImageLayout(newLayout);

			VkPipelineStageFlags sourceStage = PipelineStageForLayout(oldLayout);
			VkPipelineStageFlags destinationStage = PipelineStageForLayout(newLayout);
			vkCmdPipelineBarrier(
				cmdBuf.GetCommandBuffer(),
				sourceStage, destinationStage,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier
			);

			VulkanCommandBuffer::EndSingleTimeCommands(cmdBuf);

			cmdPool.Destroy();
		}

		static void CreateImageView(VkImageView& imageView, VkImage& image, VkFormat format, uint32_t mipLevels)
		{
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

			VkImageViewUsageCreateInfo usageInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO };
			usageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

			VkImageViewCreateInfo createInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
			createInfo.pNext = nullptr;
			createInfo.image = image;
			createInfo.format = format;
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = mipLevels;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;
			createInfo.subresourceRange.aspectMask = format == VK_FORMAT_D32_SFLOAT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

			FROST_VKCHECK(vkCreateImageView(device, &createInfo, nullptr, &imageView), "Failed to create texture image view!");
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

		static void CopyBufferToImage(const VkBuffer& buffer, VkImage& image, uint32_t width, uint32_t height)
		{
			VulkanCommandPool cmdPool;
			auto cmdBuf = VulkanCommandBuffer::BeginSingleTimeCommands(cmdPool);

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
				cmdBuf.GetCommandBuffer(),
				buffer,
				image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1,
				&region
			);

			VulkanCommandBuffer::EndSingleTimeCommands(cmdBuf);

			cmdPool.Destroy();
		}

		static void GenerateMipMaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels)
		{
			VkPhysicalDevice physicalDevice = VulkanContext::GetCurrentDevice()->GetPhysicalDevice();

			// Check if image format supports linear blitting
			VkFormatProperties formatProperties;
			vkGetPhysicalDeviceFormatProperties(physicalDevice, imageFormat, &formatProperties);

			if (!(formatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
				FROST_ASSERT(false, "Linear blitting not supported!");



			VulkanCommandPool cmdPool;
			auto cmdBuf = VulkanCommandBuffer::BeginSingleTimeCommands(cmdPool);
			auto vkCmdBuf = cmdBuf.GetCommandBuffer();

			VkImageMemoryBarrier barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.image = image;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = 1;
			barrier.subresourceRange.levelCount = 1;



			int32_t mipWidth = texWidth;
			int32_t mipHeight = texHeight;

			for (uint32_t i = 1; i < mipLevels; i++)
			{
				barrier.subresourceRange.baseMipLevel = i - 1;
				barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

				vkCmdPipelineBarrier(vkCmdBuf,
					VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
					0, nullptr,
					0, nullptr,
					1, &barrier);

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

				vkCmdBlitImage(vkCmdBuf,
					image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					1, &blit,
					VK_FILTER_LINEAR);

				barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

				vkCmdPipelineBarrier(vkCmdBuf,
					VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
					0, nullptr,
					0, nullptr,
					1, &barrier);

				if (mipWidth > 1) mipWidth /= 2;
				if (mipHeight > 1) mipHeight /= 2;

			}

			barrier.subresourceRange.baseMipLevel = mipLevels - 1;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			vkCmdPipelineBarrier(vkCmdBuf,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
				0, nullptr,
				0, nullptr,
				1, &barrier);

			VulkanCommandBuffer::EndSingleTimeCommands(cmdBuf);

			cmdPool.Destroy();
		}

	}

	VulkanImage2DD::VulkanImage2DD(ImageSpecification specification)
	{
		// Getting the texture format
		VkFormat textureFormat = Utils::GetVulkanImageFormat(m_ImageSpecification.Format);

		// Getting the usage flags
		VkImageUsageFlags imageUsageFlags{};
		if (m_ImageSpecification.Usage == ImageUsage::Texture)
			imageUsageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
		else if (m_ImageSpecification.Usage == ImageUsage::Storage)
			imageUsageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
		else if (m_ImageSpecification.Usage == ImageUsage::Attachment)
			imageUsageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		// Creating the image
		Utils::CreateImage(m_ImageSpecification.Width, m_ImageSpecification.Height, m_ImageSpecification.Mips,
			textureFormat, VK_IMAGE_TYPE_2D,
			VK_IMAGE_TILING_OPTIMAL,
			imageUsageFlags,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			m_Image, m_ImageMemory);

		// Chaning its layout from UNDEFINED to the one specified
		VkImageLayout imageLayout = Utils::GetVulkanImageLayout(m_ImageSpecification.Usage);
		Utils::ChangeImageLayout(textureFormat, VK_IMAGE_LAYOUT_UNDEFINED, imageLayout, 1, m_Image);

		// Creating the imageview
		Utils::CreateImageView(m_ImageView, m_Image, textureFormat, m_ImageSpecification.Mips);


		// Creating the sampler
		VkFilter filtering = Utils::GetVulkanSamplerFiltering(m_ImageSpecification.Sampler.SamplerFilter);
		Utils::CreateImageSampler(filtering, 1, m_Sampler);

		VulkanContext::SetStructDebugName("Image2D-Image", VK_OBJECT_TYPE_IMAGE, m_Image);
		VulkanContext::SetStructDebugName("Image2D-ImageView", VK_OBJECT_TYPE_IMAGE_VIEW, m_ImageView);
		VulkanContext::SetStructDebugName("Image2D-Sampler", VK_OBJECT_TYPE_SAMPLER, m_Sampler);

	}

	VulkanImage2DD::VulkanImage2DD(ImageSpecification specification, const void* data)
		: m_ImageSpecification(specification)
	{
		// Calculating the size of the buffer and the number of mip levels needed
		VkDeviceSize imageSize = m_ImageSpecification.Width * m_ImageSpecification.Height * 4;
		int mipLevels = specification.Mips;
		//int mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(m_ImageSpecification.Width, m_ImageSpecification.Height)))) + 1;


		// Making a staging buffer to copy the data
		VkBuffer stagingBuffer;
		VulkanMemoryInfo stagingBufferMemory;
		VulkanBufferAllocator::CreateBuffer(imageSize,
			{ BufferType::TransferSrc },
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer, stagingBufferMemory);

		// Copying the data
		void* temporaryData;
		VulkanBufferAllocator::BindBuffer(stagingBuffer, stagingBufferMemory, &temporaryData);
		memcpy(temporaryData, data, static_cast<size_t>(imageSize));
		VulkanBufferAllocator::UnbindBuffer(stagingBufferMemory);





		// Creation of the image
		VkFormat TextureFromat = Utils::GetVulkanImageFormat(m_ImageSpecification.Format);
		Utils::CreateImage(m_ImageSpecification.Width, m_ImageSpecification.Height, mipLevels, TextureFromat, VK_IMAGE_TYPE_2D,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			m_Image, m_ImageMemory);


		// Changing the layout for copying the staging buffer to the iamge
		Utils::ChangeImageLayout(TextureFromat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels, m_Image);
		Utils::CopyBufferToImage(stagingBuffer, m_Image, static_cast<uint32_t>(m_ImageSpecification.Width), static_cast<uint32_t>(m_ImageSpecification.Height));

		// Generating mip maps if necessary and changing the layout to SHADER_READ_ONLY_OPTIMAL
		Utils::GenerateMipMaps(m_Image, TextureFromat, m_ImageSpecification.Width, m_ImageSpecification.Width, mipLevels);

		// Creating the ImageView
		Utils::CreateImageView(m_ImageView, m_Image, TextureFromat, mipLevels);
		m_ImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		// Creating the sampler
		VkFilter filtering = Utils::GetVulkanSamplerFiltering(m_ImageSpecification.Sampler.SamplerFilter);
		Utils::CreateImageSampler(filtering, mipLevels, m_Sampler);


		// Clean up the staging buffer and the stbi_image
		VulkanBufferAllocator::DeleteBuffer(stagingBuffer, stagingBufferMemory);


		VulkanContext::SetStructDebugName("Image2D-Image", VK_OBJECT_TYPE_IMAGE, m_Image);
		VulkanContext::SetStructDebugName("Image2D-ImageView", VK_OBJECT_TYPE_IMAGE_VIEW, m_ImageView);
		VulkanContext::SetStructDebugName("Image2D-Sampler", VK_OBJECT_TYPE_SAMPLER, m_Sampler);

	}

	VulkanImage2DD::~VulkanImage2DD()
	{

	}

	void VulkanImage2DD::Invalidate()
	{
		
	}

	void VulkanImage2DD::Destroy()
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		vkDestroyImageView(device, m_ImageView, nullptr);
		vkDestroySampler(device, m_Sampler, nullptr);
		VulkanBufferAllocator::DestroyImage(m_Image, m_ImageMemory);
	}
}