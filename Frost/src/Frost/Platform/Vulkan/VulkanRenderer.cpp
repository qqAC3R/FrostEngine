#include "frostpch.h"
#include "VulkanRenderer.h"

#include "Frost/Core/Application.h"

#include "Frost/ImGui/ImGuiVulkanLayer.h"

#include "Frost/Renderer/Mesh.h"
#include "Frost/Renderer/Renderer.h"
#include "Frost/Renderer/Pipeline.h"
#include "Frost/Renderer/SceneRenderPass.h"
#include "Frost/Renderer/Buffers/UniformBuffer.h"

#include "Frost/Platform/Vulkan/VulkanImage.h"
#include "Frost/Platform/Vulkan/VulkanContext.h"
#include "Frost/Platform/Vulkan/VulkanMaterial.h"

// Render Passes
#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanPostFXPass.h"
#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanComputePass.h"
#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanGeometryPass.h"
#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanCompositePass.h"
#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanRayTracingPass.h"
#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanVoxelizationPass.h"
#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanShadowPass.h"
#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanVolumetricPass.h"
#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanBatchRenderingPass.h"

#include "Frost/Platform/Vulkan/VulkanBindlessAllocator.h"

namespace Frost
{
	namespace Vulkan
	{

		struct RenderData
		{
			/* The engine can support multiple renderpasses */
			Ref<SceneRenderPassPipeline> SceneRenderPasses;
			Ref<RendererDebugger> s_RendererDebugger;

			Vector<VkFence> FencesInCheck;
			VkFence FencesInFlight[FRAMES_IN_FLIGHT];
			VkSemaphore AvailableSemapore[FRAMES_IN_FLIGHT], FinishedSemapore[FRAMES_IN_FLIGHT];

			Vector<VkDescriptorPool> DescriptorPools;
		};

		struct RenderDebugData
		{
			Vector<VkQueryPool> TimestampQueryPools;
			uint32_t QueryCount = 0;
			Vector<Vector<uint64_t>> TimestampQueryResults;
			Vector<Vector<float>> ExecutionTimes;
			uint64_t NextAvailableQueryID = 2;

			float Device_TimestampPeriod;
		};
	}

	static uint32_t s_ImageIndex = 0;
	static uint64_t s_FrameCount = 0;
	static RenderQueue s_RenderQueue[FRAMES_IN_FLIGHT];
	static Vulkan::RenderData* s_Data;
	static Vulkan::RenderDebugData* s_DebugData;

	void VulkanRenderer::Init()
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		VkPhysicalDevice physicalDevice = VulkanContext::GetCurrentDevice()->GetPhysicalDevice();
		s_Data = new Vulkan::RenderData();
		s_DebugData = new Vulkan::RenderDebugData();

		// Initilization
		ImGuiLayer* imguiLayer = Application::Get().GetImGuiLayer();
		VulkanImGuiLayer* vulkanImGuiLayer = static_cast<VulkanImGuiLayer*>(imguiLayer);
		vulkanImGuiLayer->OnInit(VulkanContext::GetSwapChain()->GetRenderPass());

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


		const uint32_t maxUserQueries = 40;
		s_DebugData->QueryCount = 2 + 2 * maxUserQueries;

		s_DebugData->TimestampQueryPools.resize(Renderer::GetRendererConfig().FramesInFlight);
		for(auto& queryPool : s_DebugData->TimestampQueryPools)
		{
			VkQueryPoolCreateInfo queryPoolInfo{ VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
			queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
			queryPoolInfo.queryCount = s_DebugData->QueryCount;
			vkCreateQueryPool(device, &queryPoolInfo, nullptr, &queryPool);
			vkResetQueryPool(device, queryPool, 0, s_DebugData->QueryCount);
		}

		s_DebugData->TimestampQueryResults.resize(Renderer::GetRendererConfig().FramesInFlight);
		for (auto& timestampQueryResults : s_DebugData->TimestampQueryResults)
		{
			timestampQueryResults.resize(s_DebugData->QueryCount);
		}
		s_DebugData->ExecutionTimes.resize(Renderer::GetRendererConfig().FramesInFlight);
		for (auto& executionTimes : s_DebugData->ExecutionTimes)
		{
			executionTimes.resize(s_DebugData->QueryCount / 2);
		}
		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(physicalDevice, &properties);
		s_DebugData->Device_TimestampPeriod = properties.limits.timestampPeriod;
	}

