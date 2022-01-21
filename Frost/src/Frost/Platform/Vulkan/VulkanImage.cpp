#include "frostpch.h"
#include "VulkanImage.h"

#include "VulkanContext.h"
#include "Frost/Platform/Vulkan/VulkanRenderer.h"

namespace Frost
{

	VulkanImage2D::VulkanImage2D(const ImageSpecification& specification)
		: m_ImageSpecification(specification), m_ImageLayout(VK_IMAGE_LAYOUT_UNDEFINED)
	{
		VkFormat textureFormat = Utils::GetImageFormat(specification.Format);
		VkImageUsageFlags usageFlags = Utils::GetImageUsageFlags(specification.Usage);
		VkImageLayout newImageLayout = Utils::GetImageLayout(specification.Usage);

		// Calculate the mip chain levels
		if (specification.UseMipChain)
			m_MipLevelCount = Utils::CalculateMipMapLevels(specification.Width, specification.Height);
		else
			m_MipLevelCount = 1;

		// Creating the image and allocating the needed buffer
		// (for creation we use `VK_IMAGE_LAYOUT_UNDEFINED` layout, which will be later changed to the user's input")
		Utils::CreateImage(specification.Width, specification.Height, 1, m_MipLevelCount,
			VK_IMAGE_TYPE_2D, textureFormat,
			VK_IMAGE_TILING_OPTIMAL,
			usageFlags | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // Need the trasnfer src/dst bits for generating the mips
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, // GPU only
			m_Image, m_ImageMemory);

		// Recording a temporary commandbuffer for transitioning
		VkCommandBuffer cmdBuf = VulkanContext::GetCurrentDevice()->AllocateCommandBuffer(RenderQueueType::Graphics ,true);

		// Generating mip maps if the mip count is higher than 1, else just transition to user's input `VkImageLayout`
		TransitionLayout(cmdBuf, newImageLayout, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, Utils::GetPipelineStageFlagsFromLayout(newImageLayout));
#if 0
		if (specification.Mips > 1)
		{
			TransitionLayout(cmdBuf, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
							 Utils::GetPipelineStageFlagsFromLayout(m_ImageLayout),
							 Utils::GetPipelineStageFlagsFromLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
			);

			GenerateMipMaps(cmdBuf, newImageLayout);
		}
		else
		{
			TransitionLayout(cmdBuf, newImageLayout, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, Utils::GetPipelineStageFlagsFromLayout(newImageLayout));
		}
#endif

		// Ending the temporary commandbuffer for transitioning
		VulkanContext::GetCurrentDevice()->FlushCommandBuffer(cmdBuf);


		// Creating the image view
		Utils::CreateImageView(m_ImageView, m_Image, usageFlags, textureFormat, m_MipLevelCount, 1);

		// Creating the imageView mips
		for (uint32_t i = 0; i < m_MipLevelCount; i++)
		{
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

			VkImageViewCreateInfo createInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
			createInfo.image = m_Image;
			createInfo.format = textureFormat;
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

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

		// Creating the sampler
		VkFilter filtering = Utils::GetImageFiltering(specification.Sampler.SamplerFilter);
		VkSamplerAddressMode addressMode = Utils::GetImageWrap(specification.Sampler.SamplerWrap);
		VkSamplerMipmapMode mipMapMode = Utils::GetImageSamplerMipMapMode(specification.Sampler.SamplerFilter);
		VkSamplerReductionMode reductionMode = Utils::GetSamplerReductionMode(specification.Sampler.ReductionMode_Optional);
		Utils::CreateImageSampler(m_ImageSampler, filtering, addressMode, mipMapMode, m_MipLevelCount, reductionMode);

		// Setting the debug names for the structs
		VulkanContext::SetStructDebugName("Image2D-Image", VK_OBJECT_TYPE_IMAGE, m_Image);
		VulkanContext::SetStructDebugName("Image2D-ImageView", VK_OBJECT_TYPE_IMAGE_VIEW, m_ImageView);
		VulkanContext::SetStructDebugName("Image2D-Sampler", VK_OBJECT_TYPE_SAMPLER, m_ImageSampler);

		// Updating the descriptor
		UpdateDescriptor();
	}

	VulkanImage2D::VulkanImage2D(const ImageSpecification& specification, const void* data)
		: m_ImageSpecification(specification), m_ImageLayout(VK_IMAGE_LAYOUT_UNDEFINED)
	{
		VkFormat textureFormat = Utils::GetImageFormat(specification.Format);
		VkImageUsageFlags usageFlags = Utils::GetImageUsageFlags(specification.Usage);
		VkImageLayout newImageLayout = Utils::GetImageLayout(specification.Usage);

		uint32_t imageSize = Utils::CalculateImageBufferSize(specification.Width, specification.Height, specification.Format);

		// Calculate the mip chain levels
		if (specification.UseMipChain)
			m_MipLevelCount = Utils::CalculateMipMapLevels(specification.Width, specification.Height);
		else
			m_MipLevelCount = 1;

		// Making a staging buffer to copy the data
		VkBuffer stagingBuffer;
		VulkanMemoryInfo stagingBufferMemory;
		VulkanAllocator::AllocateBuffer(imageSize, { BufferUsage::TransferSrc }, MemoryUsage::CPU_AND_GPU, stagingBuffer, stagingBufferMemory);

		// Copying the data
		void* copyData;
		VulkanAllocator::BindBuffer(stagingBuffer, stagingBufferMemory, &copyData);
		memcpy(copyData, data, static_cast<size_t>(imageSize));
		VulkanAllocator::UnbindBuffer(stagingBufferMemory);


		// Creating the image and allocating the needed buffer
		// (for creation we use `VK_IMAGE_LAYOUT_UNDEFINED` layout, which will be later changed to the user's input")
		Utils::CreateImage(specification.Width, specification.Height, 1, m_MipLevelCount,
			VK_IMAGE_TYPE_2D, textureFormat,
			VK_IMAGE_TILING_OPTIMAL,
			usageFlags | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // Need the trasnfer src/dst bits for generating the mips
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, // GPU only
			m_Image, m_ImageMemory
		);

		// Recording a temporary commandbuffer for transitioning
		VkCommandBuffer cmdBuf = VulkanContext::GetCurrentDevice()->AllocateCommandBuffer(RenderQueueType::Graphics, true);


		// Changing the layout for copying the staging buffer to the image
		TransitionLayout(cmdBuf, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, Utils::GetPipelineStageFlagsFromLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
		Utils::CopyBufferToImage(cmdBuf, stagingBuffer, m_Image, m_ImageSpecification.Width, m_ImageSpecification.Height, 1);


		// Generating mip maps if the mip count is higher than 1, else just transition to user's input `VkImageLayout`
		auto srcPipelineStageMask = Utils::GetPipelineStageFlagsFromLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		auto dstPipelineStageMask = Utils::GetPipelineStageFlagsFromLayout(newImageLayout);
		TransitionLayout(cmdBuf, newImageLayout, srcPipelineStageMask, dstPipelineStageMask);
#if 0
		if (specification.Mips > 1)
		{
			//TransitionLayout(cmdBuf, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			//	Utils::GetPipelineStageFlagsFromLayout(m_ImageLayout),
			//	Utils::GetPipelineStageFlagsFromLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
			//);

			GenerateMipMaps(cmdBuf, newImageLayout);
		}
		else
		{
			auto srcPipelineStageMask = Utils::GetPipelineStageFlagsFromLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			auto dstPipelineStageMask = Utils::GetPipelineStageFlagsFromLayout(newImageLayout);
			TransitionLayout(cmdBuf, newImageLayout, srcPipelineStageMask, dstPipelineStageMask);
		}
#endif

		// Ending the temporary commandbuffer for transitioning
		VulkanContext::GetCurrentDevice()->FlushCommandBuffer(cmdBuf);


		// Creating the image view
		Utils::CreateImageView(m_ImageView, m_Image, usageFlags, textureFormat, m_MipLevelCount, 1);

		// Creating the imageView mips
		for (uint32_t i = 0; i < m_MipLevelCount; i++)
		{
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

			VkImageViewCreateInfo createInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
			createInfo.image = m_Image;
			createInfo.format = textureFormat;
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

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

		// Creating the sampler
		VkFilter filtering = Utils::GetImageFiltering(specification.Sampler.SamplerFilter);
		VkSamplerAddressMode addressMode = Utils::GetImageWrap(specification.Sampler.SamplerWrap);
		VkSamplerMipmapMode mipMapMode = Utils::GetImageSamplerMipMapMode(specification.Sampler.SamplerFilter);
		VkSamplerReductionMode reductionMode = Utils::GetSamplerReductionMode(specification.Sampler.ReductionMode_Optional);
		Utils::CreateImageSampler(m_ImageSampler, filtering, addressMode, mipMapMode, m_MipLevelCount, reductionMode);

		// Setting the debug names for the structs
		VulkanContext::SetStructDebugName("Image2D-Image", VK_OBJECT_TYPE_IMAGE, m_Image);
		VulkanContext::SetStructDebugName("Image2D-ImageView", VK_OBJECT_TYPE_IMAGE_VIEW, m_ImageView);
		VulkanContext::SetStructDebugName("Image2D-Sampler", VK_OBJECT_TYPE_SAMPLER, m_ImageSampler);

		// Updating the descriptor
		UpdateDescriptor();

		// Deleting the staging buffer
		VulkanAllocator::DeleteBuffer(stagingBuffer, stagingBufferMemory);
	}

	VulkanImage2D::~VulkanImage2D()
	{
		Destroy();
	}

	void VulkanImage2D::Destroy()
	{
		if (m_Image == VK_NULL_HANDLE) return;

		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		VulkanAllocator::DestroyImage(m_Image, m_ImageMemory);
		vkDestroyImageView(device, m_ImageView, nullptr);
		vkDestroySampler(device, m_ImageSampler, nullptr);

		// Delete the image view mips
		for (auto& imaveView : m_Mips)
			vkDestroyImageView(device, imaveView.second, nullptr);

		m_Image = VK_NULL_HANDLE;
	}

	void VulkanImage2D::TransitionLayout(VkCommandBuffer cmdBuf, VkImageLayout newImageLayout,
										  VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask)
	{
		VkImageAspectFlags imageAspectFlags;
		if (m_ImageSpecification.Format == ImageFormat::Depth24Stencil8 || m_ImageSpecification.Format == ImageFormat::Depth32)
			imageAspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
		else
			imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;

		VkImageSubresourceRange subresourceRange{};
		subresourceRange.aspectMask = imageAspectFlags;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = m_MipLevelCount;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.layerCount = 1;
		Utils::SetImageLayout(cmdBuf, m_Image, m_ImageLayout, newImageLayout, subresourceRange, srcStageMask, dstStageMask);

		m_ImageLayout = newImageLayout;
	}

	void VulkanImage2D::GenerateMipMaps(VkCommandBuffer cmdBuffer, VkImageLayout newImageLayout)
	{
		VkFilter blitFilter{};
		VkImageAspectFlags imageAspectMask{};
		switch (m_ImageSpecification.Format)
		{
		case ImageFormat::Depth32:
		case ImageFormat::Depth24Stencil8:
			imageAspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			blitFilter = VK_FILTER_NEAREST;
			break;
		default:
			imageAspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blitFilter = VK_FILTER_LINEAR;
		}

		int32_t mipWidth = m_ImageSpecification.Width;
		int32_t mipHeight = m_ImageSpecification.Height;

		TransitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, Utils::GetPipelineStageFlagsFromLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
		
		for (uint32_t i = 1; i < m_MipLevelCount; i++)
		{
			VkImageSubresourceRange subresourceRange{};
			subresourceRange.aspectMask = imageAspectMask;
			subresourceRange.baseArrayLayer = 0;
			subresourceRange.layerCount = 1;
			subresourceRange.levelCount = 1;
			subresourceRange.baseMipLevel = i - 1;


			VkImageBlit blit{};

			// Src
			blit.srcSubresource.aspectMask = imageAspectMask;
			blit.srcOffsets[0] = { 0, 0, 0 };
			blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
			blit.srcSubresource.mipLevel = i - 1;
			blit.srcSubresource.baseArrayLayer = 0;
			blit.srcSubresource.layerCount = 1;

			// Dst
			blit.dstSubresource.aspectMask = imageAspectMask;
			blit.dstOffsets[0] = { 0, 0, 0 };
			blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
			blit.dstSubresource.mipLevel = i;
			blit.dstSubresource.baseArrayLayer = 0;
			blit.dstSubresource.layerCount = 1;


			// Transition from `VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL` to `VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL`
			Utils::InsertImageMemoryBarrier(cmdBuffer, m_Image,
				Utils::GetAccessFlagsFromLayout(m_ImageLayout), Utils::GetAccessFlagsFromLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
				m_ImageLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				Utils::GetPipelineStageFlagsFromLayout(m_ImageLayout), Utils::GetPipelineStageFlagsFromLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
				subresourceRange
			);

			vkCmdBlitImage(cmdBuffer,
				m_Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				m_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &blit,
				blitFilter
			);

			// Transition from `VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL` to "newImageLayout"
			Utils::InsertImageMemoryBarrier(cmdBuffer, m_Image,
				Utils::GetAccessFlagsFromLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL), Utils::GetAccessFlagsFromLayout(newImageLayout),
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, newImageLayout,
				Utils::GetPipelineStageFlagsFromLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL), Utils::GetPipelineStageFlagsFromLayout(newImageLayout),
				subresourceRange
			);

			if (mipWidth > 1) mipWidth /= 2;
			if (mipHeight > 1) mipHeight /= 2;
		}

		VkImageSubresourceRange subresourceRange{};
		subresourceRange.aspectMask = imageAspectMask;
		subresourceRange.baseArrayLayer = 0;
		subresourceRange.layerCount = 1;
		subresourceRange.levelCount = 1;
		subresourceRange.baseMipLevel = m_MipLevelCount - 1;

		Utils::InsertImageMemoryBarrier(cmdBuffer, m_Image,
			Utils::GetAccessFlagsFromLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL), Utils::GetAccessFlagsFromLayout(newImageLayout),
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, newImageLayout,
			Utils::GetPipelineStageFlagsFromLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL), Utils::GetPipelineStageFlagsFromLayout(newImageLayout),
			subresourceRange
		);

