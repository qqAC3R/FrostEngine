#include "frostpch.h"
#include "VulkanFramebuffer.h"

#include "Frost/Platform/Vulkan/VulkanContext.h"
#include "Frost/Platform/Vulkan/VulkanRenderPass.h"
#include "Frost/Platform/Vulkan/VulkanTexture.h"
#include "Frost/Platform/Vulkan/VulkanImage.h"

namespace Frost
{

	namespace Utils
	{
#if 0
		// TODO: Delete this, it is useless
		std::pair<VkImageUsageFlags, VkFormat> FBFormatToVK(FramebufferTextureFormat format)
		{
			switch (format)
			{
			case FramebufferTextureFormat::R8:
				return std::make_pair(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_FORMAT_R8_UNORM);

			case FramebufferTextureFormat::R32F:
				return std::make_pair(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_FORMAT_R32_SINT);
			case FramebufferTextureFormat::R32I:
				return std::make_pair(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_FORMAT_R32_UINT);

			case FramebufferTextureFormat::RGBA8:
				return std::make_pair(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_FORMAT_R8G8B8A8_UNORM);

			case FramebufferTextureFormat::RGBA16F:
				return std::make_pair(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_FORMAT_R16G16B16A16_SFLOAT);

			case FramebufferTextureFormat::DEPTH32:
				return std::make_pair(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_FORMAT_D32_SFLOAT);

			case FramebufferTextureFormat::DEPTH24STENCIL8:
				return std::make_pair(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_FORMAT_D24_UNORM_S8_UINT);
			}

			FROST_ASSERT_MSG("FramebufferTextureFormat is invalid!")
				return {};
		}
#endif

		bool IsDepthFormat(FramebufferTextureFormat format)
		{
			switch (format)
			{
				case FramebufferTextureFormat::DEPTH24STENCIL8:
				case FramebufferTextureFormat::DEPTH32:
					return true;
			}
			return false;
		}

		ImageFormat GetImageFormat(FramebufferTextureFormat framebufferFormat)
		{
			switch (framebufferFormat)
			{
				case FramebufferTextureFormat::R8:				 return ImageFormat::R8;
				case FramebufferTextureFormat::R32F:			 return ImageFormat::R32F;
				case FramebufferTextureFormat::R16F:			 return ImageFormat::R16F;
				case FramebufferTextureFormat::R32I:			 return ImageFormat::R32I;
				case FramebufferTextureFormat::RG16F:			 return ImageFormat::RG16F;
				case FramebufferTextureFormat::RG32F:			 return ImageFormat::RG32F;
				case FramebufferTextureFormat::RGBA8:            return ImageFormat::RGBA8;
				case FramebufferTextureFormat::RGBA16F:          return ImageFormat::RGBA16F;
				case FramebufferTextureFormat::RGBA32F:          return ImageFormat::RGBA32F;
				case FramebufferTextureFormat::DEPTH32:          return ImageFormat::Depth32;
				case FramebufferTextureFormat::DEPTH24STENCIL8:  return ImageFormat::Depth24Stencil8;
				case FramebufferTextureFormat::None:             FROST_ASSERT_MSG("FramebufferTextureFormat::None is invalid!");
			}
			return ImageFormat::None;
		}

	}

	VulkanFramebuffer::VulkanFramebuffer(const FramebufferSpecification& spec)
		: m_Specification(spec)
	{
		FROST_ASSERT(m_Specification.Attachments.Attachments.size() != 0, "Framebuffer should have atleast 1 attachment");
		CreateAttachments();
	}

	VulkanFramebuffer::~VulkanFramebuffer()
	{
		Destroy();
	}

	void VulkanFramebuffer::Resize(uint32_t width, uint32_t height)
	{
	}

	void VulkanFramebuffer::Destroy()
	{
		if (m_Framebuffer == VK_NULL_HANDLE) return;

		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		vkDestroyFramebuffer(device, m_Framebuffer, nullptr);
		m_Framebuffer = VK_NULL_HANDLE;
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

			ImageSpecification spec{};
			spec.Width = m_Specification.Width;
			spec.Height = m_Specification.Height;
			spec.Format = Utils::GetImageFormat(attachment.TextureFormat);
			spec.Sampler.SamplerFilter = ImageFilter::Nearest;
			spec.Sampler.SamplerWrap = ImageWrap::ClampToEdge;
			spec.UseMipChain = false;

			if (attachment.TextureFormat == FramebufferTextureFormat::Depth || attachment.TextureFormat == FramebufferTextureFormat::DepthStencil)
			{
				spec.Usage = ImageUsage::DepthStencil;
			}
			else
				spec.Usage = ImageUsage::Storage;

			Ref<Image2D> texture = Image2D::Create(spec);
			m_Attachments.push_back(texture);
		}

	}

	void VulkanFramebuffer::CreateFramebuffer(VkRenderPass renderPass)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		Vector<VkImageView> attachments;
		for (auto& attachment : m_Attachments)
		{
			attachments.push_back(attachment.As<VulkanImage2D>()->GetVulkanImageView());
		}

		VkFramebufferCreateInfo framebufferInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = m_Specification.Width;
		framebufferInfo.height = m_Specification.Height;
		framebufferInfo.layers = 1;

		FROST_VKCHECK(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &m_Framebuffer));
		VulkanContext::SetStructDebugName("Framebuffer", VK_OBJECT_TYPE_FRAMEBUFFER, m_Framebuffer);
	}

}