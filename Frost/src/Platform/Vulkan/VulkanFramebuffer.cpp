#include "frostpch.h"
#include "VulkanFramebuffer.h"

#include "Platform/Vulkan/VulkanContext.h"

namespace Frost
{

	namespace Utils
	{

		static std::pair<VkImageUsageFlags, VkFormat> FBFormatToVK(FramebufferTextureFormat format)
		{
			switch (format)
			{
				case Frost::FramebufferTextureFormat::RGBA8:	return std::make_pair(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_FORMAT_R8G8B8A8_SRGB);
				case Frost::FramebufferTextureFormat::RGBA16F:	return std::make_pair(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_FORMAT_R16G16B16A16_SFLOAT);
				case Frost::FramebufferTextureFormat::DEPTH32:  return std::make_pair(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_FORMAT_D32_SFLOAT);
			}

			FROST_ASSERT(false, "");
			return {};
		}

		static bool IsDepthFormat(FramebufferTextureFormat format)
		{
			switch (format)
			{
				case FramebufferTextureFormat::DEPTH32:  return true;
			}

			return false;
		}
	}

	VulkanFramebuffer::VulkanFramebuffer(void* renderPass, const FramebufferSpecification& spec)
		: m_Specification(spec)
	{
		CreateAttachments();
		CreateFramebuffer(renderPass);
	}

	VulkanFramebuffer::~VulkanFramebuffer()
	{
	}

	void VulkanFramebuffer::Resize(uint32_t width, uint32_t height)
	{

	}

	void VulkanFramebuffer::Destroy()
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();


		for (auto& attachment : m_Attachments)
		{
			attachment->Destroy();
		}

		vkDestroyFramebuffer(device, m_Framebuffer, nullptr);
	}

	void VulkanFramebuffer::CreateAttachments()
	{
		for (auto spec : m_Specification.Attachments.Attachments)
		{
			if (!Utils::IsDepthFormat(spec.TextureFormat))
				m_ColorAttachmentSpecifications.emplace_back(spec);
			else
				m_DepthAttachmentSpecification = spec;


		}

		for (uint32_t i = 0; i < m_Specification.Attachments.Attachments.size(); i++)
		{
			FramebufferTextureSpecification attachment = m_Specification.Attachments.Attachments[i];
			if (!attachment.Components.Image)
			{
				TextureSpecs spec{};
				spec.Width = m_Specification.Width;
				spec.Height = m_Specification.Height;
				spec.Format = (TextureSpecs::FormatSpec)attachment.TextureFormat;

				if (attachment.TextureFormat == FramebufferTextureFormat::Depth)
					spec.Usage = { TextureSpecs::UsageSpec::DepthStencilAttachment };
				else
					spec.Usage = { TextureSpecs::UsageSpec::ColorAttachment, TextureSpecs::UsageSpec::Storage };

				Ref<Image2D> texture = Image2D::Create(spec);
				m_Attachments.push_back(texture);
			}
			else
			{
				TextureSpecs spec{};
				spec.Width = m_Specification.Width;
				spec.Height = m_Specification.Height;
				spec.Usage = { TextureSpecs::UsageSpec::ColorAttachment };
				spec.Format = TextureSpecs::FormatSpec::SWAPCHAIN;

				Ref<Image2D> texture = Image2D::Create(attachment.Components.Image, spec);
				m_Attachments.push_back(texture);

			}

		}

	}

	void VulkanFramebuffer::CreateFramebuffer(void* renderPass)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();



		std::vector<VkImageView> attachments;
		for (auto& attachment : m_Attachments)
		{
			attachments.push_back(attachment->GetVulkanImageView());
		}


		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = (VkRenderPass)renderPass;
		framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = m_Specification.Width;
		framebufferInfo.height = m_Specification.Height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &m_Framebuffer) != VK_SUCCESS)
		{
			FROST_ASSERT(0, "Failed to create framebuffer!");
		}

		VulkanContext::SetStructDebugName("Framebuffer", VK_OBJECT_TYPE_FRAMEBUFFER, m_Framebuffer);


	}

}