		m_ImageLayout = newImageLayout;
	}

	void VulkanImage2D::BlitImage(VkCommandBuffer cmdBuf, const Ref<Image2D>& srcImage, uint32_t mipLevel)
	{
		VkImageBlit blit{};

		VkFilter blitFilter{};
		VkImageAspectFlags imageAspectMask{};
		switch (m_ImageSpecification.Format)
		{
		case ImageFormat::Depth32:
		case ImageFormat::Depth24Stencil8:
			imageAspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			blitFilter = VK_FILTER_NEAREST;
			break;
		default:
			imageAspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blitFilter = VK_FILTER_LINEAR;
		}

		VkImageSubresourceRange subresourceRange{};
		subresourceRange.aspectMask = imageAspectMask;
		subresourceRange.baseArrayLayer = 0;
		subresourceRange.layerCount = 1;
		subresourceRange.levelCount = 1;
		subresourceRange.baseMipLevel = mipLevel;


		// #SOURCE
		// Getting all the information neccesary
		Ref<VulkanImage2D> vulkanSrcImage = srcImage.As<VulkanImage2D>();
		int32_t blitImageWidth = vulkanSrcImage->m_ImageSpecification.Width;
		int32_t blitImageHeight = vulkanSrcImage->m_ImageSpecification.Height;
		VkImage vulkanSrcRawImage = vulkanSrcImage->GetVulkanImage();
		VkImageLayout initialSrcImageLayout = vulkanSrcImage->GetVulkanImageLayout();

		// Calculate the area of the mip level
		if (mipLevel != 0)
		{
			blitImageWidth /= mipLevel;
			blitImageHeight /= mipLevel;
		}

		blit.srcSubresource.aspectMask = imageAspectMask;
		blit.srcOffsets[0] = { 0, 0, 0 };
		blit.srcOffsets[1] = { blitImageWidth, blitImageHeight, 1 };
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = 1;
		blit.srcSubresource.mipLevel = mipLevel;

		// Transition the src image to `VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL`
		Utils::InsertImageMemoryBarrier(cmdBuf, vulkanSrcRawImage,
			Utils::GetAccessFlagsFromLayout(initialSrcImageLayout),
			Utils::GetAccessFlagsFromLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
			initialSrcImageLayout,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			Utils::GetPipelineStageFlagsFromLayout(initialSrcImageLayout),
			Utils::GetPipelineStageFlagsFromLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
			subresourceRange
		);




		// #DESTIONATION
		// Transition the dst (this class) image to `VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL`
		Utils::InsertImageMemoryBarrier(cmdBuf, m_Image,
			Utils::GetAccessFlagsFromLayout(m_ImageLayout),
			Utils::GetAccessFlagsFromLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
			m_ImageLayout,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			Utils::GetPipelineStageFlagsFromLayout(m_ImageLayout),
			Utils::GetPipelineStageFlagsFromLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
			subresourceRange
		);
		blit.dstSubresource.aspectMask = imageAspectMask;
		blit.dstOffsets[0] = { 0, 0, 0 };
		blit.dstOffsets[1] = { blitImageWidth, blitImageHeight, 1 };
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = 1;
		blit.dstSubresource.mipLevel = mipLevel;

		vkCmdBlitImage(cmdBuf,
			vulkanSrcRawImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			m_Image,		   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &blit,
			blitFilter
		);


		// Transition both images to the layouts they had before the blit
		Utils::InsertImageMemoryBarrier(cmdBuf, vulkanSrcRawImage,
			Utils::GetAccessFlagsFromLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
			Utils::GetAccessFlagsFromLayout(initialSrcImageLayout),
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			initialSrcImageLayout,
			Utils::GetPipelineStageFlagsFromLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
			Utils::GetPipelineStageFlagsFromLayout(initialSrcImageLayout),
			subresourceRange
		);

		Utils::InsertImageMemoryBarrier(cmdBuf, m_Image,
			Utils::GetAccessFlagsFromLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
			Utils::GetAccessFlagsFromLayout(m_ImageLayout),
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			m_ImageLayout,
			Utils::GetPipelineStageFlagsFromLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
			Utils::GetPipelineStageFlagsFromLayout(m_ImageLayout),
			subresourceRange
		);
	}

	void VulkanImage2D::UpdateDescriptor()
	{
		m_DescriptorInfo[DescriptorImageType::Sampled].imageView = m_ImageView;
		m_DescriptorInfo[DescriptorImageType::Sampled].imageLayout = m_ImageLayout;
		m_DescriptorInfo[DescriptorImageType::Sampled].sampler = m_ImageSampler;

		m_DescriptorInfo[DescriptorImageType::Storage].imageView = m_ImageView;
		m_DescriptorInfo[DescriptorImageType::Storage].imageLayout = m_ImageLayout;
		m_DescriptorInfo[DescriptorImageType::Storage].sampler = nullptr;
	}

	namespace Utils
	{

		void CreateImage(uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevels,
						 VkImageType type, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
						 VkMemoryPropertyFlags properties,
						 VkImage& image, VulkanMemoryInfo& imageMemory)
		{
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

			VkImageCreateInfo imageInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
			imageInfo.imageType = type;
			imageInfo.extent.width = width;
			imageInfo.extent.height = height;
			imageInfo.extent.depth = 1; //TODO: Take in another parameter (for 3D textures?)
			imageInfo.mipLevels = mipLevels;
			imageInfo.arrayLayers = depth;
			imageInfo.format = format;
			imageInfo.tiling = tiling;
			imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageInfo.usage = usage;
			imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			imageInfo.flags = depth == 6 ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0; // If the image has 6 faces, then it is a cubemap

			VulkanAllocator::AllocateImage(imageInfo, MemoryUsage::GPU_ONLY, image, imageMemory);
		}

		void CreateImageView(VkImageView& imageView, VkImage image, VkImageUsageFlags imageUsage, VkFormat format, uint32_t mipLevels, uint32_t textureDepth)
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

		void CreateImageSampler(VkSampler& sampler, VkFilter filtering, VkSamplerAddressMode samplerAdressMode, VkSamplerMipmapMode samplerMipMapMode, uint32_t mipLevels, VkSamplerReductionMode reductionMode)
		{
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
			VkPhysicalDevice physicalDevice = VulkanContext::GetCurrentDevice()->GetPhysicalDevice();

			VkPhysicalDeviceProperties properties;
			vkGetPhysicalDeviceProperties(physicalDevice, &properties);

			VkSamplerCreateInfo samplerInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
			samplerInfo.magFilter = filtering;
			samplerInfo.minFilter = filtering;
			
			samplerInfo.addressModeU = samplerAdressMode;
			samplerInfo.addressModeV = samplerAdressMode;
			samplerInfo.addressModeW = samplerAdressMode;

			samplerInfo.anisotropyEnable = VK_TRUE;
			samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
			samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
			samplerInfo.unnormalizedCoordinates = VK_FALSE;
			samplerInfo.compareEnable = VK_FALSE;
			samplerInfo.compareOp = VK_COMPARE_OP_NEVER;

			samplerInfo.mipmapMode = samplerMipMapMode;
			samplerInfo.mipLodBias = 0.0f;
			samplerInfo.minLod = 0.0f;
			samplerInfo.maxLod = static_cast<float>(mipLevels);

			// Optional features (for depth pyramid mostly)
			VkSamplerReductionModeCreateInfoEXT createInfoReduction{ VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT };
			if (reductionMode != VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE)
			{
				createInfoReduction.reductionMode = reductionMode;

				samplerInfo.pNext = &createInfoReduction;
			}


			FROST_VKCHECK(vkCreateSampler(device, &samplerInfo, nullptr, &sampler));
		}

#if 0
		void ChangeImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t depthMap, uint32_t mipLevels)
		{
			VkCommandBuffer cmdBuf = VulkanContext::GetCurrentDevice()->AllocateCommandBuffer(true);

		}
