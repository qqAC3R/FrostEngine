#include "frostpch.h"
#include "VulkanRenderer.h"

#include "Frost/Core/Application.h"

#include "Frost/Renderer/Mesh.h"
#include "Frost/Renderer/Renderer.h"
#include "Frost/Renderer/Pipeline.h"
#include "Frost/Renderer/SceneRenderPass.h"
#include "Frost/Renderer/Buffers/UniformBuffer.h"

#include "Frost/Platform/Vulkan/VulkanImage.h"
#include "Frost/Platform/Vulkan/VulkanContext.h"
#include "Frost/Platform/Vulkan/VulkanMaterial.h"
#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanPostFXPass.h"
#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanComputePass.h"
#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanGeometryPass.h"
#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanCompositePass.h"
#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanRayTracingPass.h"
#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanDynamicSkyPass.h"

#include "Frost/Platform/Vulkan/VulkanBindlessAllocator.h"

#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>

namespace Frost
{
	namespace Vulkan
	{

		struct RenderData
		{
			/* The engine can support multiple renderpasses */
			Ref<SceneRenderPassPipeline> SceneRenderPasses;

			/* Camera related */
			glm::mat4 ProjectionMatrix;
			glm::mat4 ViewMatrix;

			Vector<VkFence> FencesInCheck;
			VkFence FencesInFlight[FRAMES_IN_FLIGHT];
			VkSemaphore AvailableSemapore[FRAMES_IN_FLIGHT], FinishedSemapore[FRAMES_IN_FLIGHT];

			Vector<VkDescriptorPool> DescriptorPools;
		};
	}

	static uint32_t s_ImageIndex = 0;
	static RenderQueue s_RenderQueue[FRAMES_IN_FLIGHT];
	static Vulkan::RenderData* s_Data;

	void VulkanRenderer::Init()
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		s_Data = new Vulkan::RenderData();

		// Initilization
		Application::Get().GetImGuiLayer()->OnInit(VulkanContext::GetSwapChain()->GetRenderPass());
		VulkanMaterial::AllocateDescriptorPool();
	