	void VulkanRenderer::InitRenderPasses()
	{
		// Add the white texture as the default texture for the bindless residency
		VulkanBindlessAllocator::AddTextureCustomSlot(
			Renderer::GetWhiteLUT(),
			BindlessAllocator::GetWhiteTextureID()
		);

		/// Init scene render passes
		s_Data->SceneRenderPasses = Ref<SceneRenderPassPipeline>::Create();

		// Render all the neccesary data into a G-Buffer
		s_Data->SceneRenderPasses->AddRenderPass(Ref<VulkanGeometryPass>::Create());

		// Render the cascaded shadow maps and compute a shadow texture
		s_Data->SceneRenderPasses->AddRenderPass(Ref<VulkanShadowPass>::Create());

		// Voxelize the scene into a 3D texture and then compute Voxel GI
		s_Data->SceneRenderPasses->AddRenderPass(Ref<VulkanVoxelizationPass>::Create());

		// Put everything together to get a PBR looking image
		s_Data->SceneRenderPasses->AddRenderPass(Ref<VulkanCompositePass>::Create());

		// Compute volumetrics (Volumetrics on Froxels and Clouds)
		s_Data->SceneRenderPasses->AddRenderPass(Ref<VulkanVolumetricPass>::Create());

		// Add Post Processing effects to the final image (Bloom, SSR, AO, Color correction)
		s_Data->SceneRenderPasses->AddRenderPass(Ref<VulkanPostFXPass>::Create());

		// Batch renderer (For rendering quads/circles/lines/billboards)
		s_Data->SceneRenderPasses->AddRenderPass(Ref<VulkanBatchRenderingPass>::Create());
		
#if 0
		// Depracated
		s_Data->SceneRenderPasses->AddRenderPass(Ref<VulkanComputePass>::Create());
		s_Data->SceneRenderPasses->AddRenderPass(Ref<VulkanRayTracingPass>::Create());
#endif

		/// Late init for scene render passes
		s_Data->SceneRenderPasses->InitLateRenderPasses();

		/// Create the renderer debugger
		s_Data->s_RendererDebugger = RendererDebugger::Create();
		s_Data->s_RendererDebugger->Init(s_Data->SceneRenderPasses.Raw());
	}

