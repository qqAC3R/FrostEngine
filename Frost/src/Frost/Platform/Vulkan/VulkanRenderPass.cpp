#include "frostpch.h"
#include "VulkanRenderPass.h"

#include "VulkanContext.h"

namespace Frost
{

	namespace Utils
	{

		struct RenderPassAttachmentSpecs
		{
			VkFormat Format;
			VkAttachmentStoreOp StoreOp;
			VkImageLayout Layout;
		};

		static RenderPassAttachmentSpecs FBFormatToRenderPassSpecs(FramebufferTextureFormat format)
		{
			VkImageLayout colorImageLayout   = VK_IMAGE_LAYOUT_GENERAL;
			VkImageLayout depthImageLayout   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			VkImageLayout presentImageLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

			switch (format)
			{
				case Frost::FramebufferTextureFormat::RGBA8:	 return { VK_FORMAT_R8G8B8A8_SRGB,       VK_ATTACHMENT_STORE_OP_STORE, colorImageLayout };
				case Frost::FramebufferTextureFormat::RGBA16F:	 return { VK_FORMAT_R16G16B16A16_SFLOAT, VK_ATTACHMENT_STORE_OP_STORE, colorImageLayout };
				case Frost::FramebufferTextureFormat::DEPTH32:	 return { VK_FORMAT_D32_SFLOAT,			 VK_ATTACHMENT_STORE_OP_STORE, depthImageLayout };
			}

			FROST_ASSERT(false, "");
			return {};
		}

	}

	VulkanRenderPass::VulkanRenderPass(const FramebufferSpecification& framebufferSpecs)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		Vector<VkAttachmentDescription> attachmentDescriptions;
		Vector<VkAttachmentReference> colorAttachementRef{};
		Vector<VkAttachmentReference> depthAttachmentRef{};

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

		uint32_t colorAttachmentCount = 0;
		for (uint32_t i = 0; i < framebufferSpecs.Attachments.Attachments.size(); i++)
		{

			auto attachmentSpec = Utils::FBFormatToRenderPassSpecs(framebufferSpecs.Attachments.Attachments[i].TextureFormat);
	
			{
				VkAttachmentDescription attachment{};
				attachment.format = attachmentSpec.Format;
				attachment.samples = VK_SAMPLE_COUNT_1_BIT;
				attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
				attachment.storeOp = attachmentSpec.StoreOp;
				attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				
				attachment.initialLayout = attachmentSpec.Layout;
				attachment.finalLayout = attachmentSpec.Layout;
				
				attachmentDescriptions.push_back(attachment);

			}


			{
				VkAttachmentReference attachment{};
				attachment.attachment = i;
				
				switch (attachmentSpec.Layout)
				{
				case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
					attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; break;
				default:
					attachment.layout = attachmentSpec.Layout;
				}


				if (attachment.layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
				{
					depthAttachmentRef.push_back(attachment);
				}
				else
				{
					subpass.colorAttachmentCount = ++colorAttachmentCount;
					colorAttachementRef.push_back(attachment);
				}

			}
		}

		subpass.pColorAttachments = colorAttachementRef.data();
		subpass.pDepthStencilAttachment = depthAttachmentRef.data();


		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size());
		renderPassInfo.pAttachments = attachmentDescriptions.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &m_RenderPass) != VK_SUCCESS)
		{
			FROST_ASSERT(0, "Failed to create render pass!");
		}

		VulkanContext::SetStructDebugName("RenderPass", VK_OBJECT_TYPE_RENDER_PASS, m_RenderPass);
	}

	VulkanRenderPass::~VulkanRenderPass()
	{
	}

	void VulkanRenderPass::Bind()
	{
#if 0
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = m_RenderPass;
		renderPassInfo.framebuffer = ;
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = { extent.width, extent.height };

		std::array<VkClearValue, 2> vkClearValues;
		vkClearValues[0].color = { 0.0f, 0.0f, 0.0f, 0.0f };
		vkClearValues[1].depthStencil = { 1.0f, 0 };

		renderPassInfo.clearValueCount = static_cast<uint32_t>(vkClearValues.size());
		renderPassInfo.pClearValues = vkClearValues.data();

		vkCmdBeginRenderPass(cmdBuf, , VK_SUBPASS_CONTENTS_INLINE)
#endif
	}

	void VulkanRenderPass::Unbind()
	{

	}

	void VulkanRenderPass::Destroy()
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		vkDestroyRenderPass(device, m_RenderPass, nullptr);
	}

}