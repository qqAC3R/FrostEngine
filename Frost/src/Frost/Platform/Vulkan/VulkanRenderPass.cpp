#include "frostpch.h"
#include "VulkanRenderPass.h"

#include "Frost/Platform/Vulkan/VulkanContext.h"
#include "Frost/Platform/Vulkan/VulkanFramebuffer.h"
#include "Frost/Platform/Vulkan/VulkanTexture.h"
#include "Frost/Platform/Vulkan/VulkanImage.h"

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
			VkImageLayout colorImageLayout   = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			VkImageLayout depthImageLayout   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			VkImageLayout presentImageLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

			switch (format)
			{
			case FramebufferTextureFormat::RGBA8:	 return { VK_FORMAT_R8G8B8A8_SRGB,       VK_ATTACHMENT_STORE_OP_STORE, colorImageLayout };
			case FramebufferTextureFormat::RGBA16F:	 return { VK_FORMAT_R16G16B16A16_SFLOAT, VK_ATTACHMENT_STORE_OP_STORE, colorImageLayout };
			case FramebufferTextureFormat::DEPTH32:	 return { VK_FORMAT_D32_SFLOAT,			 VK_ATTACHMENT_STORE_OP_STORE, depthImageLayout };
			}

			FROST_ASSERT(false, "");
			return {};
		}

		VkAttachmentStoreOp GetVulkanAttachmentStoreOp(OperationStore attachmentOp)
		{
			switch (attachmentOp)
			{
				case OperationStore::Store:     return VK_ATTACHMENT_STORE_OP_STORE;
				case OperationStore::DontCare:  return VK_ATTACHMENT_STORE_OP_DONT_CARE;
			}

			FROST_ASSERT(false, "");
			return VkAttachmentStoreOp();
		}

		VkAttachmentLoadOp GetVulkanAttachmentLoadOp(OperationLoad attachmentOp)
		{
			switch (attachmentOp)
			{
				case OperationLoad::Clear:     return VK_ATTACHMENT_LOAD_OP_CLEAR;
				case OperationLoad::Load:      return VK_ATTACHMENT_LOAD_OP_LOAD;
				case OperationLoad::DontCare:  return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			}

			FROST_ASSERT(false, "");
			return VkAttachmentLoadOp();
		}

		VkFormat GetVulkanFBFormat(FramebufferTextureFormat format)
		{
			switch (format)
			{
				case FramebufferTextureFormat::RGBA8:	 return VK_FORMAT_R8G8B8A8_SRGB;
				case FramebufferTextureFormat::RGBA16F:	 return VK_FORMAT_R16G16B16A16_SFLOAT;
				case FramebufferTextureFormat::DEPTH32:  return VK_FORMAT_D32_SFLOAT;
			}

			FROST_ASSERT(false, "");
			return VkFormat();
		}
	}

	VulkanRenderPass::VulkanRenderPass(const RenderPassSpecification& renderPassSpecs)
		: m_Specification(renderPassSpecs)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		m_Framebuffers.resize(renderPassSpecs.FramebufferCount);
		for (auto& framebuffer : m_Framebuffers)
			framebuffer = Framebuffer::Create(renderPassSpecs.FramebufferSpecification);


		Vector<VkAttachmentDescription> attachmentDescriptions;
		Vector<VkAttachmentReference> colorAttachmentRefs;
		Vector<VkAttachmentReference> depthAttachmentRefs;

		//uint32_t colorAttachmentCount = 0; uint32_t depthAttachmentCount = 0;
		uint32_t attachmentCount = 0;
		for(uint32_t i = 0; i < renderPassSpecs.RenderPassSpec.size(); i++)
		{
			Vector<FramebufferTextureSpecification> framebufferAttachmentInfo = renderPassSpecs.FramebufferSpecification.Attachments.Attachments;
			Ref<VulkanImage2D> attachmentImage = m_Framebuffers[0]->GetColorAttachment(i).As<VulkanImage2D>();

			VkAttachmentDescription& attachment = attachmentDescriptions.emplace_back();
			attachment.format = Utils::GetVulkanFBFormat(framebufferAttachmentInfo[i].TextureFormat);
			attachment.samples = VK_SAMPLE_COUNT_1_BIT;
			attachment.loadOp = Utils::GetVulkanAttachmentLoadOp(renderPassSpecs.RenderPassSpec[i].LoadOperation);
			attachment.storeOp = Utils::GetVulkanAttachmentStoreOp(renderPassSpecs.RenderPassSpec[i].StoreOperation);
			attachment.stencilLoadOp = Utils::GetVulkanAttachmentLoadOp(renderPassSpecs.RenderPassSpec[i].DepthLoadOperation);;
			attachment.stencilStoreOp = Utils::GetVulkanAttachmentStoreOp(renderPassSpecs.RenderPassSpec[i].DepthStoreOperation);;
			attachment.initialLayout = attachmentImage->GetVulkanImageLayout();
			attachment.finalLayout = Utils::GetImageLayout(renderPassSpecs.RenderPassSpec[i].FinalLayout);
			
			VkAttachmentReference attachmentRef;
			attachmentRef.layout = m_Framebuffers[0]->GetColorAttachment(i).As<VulkanImage2D>()->GetVulkanImageLayout();
			attachmentRef.attachment = attachmentCount;
			attachmentCount++;

			if (framebufferAttachmentInfo[i].TextureFormat == FramebufferTextureFormat::Depth)
				depthAttachmentRefs.push_back(attachmentRef);
			else
				colorAttachmentRefs.push_back(attachmentRef);

			m_AttachmentLayouts[i] = attachment.finalLayout;


			VkClearValue clearValue{};
			switch (renderPassSpecs.RenderPassSpec[i].FramebufferAttachmentSpec.TextureFormat)
			{
			case FramebufferTextureFormat::RGBA8:
			case FramebufferTextureFormat::RGBA16F: clearValue.color =        { 0.0f, 0.0f, 0.0f, 0.0f}; break;
			case FramebufferTextureFormat::Depth:   clearValue.depthStencil = { 1.0f, 0 };               break;
			}
			m_ClearValues.push_back(clearValue);
		}

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.pColorAttachments = colorAttachmentRefs.data();
		subpass.colorAttachmentCount = colorAttachmentRefs.size();
		subpass.pDepthStencilAttachment = depthAttachmentRefs.data();

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

		FROST_VKCHECK(vkCreateRenderPass(device, &renderPassInfo, nullptr, &m_RenderPass));
		VulkanContext::SetStructDebugName("RenderPass", VK_OBJECT_TYPE_RENDER_PASS, m_RenderPass);

		// Creating the framebuffer (after creating the renderpass)
		for (auto& framebuffer : m_Framebuffers)
		{
			Ref<VulkanFramebuffer> vulkanFramebuffer = framebuffer.As<VulkanFramebuffer>();
			vulkanFramebuffer->CreateFramebuffer(m_RenderPass);
		}
	}

	VulkanRenderPass::~VulkanRenderPass()
	{
	}

	void VulkanRenderPass::Bind()
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		Ref<Framebuffer> framebuffer = m_Framebuffers.size() > 1 ? m_Framebuffers[currentFrameIndex] : m_Framebuffers[0];
		VkFramebuffer framebufferHandle = (VkFramebuffer)framebuffer->GetFramebufferHandle();

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = m_RenderPass;
		renderPassInfo.framebuffer = framebufferHandle;
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = { m_Specification.FramebufferSpecification.Width, m_Specification.FramebufferSpecification.Height };


		renderPassInfo.clearValueCount = static_cast<uint32_t>(m_ClearValues.size());
		renderPassInfo.pClearValues = m_ClearValues.data();

		vkCmdBeginRenderPass(cmdBuf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		// TODO: Change Image2D Layout Here
		// TODO: Change Image2D Layout Here
		// TODO: Change Image2D Layout Here
		// TODO: Change Image2D Layout Here
	}

	void VulkanRenderPass::Unbind()
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		vkCmdEndRenderPass(cmdBuf);

		Ref<Framebuffer> framebuffer = m_Framebuffers.size() > 1 ? m_Framebuffers[currentFrameIndex] : m_Framebuffers[0];
		auto attachments = framebuffer->GetColorAttachments();
		for (uint32_t i = 0; i < attachments.size(); i++)
		{
			Ref<VulkanImage2D> attachment = attachments[i].As<VulkanImage2D>();
			attachment->m_ImageLayout = m_AttachmentLayouts[i];
		}
	}

	void VulkanRenderPass::Destroy()
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		for (auto& framebuffer : m_Framebuffers)
			framebuffer->Destroy();
		vkDestroyRenderPass(device, m_RenderPass, nullptr);
	}

}