	void VulkanRenderer::BeginFrame()
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		Renderer::Submit([&, currentFrameIndex]()
		{
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

			Application::Get().GetWindow().GetGraphicsContext()->WaitDevice();

			/* Checking if the fence in flight is already being used to render */
			if (s_Data->FencesInCheck[currentFrameIndex] != VK_NULL_HANDLE)
				vkWaitForFences(device, 1, &s_Data->FencesInCheck[currentFrameIndex], VK_TRUE, UINT64_MAX);


			/* Acquire the image */
			VulkanContext::GetSwapChain()->BeginFrame(s_Data->AvailableSemapore[currentFrameIndex], &s_ImageIndex);

			
			/* When the fence was finished being used by the renderer, we mark the fence as now being in use by this frame */
			s_Data->FencesInCheck[currentFrameIndex] = s_Data->FencesInFlight[currentFrameIndex];

			/* Reset the descriptor pool */
			FROST_VKCHECK(vkResetDescriptorPool(device, s_Data->DescriptorPools[currentFrameIndex], 0));

			/* Resetting the render queue that was used the previous `currentFrameIndex` frame,
			   because there may be chances of an mesh being deleted while it is being rendered  */
			s_RenderQueue[currentFrameIndex].Reset();

			GetTimestampResults(currentFrameIndex);

			s_DebugData->NextAvailableQueryID = 2;
		});
		s_FrameCount++;
		if (s_FrameCount == ~0) s_FrameCount = 0;
	}

	void VulkanRenderer::EndFrame()
	{
		Renderer::Submit([&]()
		{
			VulkanRenderer::Render();
		});
#if 0
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

			// Timestamp query
			vkCmdResetQueryPool(cmdBuf, s_DebugData->TimestampQueryPools[currentFrameIndex], 0, s_DebugData->QueryCount);
			vkCmdWriteTimestamp(cmdBuf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, s_DebugData->TimestampQueryPools[currentFrameIndex], 0);


			/* Update the sky */
			Renderer::GetSceneEnvironment()->UpdateAtmosphere(s_RenderQueue[currentFrameIndex]);

			/* Updating all the graphics passes */
			s_Data->SceneRenderPasses->UpdateRenderPasses(s_RenderQueue[currentFrameIndex]);


			/* Rendering a quad for the swap chain image */
			VulkanContext::GetSwapChain()->BeginRenderPass();
			Application::Get().GetImGuiLayer()->Render();
			vkCmdEndRenderPass(cmdBuf);

			// Stop the Timestamp query
			vkCmdWriteTimestamp(cmdBuf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, s_DebugData->TimestampQueryPools[currentFrameIndex], 1);

			// End the render command buffer
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
#endif
	}

	void VulkanRenderer::SubmitCmdsToRender()
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

			// Timestamp query
			vkCmdResetQueryPool(cmdBuf, s_DebugData->TimestampQueryPools[currentFrameIndex], 0, s_DebugData->QueryCount);
			vkCmdWriteTimestamp(cmdBuf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, s_DebugData->TimestampQueryPools[currentFrameIndex], 0);


			/* Update the sky */
			Renderer::GetSceneEnvironment()->UpdateAtmosphere(s_RenderQueue[currentFrameIndex]);

			/* Updating all the graphics passes */
			s_Data->SceneRenderPasses->UpdateRenderPasses(s_RenderQueue[currentFrameIndex]);


			/* Rendering a quad for the swap chain image */
			VulkanContext::GetSwapChain()->BeginRenderPass();
			Application::Get().GetImGuiLayer()->Render();
			vkCmdEndRenderPass(cmdBuf);

			// Stop the Timestamp query
			vkCmdWriteTimestamp(cmdBuf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, s_DebugData->TimestampQueryPools[currentFrameIndex], 1);

			// End the render command buffer
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
		});
	}

	void VulkanRenderer::BeginScene(Ref<Scene> scene, Ref<EditorCamera>& camera)
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		Renderer::Submit([&, scene, camera, currentFrameIndex]()
		{
			s_RenderQueue[currentFrameIndex].SetCamera(camera);
			s_RenderQueue[currentFrameIndex].SetActiveScene(scene);
		});
	}

	void VulkanRenderer::BeginScene(Ref<Scene> scene, Ref<RuntimeCamera>& camera)
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		Renderer::Submit([&, scene, camera, currentFrameIndex]()
		{
			s_RenderQueue[currentFrameIndex].SetCamera(camera);
			s_RenderQueue[currentFrameIndex].SetActiveScene(scene);
		});
	}

	void VulkanRenderer::EndScene()
	{
	}

	void VulkanRenderer::SubmitBillboards(const glm::vec3& positon, const glm::vec2& size, const glm::vec4& color)
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		Renderer::Submit([&, currentFrameIndex, positon, color, size]()
		{
			s_RenderQueue[currentFrameIndex].AddBillBoard(positon, size, color);
		});
	}

	void VulkanRenderer::SubmitBillboards(const glm::vec3& positon, const glm::vec2& size, Ref<Texture2D> texture)
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		Renderer::Submit([&, currentFrameIndex, positon, texture, size]()
		{
			s_RenderQueue[currentFrameIndex].AddBillBoard(positon, size, texture);
		});
	}

	void VulkanRenderer::SubmitLines(const glm::vec3& positon0, const glm::vec3& positon1, const glm::vec4& color)
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		Renderer::Submit([&, currentFrameIndex, positon0, positon1, color]()
		{
			s_RenderQueue[currentFrameIndex].AddLines(positon0, positon1, color);
		});
	}

	void VulkanRenderer::SubmitText(const std::string& string, const Ref<Font>& font, const glm::mat4& transform, float maxWidth, float lineHeightOffset, float kerningOffset, const glm::vec4& color)
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		Renderer::Submit([&, currentFrameIndex, string, font, transform, maxWidth, lineHeightOffset, kerningOffset, color]()
		{
			s_RenderQueue[currentFrameIndex].AddTextRender(string, font, transform, maxWidth, lineHeightOffset, kerningOffset, color);
		});
	}

	void VulkanRenderer::SubmitWireframeMesh(Ref<Mesh> mesh, const glm::mat4& transform, const glm::vec4& color, float lineWidth)
	{
		// TODO: Add Renderer Submittion back!! (only tested for mesh AABB)
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		Renderer::Submit([&, currentFrameIndex, mesh, transform, color, lineWidth]()
		{
			s_RenderQueue[currentFrameIndex].AddWireframeMesh(mesh, transform, color, lineWidth);
		});
	}

	uint32_t VulkanRenderer::ReadPixelFromFramebufferEntityID(uint32_t x, uint32_t y)
	{
		return s_Data->SceneRenderPasses->GetRenderPass<VulkanBatchRenderingPass>()->ReadPixelFromTextureEntityID(x, y);
	}

	uint32_t VulkanRenderer::GetCurrentFrameIndex()
	{
		return VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
	}

	uint64_t VulkanRenderer::GetFrameCount()
	{
		return s_FrameCount;
	}

	Ref<Scene> VulkanRenderer::GetActiveScene()
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		return s_RenderQueue[currentFrameIndex].m_ActiveScene;
	}

	void VulkanRenderer::Submit(const Ref<Mesh>& mesh, const glm::mat4& transform, uint32_t entityID)
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		Renderer::Submit([&, mesh, transform, entityID, currentFrameIndex]()
		{
			s_RenderQueue[currentFrameIndex].Add(mesh, transform, entityID);
		});
	}

	void VulkanRenderer::Submit(const PointLightComponent& pointLight, const glm::vec3& position)
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		Renderer::Submit([&, pointLight, position, currentFrameIndex]()
		{
			s_RenderQueue[currentFrameIndex].AddPointLight(pointLight, position);
		});
	}

	void VulkanRenderer::Submit(const DirectionalLightComponent& directionalLight, const glm::vec3& direction)
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		Renderer::Submit([&, directionalLight, direction, currentFrameIndex]()
		{
			s_RenderQueue[currentFrameIndex].SetDirectionalLight(directionalLight, direction);
		});
	}

	void VulkanRenderer::Submit(const FogBoxVolumeComponent& fogVolume, const glm::mat4& transform)
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		Renderer::Submit([&, fogVolume, transform, currentFrameIndex]()
		{
			s_RenderQueue[currentFrameIndex].AddFogVolume(fogVolume, transform);
		});
	}

	void VulkanRenderer::Submit(const CloudVolumeComponent& cloudVolume, const glm::vec3& position, const glm::vec3& scale)
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		Renderer::Submit([&, cloudVolume, position, scale, currentFrameIndex]()
		{
			s_RenderQueue[currentFrameIndex].AddCloudVolume(cloudVolume, position, scale);
		});
	}

	void VulkanRenderer::Submit(const RectangularLightComponent& rectLight, const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& scale)
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		Renderer::Submit([&, rectLight, position, rotation, scale, currentFrameIndex]()
		{
			s_RenderQueue[currentFrameIndex].AddRectangularLight(rectLight, position, rotation, scale);
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

			// Debug Data
			vkDestroyQueryPool(device, s_DebugData->TimestampQueryPools[i], nullptr);

			// Clear all the submitted data (from every frame remaning)
			s_RenderQueue[i].Reset();
		}
		VulkanMaterial::DeallocateDescriptorPool();
	}

	uint64_t VulkanRenderer::BeginTimestampQuery()
	{
		uint64_t queryIndex = s_DebugData->NextAvailableQueryID;
		s_DebugData->NextAvailableQueryID += 2;

		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		VkQueryPool queryPool = s_DebugData->TimestampQueryPools[currentFrameIndex];
		vkCmdWriteTimestamp(cmdBuf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPool, queryIndex);
		return queryIndex;
	}

	void VulkanRenderer::EndTimestampQuery(uint64_t queryId)
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		VkQueryPool queryPool = s_DebugData->TimestampQueryPools[currentFrameIndex];
		vkCmdWriteTimestamp(cmdBuf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPool, queryId + 1);
	}

	void VulkanRenderer::GetTimestampResults(uint32_t currentFrameIndex)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		// Retrieve timestamp query results
		vkGetQueryPoolResults(device, s_DebugData->TimestampQueryPools[currentFrameIndex], 0, s_DebugData->NextAvailableQueryID,
			s_DebugData->NextAvailableQueryID * sizeof(uint64_t), s_DebugData->TimestampQueryResults[currentFrameIndex].data(), sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);

		for (uint32_t i = 0; i < s_DebugData->NextAvailableQueryID; i += 2)
		{
			uint64_t startTime = s_DebugData->TimestampQueryResults[currentFrameIndex][i];
			uint64_t endTime = s_DebugData->TimestampQueryResults[currentFrameIndex][i + 1];
			float nsTime = endTime > startTime ? (endTime - startTime) * s_DebugData->Device_TimestampPeriod : 0.0f;
			s_DebugData->ExecutionTimes[currentFrameIndex][i / 2] = nsTime * 0.000001f; // Time in ms
		}
	}

	const Vector<float>& VulkanRenderer::GetFrameExecutionTimings()
	{
		int32_t currentFrameIndex = int32_t(VulkanContext::GetSwapChain()->GetCurrentFrameIndex());
		return s_DebugData->ExecutionTimes[currentFrameIndex];
	}

	void VulkanRenderer::Render()
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VulkanContext::GetSwapChain()->Present(s_Data->FinishedSemapore[currentFrameIndex], s_ImageIndex);
	}

	void VulkanRenderer::RenderDebugger()
	{
		s_Data->s_RendererDebugger->ImGuiRender();
	}

	void VulkanRenderer::BeginTimeStampPass(const std::string& passName)
	{
		s_Data->s_RendererDebugger->StartTimeStapForPass(passName);
	}

	void VulkanRenderer::EndTimeStampPass(const std::string& passName)
	{
		s_Data->s_RendererDebugger->EndTimeStapForPass(passName);
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