		// Creating the semaphores and fences
		for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			VkSemaphoreCreateInfo semaphoreInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
			FROST_VKCHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &s_Data->AvailableSemapore[i]));
			FROST_VKCHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &s_Data->FinishedSemapore[i]));

			VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
			fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
			FROST_VKCHECK(vkCreateFence(device, &fenceInfo, nullptr, &s_Data->FencesInFlight[i]));
			s_Data->FencesInCheck.push_back(VK_NULL_HANDLE);
		}

		// Creating 3 descriptor pools for allocating descriptor sets by the application
		s_Data->DescriptorPools.resize(Renderer::GetRendererConfig().FramesInFlight);
		for (auto& descriptorPool : s_Data->DescriptorPools)
		{
			VkDescriptorPoolSize pool_sizes[] =
			{
				{ VK_DESCRIPTOR_TYPE_SAMPLER, 100 },
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100 },
				{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 100 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 100 },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 100 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 100 },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100 },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 100 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 100 },
				{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 100 }
			};
			VkDescriptorPoolCreateInfo pool_info = {};
			pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
			pool_info.maxSets = 100 * (sizeof(pool_sizes) / sizeof(VkDescriptorPoolSize));
			pool_info.poolSizeCount = (uint32_t)(sizeof(pool_sizes) / sizeof(VkDescriptorPoolSize));
			pool_info.pPoolSizes = pool_sizes;
			FROST_VKCHECK(vkCreateDescriptorPool(device, &pool_info, nullptr, &descriptorPool));
		}
	}

	void VulkanRenderer::InitRenderPasses()
	{
		// Init scene render passes
		s_Data->SceneRenderPasses = Ref<SceneRenderPassPipeline>::Create();
		s_Data->SceneRenderPasses->AddRenderPass(Ref<VulkanDynamicSkyPass>::Create());
		s_Data->SceneRenderPasses->AddRenderPass(Ref<VulkanGeometryPass>::Create());
		s_Data->SceneRenderPasses->AddRenderPass(Ref<VulkanCompositePass>::Create());
		s_Data->SceneRenderPasses->AddRenderPass(Ref<VulkanPostFXPass>::Create());
		//s_Data->SceneRenderPasses->AddRenderPass(Ref<VulkanComputePass>::Create());
		//s_Data->SceneRenderPasses->AddRenderPass(Ref<VulkanRayTracingPass>::Create());

		// Late init for scene render passes
		s_Data->SceneRenderPasses->InitLateRenderPasses();
	}

	void VulkanRenderer::BeginFrame()
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		Renderer::Submit([&, currentFrameIndex]()
		{
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

			/* Acquire the image */
			VulkanContext::GetSwapChain()->BeginFrame(s_Data->AvailableSemapore[currentFrameIndex], &s_ImageIndex);

			/* Checking if the fence in flight is already being used to render */
			if (s_Data->FencesInCheck[currentFrameIndex] != VK_NULL_HANDLE)
				vkWaitForFences(device, 1, &s_Data->FencesInCheck[currentFrameIndex], VK_TRUE, UINT64_MAX);

			/* When the fence was finished being used by the renderer, we mark the fence as now being in use by this frame */
			s_Data->FencesInCheck[currentFrameIndex] = s_Data->FencesInFlight[currentFrameIndex];

			/* Reset the descriptor pool */
			FROST_VKCHECK(vkResetDescriptorPool(device, s_Data->DescriptorPools[currentFrameIndex], 0));

			/* Resetting the render queue that was used the previous `currentFrameIndex` frame,
			   because there may be chances of an mesh being deleted while it is being rendered  */
			s_RenderQueue[currentFrameIndex].Reset();
		});
	}

	void VulkanRenderer::EndFrame()
	{
		Renderer::Submit([&]()
		{
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
			uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
			VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
			VkFence fenceInFlight = s_Data->FencesInFlight[currentFrameIndex];
			VkSemaphore finishedSemaphore = s_Data->FinishedSemapore[currentFrameIndex];
			VkSemaphore availableSemaphore = s_Data->AvailableSemapore[currentFrameIndex];



			/* Begin recording the frame's commandbuffer */
			VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
			FROST_VKCHECK(vkBeginCommandBuffer(cmdBuf, &beginInfo));


			/* Updating all the graphics passes */
			s_Data->SceneRenderPasses->UpdateRenderPasses(s_RenderQueue[currentFrameIndex]);


			/* Rendering a quad for the swap chain image */
			VulkanContext::GetSwapChain()->BeginRenderPass();
			Application::Get().GetImGuiLayer()->Render();
			vkCmdEndRenderPass(cmdBuf);


			FROST_VKCHECK(vkEndCommandBuffer(cmdBuf));
			

			VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
			VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
			submitInfo.pWaitDstStageMask = waitStages;
			submitInfo.waitSemaphoreCount = 1;
			submitInfo.pWaitSemaphores = &availableSemaphore;
			submitInfo.signalSemaphoreCount = 1;
			submitInfo.pSignalSemaphores = &finishedSemaphore;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &cmdBuf;
			vkResetFences(device, 1, &fenceInFlight);

			VkQueue graphicsQueue = VulkanContext::GetCurrentDevice()->GetQueueFamilies().GraphicsFamily.Queue;
			FROST_VKCHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, fenceInFlight));


			VulkanRenderer::Render();
		});
	}

	void VulkanRenderer::BeginScene(const EditorCamera& camera)
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		Renderer::Submit([&, camera, currentFrameIndex]()
		{
			s_Data->ViewMatrix = camera.GetViewMatrix();
			s_Data->ProjectionMatrix = camera.GetProjectionMatrix();
			s_RenderQueue[currentFrameIndex].SetCamera(camera);
		});
	}

	void VulkanRenderer::EndScene()
	{
	}

	void VulkanRenderer::Submit(const Ref<Mesh>& mesh, const glm::mat4& transform)
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		Renderer::Submit([&, mesh, transform, currentFrameIndex]()
		{
			s_RenderQueue[currentFrameIndex].Add(mesh, transform);
		});
	}

	void VulkanRenderer::Submit(const Ref<Mesh>& mesh, Ref<Material> material, const glm::mat4& transform)
	{
		// TODO: Materials dont acutally work
		//s_RenderQueue.Add(mesh, material, transform);
	}

	void VulkanRenderer::Submit(const PointLightComponent& pointLight, const glm::vec3& position)
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		Renderer::Submit([&, pointLight, position, currentFrameIndex]()
		{
			s_RenderQueue[currentFrameIndex].AddPointLight(pointLight, position);
		});
	}

	void VulkanRenderer::Resize(uint32_t width, uint32_t height)
	{
		Renderer::Submit([width, height]()
		{
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
			vkDeviceWaitIdle(device);

			s_Data->SceneRenderPasses->ResizeRenderPasses(width, height);
		});
	}

	void VulkanRenderer::ShutDown()
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		s_Data->SceneRenderPasses->ShutDown();

		for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			vkDestroySemaphore(device, s_Data->AvailableSemapore[i], nullptr);
			vkDestroySemaphore(device, s_Data->FinishedSemapore[i], nullptr);
			vkDestroyFence(device, s_Data->FencesInFlight[i], nullptr);
			vkDestroyDescriptorPool(device, s_Data->DescriptorPools[i], nullptr);

			// Clear all the submitted data (from every frame remaning)
			s_RenderQueue[i].Reset();
		}
		VulkanMaterial::DeallocateDescriptorPool();
	}

	void VulkanRenderer::Render()
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VulkanContext::GetSwapChain()->Present(s_Data->FinishedSemapore[currentFrameIndex], s_ImageIndex);
	}

	Ref<Image2D> VulkanRenderer::GetFinalImage(uint32_t id) const
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		if(id == 0)
			return s_Data->SceneRenderPasses->GetRenderPassData<VulkanRayTracingPass>()->DisplayTexture[currentFrameIndex];
		
		return s_Data->SceneRenderPasses->GetRenderPassData<VulkanPostFXPass>()->FinalTexture[currentFrameIndex];
		//return s_Data->SceneRenderPasses->GetRenderPassData<VulkanPostFXPass>()->AO_Image[currentFrameIndex];
		//return s_Data->SceneRenderPasses->GetRenderPassData<VulkanCompositePass>()->RenderPass->GetColorAttachment(0, currentFrameIndex);
		//return s_Data->SceneRenderPasses->GetRenderPassData<VulkanGeometryPass>()->RenderPass->GetColorAttachment(0, currentFrameIndex);
	}

	VkDescriptorSet VulkanRenderer::AllocateDescriptorSet(VkDescriptorSetAllocateInfo allocInfo)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkDescriptorSet descriptorSet;
		allocInfo.descriptorPool = s_Data->DescriptorPools[currentFrameIndex];
		FROST_VKCHECK(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));
		return descriptorSet;
	}

	static VkAccessFlags AccessFlagsToImageLayout(VkImageLayout layout)
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

	static VkAccessFlags2KHR AccessFlagsToImageLayout2(VkImageLayout layout)
	{
		switch (layout)
		{
		case VK_IMAGE_LAYOUT_PREINITIALIZED:				   return VK_ACCESS_2_HOST_READ_BIT_KHR;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:			   return VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR;
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:			   return VK_ACCESS_2_TRANSFER_READ_BIT_KHR;
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:		   return VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: return VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT_KHR;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:		   return VK_ACCESS_2_SHADER_READ_BIT_KHR;
		}
		return VkAccessFlags2KHR();
	}

	void Utils::InsertImageMemoryBarrier(VkCommandBuffer cmdbuffer,
										 VkImage image,
										 VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
										 VkImageLayout oldImageLayout, VkImageLayout newImageLayout,
										 VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask,
										 VkImageSubresourceRange subresourceRange)
	{
		VkImageMemoryBarrier imageMemoryBarrier{};
		imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

		imageMemoryBarrier.srcAccessMask = srcAccessMask;
		imageMemoryBarrier.dstAccessMask = dstAccessMask;
		imageMemoryBarrier.oldLayout = oldImageLayout;
		imageMemoryBarrier.newLayout = newImageLayout;
		imageMemoryBarrier.image = image;
		imageMemoryBarrier.subresourceRange = subresourceRange;

		vkCmdPipelineBarrier(
			cmdbuffer,
			srcStageMask,
			dstStageMask,
			0,
			0, nullptr,
			0, nullptr,
			1, &imageMemoryBarrier
		);
	}

	void Utils::SetImageLayout(VkCommandBuffer cmdbuffer,
							   VkImage image,
							   VkImageLayout oldImageLayout, VkImageLayout newImageLayout,
							   VkImageSubresourceRange subresourceRange,
							   VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask)
	{
		VkImageMemoryBarrier imageMemoryBarrier = {};
		imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageMemoryBarrier.oldLayout = oldImageLayout;
		imageMemoryBarrier.newLayout = newImageLayout;
		imageMemoryBarrier.image = image;
		imageMemoryBarrier.subresourceRange = subresourceRange;

		imageMemoryBarrier.srcAccessMask = AccessFlagsToImageLayout(oldImageLayout);
		imageMemoryBarrier.dstAccessMask = AccessFlagsToImageLayout(newImageLayout);


		// Put barrier inside setup command buffer
		vkCmdPipelineBarrier(
			cmdbuffer,
			srcStageMask,
			dstStageMask,
			0,
			0, nullptr,
			0, nullptr,
			1, &imageMemoryBarrier
		);


#if 0
		VkImageMemoryBarrier2KHR imageMemoryBarrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageMemoryBarrier.oldLayout = oldImageLayout;
		imageMemoryBarrier.newLayout = newImageLayout;
		imageMemoryBarrier.image = image;
		imageMemoryBarrier.subresourceRange = subresourceRange;
		imageMemoryBarrier.srcAccessMask = AccessFlagsToImageLayout2(oldImageLayout);
		imageMemoryBarrier.dstAccessMask = AccessFlagsToImageLayout2(newImageLayout);


		VkDependencyInfoKHR depencyInfo{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR };
		depencyInfo.pNext = nullptr;
		depencyInfo.dependencyFlags = 0;

		depencyInfo.bufferMemoryBarrierCount = 0;
		depencyInfo.pBufferMemoryBarriers = nullptr;
		depencyInfo.memoryBarrierCount = 0;
		depencyInfo.pMemoryBarriers = nullptr;

		depencyInfo.imageMemoryBarrierCount = 1;
		depencyInfo.pImageMemoryBarriers = &imageMemoryBarrier;

		vkCmdPipelineBarrier2KHR(cmdbuffer, &depencyInfo);
#endif
	}

	void Utils::SetImageLayout(VkCommandBuffer cmdbuffer,
							   VkImage image, VkImageAspectFlags aspectMask,
							   VkImageLayout oldImageLayout, VkImageLayout newImageLayout,
							   VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask)
	{
		VkImageSubresourceRange subresourceRange{};
		subresourceRange.aspectMask = aspectMask;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = 1;
		subresourceRange.layerCount = 1;
		SetImageLayout(cmdbuffer, image, oldImageLayout, newImageLayout, subresourceRange, srcStageMask, dstStageMask);
	}

}