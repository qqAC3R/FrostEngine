#include "frostpch.h"
#include "VulkanTexture.h"

#include "VulkanImage.h"

#include "Frost/Platform/Vulkan/VulkanRenderer.h"
#include "Frost/Platform/Vulkan/VulkanContext.h"
#include "Frost/Asset/AssetManager.h"

#include <stb_image.h>
//#include <dds.hpp>
//#include <dds_loader.h>

#include <compressonator.h>
#include <helpers/dds_helpers.h>

namespace Frost
{
	/////////////////////////////////////////////////////
	// VULKAN TEXTURE 2D
	/////////////////////////////////////////////////////
	VulkanTexture2D::VulkanTexture2D(const std::string& filepath, const TextureSpecification& textureSpec)
		: m_TextureSpecification(textureSpec), m_Filepath(filepath)
	{
		// Loading the texture
		int width, height, channels;
		ImageFormat imageFormat;

		std::filesystem::path systenFilepath = filepath;
		std::string extension = systenFilepath.extension().string();

		ImageSpecification imageSpec{};


		//dds::readFile
		CMP_Texture compressedTexture{};

		if (extension == ".dds")
		{
			//CMP_InitializeBCLibrary();
			LoadDDSFile(filepath.c_str(), compressedTexture);

			switch (compressedTexture.format)
			{
				case CMP_FORMAT_DXT1:
				case CMP_FORMAT_BC1: 
				{
					imageFormat = compressedTexture.nAlphaChannelSupport ?  ImageFormat::RGBA_BC1 : ImageFormat::RGB_BC1;
					break;
				}

				case CMP_FORMAT_DXT3: imageFormat = ImageFormat::BC2; break;
				case CMP_FORMAT_DXT5: imageFormat = ImageFormat::BC3; break;

				case CMP_FORMAT_ATI2N_XY:
				case CMP_FORMAT_BC5: imageFormat = ImageFormat::BC5; break;

				default:
				{
					imageFormat = ImageFormat::RGBA8;
					FROST_ASSERT_MSG("Unknown format!"); break;
				}
			}

			m_TextureData.Data = (void*)compressedTexture.pData;
			m_TextureData.Size = compressedTexture.dwDataSize;

			width = compressedTexture.dwWidth;
			height = compressedTexture.dwHeight;

			m_IsLoaded = true;
		}
		else if (textureSpec.Format == ImageFormat::RGB_BC1 ||
				 textureSpec.Format == ImageFormat::RGBA_BC1 ||
				 textureSpec.Format == ImageFormat::BC2 ||
				 textureSpec.Format == ImageFormat::BC3 ||
				 textureSpec.Format == ImageFormat::BC4 ||
				 textureSpec.Format == ImageFormat::BC5)
		{

			CMP_MipSet textureMipSet;
			CMP_LoadTexture(filepath.c_str(), &textureMipSet);


			// Generate the mips before compressing, so when it will compress, it will do the mips as well.
			{
				uint32_t totalMipMapLevels = Utils::CalculateMipMapLevels(textureMipSet.dwWidth, textureMipSet.dwHeight);
				uint32_t compressedMipMapLevels = (uint32_t)glm::max((int32_t)totalMipMapLevels - 2, 0);
				CMP_GenerateMIPLevels(&textureMipSet, compressedMipMapLevels);
			}



			// Initialize the compressed mip set and compress the mips.
			CMP_MipSet compresstedMipSet;
			{
				compresstedMipSet.m_nWidth = textureMipSet.m_nWidth;
				compresstedMipSet.m_nHeight = textureMipSet.m_nHeight;
				compresstedMipSet.m_nDepth = textureMipSet.m_nDepth;

				// Set the compression parameters
				CMP_CompressOptions options = { 0 };
				options.dwSize = sizeof(options);
				options.fquality = 0.05f;
				options.dwnumThreads = 8;

				switch (textureSpec.Format)
				{
					case ImageFormat::RGB_BC1:
					case ImageFormat::RGBA_BC1:  options.DestFormat = CMP_FORMAT_BC1; break;
					case ImageFormat::BC2:		 options.DestFormat = CMP_FORMAT_BC2; break;
					case ImageFormat::BC3:		 options.DestFormat = CMP_FORMAT_BC3; break;
					case ImageFormat::BC4:		 options.DestFormat = CMP_FORMAT_BC4; break;
					case ImageFormat::BC5:		 options.DestFormat = CMP_FORMAT_BC5; break;
					default: FROST_ASSERT_MSG("Something is wrong! :(");
				}

				// Compress
				CMP_ERROR status = CMP_ConvertMipTexture(&textureMipSet, &compresstedMipSet, &options, nullptr);
			}

			
			// Pack all the compressed mip set into a single buffer for it to copy more efficiently into the image buffer
			Buffer compressedMipSetBuffer{};
			{
				// Compute the total buffer size required
				uint64_t totalBufferSize = 0;
				for (uint32_t i = 0; i < compresstedMipSet.m_nMipLevels; i++)
				{
					totalBufferSize += Utils::CalculateImageBufferSize(
						compresstedMipSet.m_pMipLevelTable[i]->m_nWidth,
						compresstedMipSet.m_pMipLevelTable[i]->m_nHeight,
						textureSpec.Format
					);
				}

				// Allocate the whole buffer
				compressedMipSetBuffer.Allocate(totalBufferSize);
				

				uint64_t bufferOffset = 0;
				for (uint32_t i = 0; i < compresstedMipSet.m_nMipLevels; i++)
				{
					// Get the current mip image buffer size
					uint64_t mipBufferSize = Utils::CalculateImageBufferSize(
						compresstedMipSet.m_pMipLevelTable[i]->m_nWidth,
						compresstedMipSet.m_pMipLevelTable[i]->m_nHeight,
						textureSpec.Format
					);
					
					// Write into the buffer with an offset
					compressedMipSetBuffer.Write(compresstedMipSet.m_pMipLevelTable[i]->m_pbData, mipBufferSize, bufferOffset);

					// Add the current mip buffer size to the buffer offset
					bufferOffset += mipBufferSize;
				}
			}

			m_TextureData.Data = (void*)compressedMipSetBuffer.Data;
			m_TextureData.Size = compressedMipSetBuffer.GetSize();

			width = compresstedMipSet.m_pMipLevelTable[0]->m_nWidth;
			height = compresstedMipSet.m_pMipLevelTable[0]->m_nHeight;
			imageFormat = textureSpec.Format;

			m_IsLoaded = true;

			// After copying the buffers into our own packed mip buffer, we do not those anymore
			CMP_FreeMipSet(&textureMipSet);
			CMP_FreeMipSet(&compresstedMipSet);
		}
		else
		{
			stbi_set_flip_vertically_on_load(textureSpec.FlipTexture);

			if (stbi_is_hdr(filepath.c_str()))
			{
				m_TextureData.Data = (void*)stbi_loadf(filepath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
				m_TextureData.Size = width * height * 4 * sizeof(float);
				imageFormat = ImageFormat::RGBA32F;
			}
			else
			{
				m_TextureData.Data = (void*)stbi_load(filepath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
				m_TextureData.Size = width * height * sizeof(float);
				imageFormat = ImageFormat::RGBA8;
			}

			if (m_TextureData.Data == nullptr)
			{
				FROST_CORE_WARN("Texture with filepath '{0}' hasn't been found", filepath);
				m_IsLoaded = false;
				return;
			}
			else
			{
				m_IsLoaded = true;
			}
		}

		m_Width = width;
		m_Height = height;
		m_TextureSpecification.Format = imageFormat;

		if (textureSpec.UseMips)
			m_MipMapLevels = Utils::CalculateMipMapLevels(width, height);
				
		imageSpec.Width = width;
		imageSpec.Height = height;
		imageSpec.UseMipChain = m_TextureSpecification.UseMips;
		imageSpec.Sampler.SamplerFilter = m_TextureSpecification.Sampler.SamplerFilter;
		imageSpec.Sampler.SamplerWrap = m_TextureSpecification.Sampler.SamplerWrap;
		imageSpec.Usage = m_TextureSpecification.Usage;
		imageSpec.Format = m_TextureSpecification.Format;
		m_Image = Image2D::Create(imageSpec, m_TextureData);

		Ref<VulkanImage2D> vulkanImage = m_Image.As<VulkanImage2D>();
		m_DescriptorInfo[DescriptorImageType::Sampled] = vulkanImage->GetVulkanDescriptorInfo(DescriptorImageType::Sampled);
		m_DescriptorInfo[DescriptorImageType::Storage] = vulkanImage->GetVulkanDescriptorInfo(DescriptorImageType::Storage);
	}

	VulkanTexture2D::VulkanTexture2D(uint32_t width, uint32_t height, const TextureSpecification& textureSpec, const void* data)
	{
		m_Width = width;
		m_Height = height;
		if (textureSpec.UseMips)
			m_MipMapLevels = Utils::CalculateMipMapLevels(width, height);

		ImageSpecification imageSpec{};
		imageSpec.Width = width;
		imageSpec.Height = height;
		imageSpec.UseMipChain = textureSpec.UseMips;
		imageSpec.Sampler.SamplerFilter = textureSpec.Sampler.SamplerFilter;
		imageSpec.Sampler.SamplerWrap = textureSpec.Sampler.SamplerWrap;
		imageSpec.Usage = textureSpec.Usage;
		imageSpec.Format = textureSpec.Format;
		
		if (data)
		{
			Buffer copyByffer{};
			copyByffer.Data = const_cast<void*>(data); // This is kind of a lifehack and could lead to various bugs if the data passed was initially intended to be const
			copyByffer.Size = Utils::CalculateImageBufferSize(width, height, imageSpec.Format);

			m_Image = Image2D::Create(imageSpec, copyByffer);
			m_IsLoaded = true;
		}
		else
		{
			m_Image = Image2D::Create(imageSpec);
			m_IsLoaded = false;
		}

		Ref<VulkanImage2D> vulkanImage = m_Image.As<VulkanImage2D>();
		m_DescriptorInfo[DescriptorImageType::Sampled] = vulkanImage->GetVulkanDescriptorInfo(DescriptorImageType::Sampled);
		m_DescriptorInfo[DescriptorImageType::Storage] = vulkanImage->GetVulkanDescriptorInfo(DescriptorImageType::Storage);

		m_TextureData.Allocate(width * height * sizeof(glm::vec4) / 4.0f);
	}

	void VulkanTexture2D::SubmitDataToGPU()
	{
		Ref<VulkanImage2D> vulkanImage = m_Image.As<VulkanImage2D>();
		VkImageLayout newImageLayout = Utils::GetImageLayout(m_TextureSpecification.Usage);

		uint32_t imageSize = Utils::CalculateImageBufferSize(m_Width, m_Height, m_TextureSpecification.Format);

		// Making a staging buffer to copy the data
		VkBuffer stagingBuffer;
		VulkanMemoryInfo stagingBufferMemory;
		VulkanAllocator::AllocateBuffer(imageSize, { BufferUsage::TransferSrc }, MemoryUsage::CPU_TO_GPU, stagingBuffer, stagingBufferMemory);

		// Copying the data
		void* copyData;
		VulkanAllocator::BindBuffer(stagingBuffer, stagingBufferMemory, &copyData);
		memcpy(copyData, m_TextureData.Data, static_cast<size_t>(imageSize));
		VulkanAllocator::UnbindBuffer(stagingBufferMemory);


		// Recording a temporary commandbuffer for transitioning
		VkCommandBuffer cmdBuf = VulkanContext::GetCurrentDevice()->AllocateCommandBuffer(RenderQueueType::Graphics, true);

		// Changing the layout for copying the staging buffer to the image
		vulkanImage->TransitionLayout(cmdBuf, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, Utils::GetPipelineStageFlagsFromLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));

		Utils::CopyBufferToImage(cmdBuf,
			stagingBuffer,
			vulkanImage->GetVulkanImage(),
			m_Width,
			m_Height,
			1
		);


		// Generating mip maps if the mip count is higher than 1, else just transition to user's input `VkImageLayout`
		auto srcPipelineStageMask = Utils::GetPipelineStageFlagsFromLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		auto dstPipelineStageMask = Utils::GetPipelineStageFlagsFromLayout(newImageLayout);
		vulkanImage ->TransitionLayout(cmdBuf, newImageLayout, srcPipelineStageMask, dstPipelineStageMask);

		// Ending the temporary commandbuffer for transitioning
		VulkanContext::GetCurrentDevice()->FlushCommandBuffer(cmdBuf);

		VulkanAllocator::DeleteBuffer(stagingBuffer, stagingBufferMemory);

		m_IsLoaded = true;
	}

	void VulkanTexture2D::SetToWriteableBuffer(void* data)
	{
		memcpy(m_TextureData.Data, data, m_TextureData.Size);
	}

	Buffer VulkanTexture2D::GetWritableBuffer()
	{
		return m_TextureData;
	}

	void VulkanTexture2D::GenerateMipMaps()
	{
		bool isCompressed = m_TextureSpecification.Format == ImageFormat::RGB_BC1 || 
							m_TextureSpecification.Format == ImageFormat::RGBA_BC1 ||
							m_TextureSpecification.Format == ImageFormat::BC2 ||
							m_TextureSpecification.Format == ImageFormat::BC3 ||
							m_TextureSpecification.Format == ImageFormat::BC5;

		if (!m_TextureSpecification.UseMips || isCompressed) return;

		// TODO: Im not sure if this works while another command buffer is being recorded
		VkCommandBuffer cmdBuf = VulkanContext::GetCurrentDevice()->AllocateCommandBuffer(RenderQueueType::Graphics, true);
		auto vulkanImage = m_Image.As<VulkanImage2D>();
		vulkanImage->GenerateMipMaps(cmdBuf, vulkanImage->GetVulkanImageLayout());
		VulkanContext::GetCurrentDevice()->FlushCommandBuffer(cmdBuf);
	}

	bool VulkanTexture2D::ReloadData(const std::string& filepath)
	{
		std::string totalFilepath = AssetManager::GetFileSystemPathString(AssetManager::GetMetadata(filepath));
		
		// Loading the texture
		int width, height, channels;
		ImageFormat imageFormat;

		stbi_set_flip_vertically_on_load(m_TextureSpecification.FlipTexture);

		if (stbi_is_hdr(totalFilepath.c_str()))
		{
			m_TextureData.Data = (void*)stbi_loadf(totalFilepath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
			m_TextureData.Size = width * height * 4 * sizeof(float);
			imageFormat = ImageFormat::RGBA32F;
		}
		else
		{
			m_TextureData.Data = (void*)stbi_load(totalFilepath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
			m_TextureData.Size = width * height * 4;
			imageFormat = ImageFormat::RGBA8;
		}

		if (m_TextureData.Data == nullptr)
		{
			FROST_CORE_WARN("Texture with filepath '{0}' hasn't been found", totalFilepath);
			m_IsLoaded = false;
			return false;
		}
		else
		{
			m_IsLoaded = true;
		}

		// Else, we can just submit the data to the GPU!
		SubmitDataToGPU();

		if (m_TextureSpecification.UseMips)
			GenerateMipMaps();

		return true;
	}

	void VulkanTexture2D::Destroy()
	{
		if (m_Image)
		{
			m_Image->Destroy();
		}

		if (m_TextureData.Data != nullptr)
		{
			// Free the CPU memory allocated for the texture
			free(m_TextureData.Data);
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
			ImageMemoryProperties::GPU_ONLY,
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
			FROST_ASSERT_INTERNAL("Could not find a matching memory type");
			return 0;
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
			ImageMemoryProperties::GPU_ONLY,
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
		int32_t mipDepth = m_ImageSpecification.Depth;

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
			blit.srcOffsets[1] = { mipWidth, mipHeight, mipDepth };
			blit.srcSubresource.baseArrayLayer = 0;
			blit.srcSubresource.layerCount = 1;
			blit.srcSubresource.mipLevel = i - 1;

			// Dst
			blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.dstOffsets[0] = { 0, 0, 0 };
			blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, mipDepth > 1 ? mipDepth / 2 : 1 };
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
			if (mipDepth > 1) mipDepth /= 2;
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
		m_MipLevelCount = Utils::CalculateMipMapLevels(Utils::CalculateMipMapLevels(m_ImageSpecification.Width, m_ImageSpecification.Height), m_ImageSpecification.Depth);

		uint32_t mipWidth = m_ImageSpecification.Width;
		uint32_t mipHeight = m_ImageSpecification.Height;
		uint32_t mipDepth = m_ImageSpecification.Depth;

		for (uint32_t mip = 0; mip < m_MipLevelCount; mip++)
		{
			if (mip != 0)
			{
				mipWidth /= 2;
				mipHeight /= 2;
				mipDepth /= 2;
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