#endif

		void CopyBufferToImage(VkCommandBuffer cmdBuf, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t depth)
		{
			VkBufferImageCopy region{};
			region.bufferOffset = 0;
			region.bufferRowLength = 0;
			region.bufferImageHeight = 0;

			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.mipLevel = 0;
			region.imageSubresource.layerCount = 1;
			region.imageSubresource.baseArrayLayer = 0;

			region.imageOffset = { 0, 0, 0 };
			region.imageExtent = {
				width,
				height,
				depth
			};

			vkCmdCopyBufferToImage(cmdBuf, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
		}


		VkFormat GetImageFormat(ImageFormat imageFormat)
		{
			switch (imageFormat)
			{
				case ImageFormat::R8:               return VK_FORMAT_R8_UNORM;
				case ImageFormat::R32:              return VK_FORMAT_R32_SFLOAT;
				case ImageFormat::RGBA8:            return VK_FORMAT_R8G8B8A8_UNORM;
				case ImageFormat::RGBA16F:          return VK_FORMAT_R16G16B16A16_SFLOAT;
				case ImageFormat::RGBA32F:          return VK_FORMAT_R32G32B32A32_SFLOAT;
				case ImageFormat::Depth24Stencil8:  return VK_FORMAT_D24_UNORM_S8_UINT;
				case ImageFormat::Depth32:          return VK_FORMAT_D32_SFLOAT;
				case ImageFormat::None:             FROST_ASSERT_MSG("ImageFormat::None is invalid!");
			}
			return VkFormat();
		}

		VkFilter GetImageFiltering(ImageFilter imageFiltering)
		{
			switch (imageFiltering)
			{
				case ImageFilter::Linear:   return VK_FILTER_LINEAR;
				case ImageFilter::Nearest:  return VK_FILTER_NEAREST;
			}
			FROST_ASSERT_MSG("ImageFilter is invalid!");
			return VkFilter();
		}

		VkSamplerAddressMode GetImageWrap(ImageWrap imageWrap)
		{
			switch (imageWrap)
			{
				case ImageWrap::Repeat:		 return VK_SAMPLER_ADDRESS_MODE_REPEAT;
				case ImageWrap::ClampToEdge: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			}
			FROST_ASSERT_MSG("ImageWrap is invalid!");
			return VkSamplerAddressMode();
		}

		VkSamplerMipmapMode GetImageSamplerMipMapMode(ImageFilter imageFiltering)
		{
			switch (imageFiltering)
			{
			case ImageFilter::Linear:   return VK_SAMPLER_MIPMAP_MODE_LINEAR;
			case ImageFilter::Nearest:  return VK_SAMPLER_MIPMAP_MODE_NEAREST;
			}
			FROST_ASSERT_MSG("ImageFilter is invalid!");
			return VkSamplerMipmapMode();
		}

		VkImageUsageFlags GetImageUsageFlags(ImageUsage imageUsage)
		{
			switch (imageUsage)
			{
				case ImageUsage::Storage:         return VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
													   VK_IMAGE_USAGE_SAMPLED_BIT;
				case ImageUsage::ReadOnly:		  return VK_IMAGE_USAGE_SAMPLED_BIT;
				case ImageUsage::ColorAttachment: return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
				case ImageUsage::DepthStencil:    return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
				case ImageUsage::None:            FROST_ASSERT_MSG("ImageUsage::None is invalid!");
			}
			return VkImageUsageFlags();
		}

		VkImageLayout GetImageLayout(ImageUsage imageUsage)
		{
			switch (imageUsage)
			{
				case ImageUsage::Storage:            return VK_IMAGE_LAYOUT_GENERAL;
				case ImageUsage::ReadOnly:           return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				case ImageUsage::ColorAttachment:    return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				case ImageUsage::DepthStencil:       return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL;
				case ImageUsage::None:               FROST_ASSERT_MSG("ImageUsage::None is invalid!");
			}
			return VkImageLayout();
		}

		VkAccessFlags GetAccessFlagsFromLayout(VkImageLayout imageLayout)
		{
			switch (imageLayout)
			{
				case VK_IMAGE_LAYOUT_PREINITIALIZED:                   return VK_ACCESS_HOST_WRITE_BIT;
				case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:             return VK_ACCESS_TRANSFER_WRITE_BIT;
				case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:             return VK_ACCESS_TRANSFER_READ_BIT;
				case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:         return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:         return VK_ACCESS_SHADER_READ_BIT;
			}
			return VK_ACCESS_NONE_KHR;
		}

		VkSamplerReductionMode GetSamplerReductionMode(ReductionMode reductionMode)
		{
			switch (reductionMode)
			{
				case ReductionMode::Min:    return VK_SAMPLER_REDUCTION_MODE_MIN;
				case ReductionMode::Max:    return VK_SAMPLER_REDUCTION_MODE_MAX;
				case ReductionMode::None:	return VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE;
			}
			return VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE;
		}

		VkPipelineStageFlags GetPipelineStageFlagsFromLayout(VkImageLayout imageLayout)
		{
			switch (imageLayout)
			{
				case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
				case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:             return VK_PIPELINE_STAGE_TRANSFER_BIT;
				case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:         return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
				case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:         return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
				case VK_IMAGE_LAYOUT_PREINITIALIZED:                   return VK_PIPELINE_STAGE_HOST_BIT;
				case VK_IMAGE_LAYOUT_UNDEFINED:                        return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			}
			return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		}

		VkDeviceSize CalculateImageBufferSize(uint32_t width, uint32_t height, ImageFormat imageFormat)
		{
			switch (imageFormat)
			{
				case ImageFormat::RGBA8:     return width * height * sizeof(float);
				case ImageFormat::RGBA16F:   return width * height * sizeof(float) * 2;
				case ImageFormat::RGBA32F:   return width * height * sizeof(float) * 4;
				case ImageFormat::None:      FROST_ASSERT_MSG("ImageFormat::None is invalid!");
			}
			return 0;
		}
	}
}