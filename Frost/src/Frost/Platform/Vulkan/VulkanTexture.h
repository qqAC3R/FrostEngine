#pragma once

#include "Frost/Renderer/Texture.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanBufferAllocator.h"

namespace Frost
{
	enum class DescriptorImageType;

	class VulkanTexture2D : public Texture2D
	{
	public:
		VulkanTexture2D(const std::string& filepath, const TextureSpecification& textureSpec);
		virtual ~VulkanTexture2D();

		virtual void Destroy() override;

		virtual uint32_t GetWidth() const override { return m_Width; }
		virtual uint32_t GetHeight() const override { return m_Height; }

		virtual TextureSpecification& GetSpecification() override { return m_TextureSpecification; }
		virtual const TextureSpecification& GetSpecification() const override { return m_TextureSpecification; }

		virtual const Ref<Image2D> GetImage2D() const override { return m_Image; }

		virtual bool Loaded() const override { return m_IsLoaded; }

		VkDescriptorImageInfo& GetVulkanDescriptorInfo(DescriptorImageType imageType) {
			FROST_ASSERT_INTERNAL((m_DescriptorInfo.find(imageType) != m_DescriptorInfo.end()));
			return m_DescriptorInfo[imageType];
		}
	private:
		Ref<Image2D> m_Image = nullptr;
		TextureSpecification m_TextureSpecification;
		uint32_t m_Width;
		uint32_t m_Height;
		uint32_t m_MipMapLevels = 1;

		HashMap<DescriptorImageType, VkDescriptorImageInfo> m_DescriptorInfo;
		bool m_IsLoaded;
	};

	class VulkanTextureCubeMap : public TextureCubeMap
	{
	public:
		VulkanTextureCubeMap(const ImageSpecification& imageSpecification);
		virtual ~VulkanTextureCubeMap();

		virtual void Destroy() override;

		void TransitionLayout(VkCommandBuffer cmdBuf, VkImageLayout newImageLayout,
			VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

		void GenerateMipMaps(VkCommandBuffer cmdBuffer, VkImageLayout newImageLayout);

		virtual uint32_t GetWidth() const override { return m_ImageSpecification.Width; }
		virtual uint32_t GetHeight() const override { return m_ImageSpecification.Height; }

		virtual ImageSpecification& GetSpecification() override { return m_ImageSpecification; }
		virtual const ImageSpecification& GetSpecification() const override { return m_ImageSpecification; }

		VkSampler GetVulkanSampler() const { return m_ImageSampler; }
		VkImage GetVulkanImage() const { return m_Image; }
		VkImageView GetVulkanImageView() const { return m_ImageView; }
		VkImageLayout GetVulkanImageLayout() const { return m_ImageLayout; }
		VkImageView GetVulkanImageViewMip(uint32_t mip) { return m_Mips[mip]; }

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

		std::unordered_map<uint32_t, VkImageView> m_Mips;

		ImageSpecification m_ImageSpecification;
		HashMap<DescriptorImageType, VkDescriptorImageInfo> m_DescriptorInfo;
	};

}