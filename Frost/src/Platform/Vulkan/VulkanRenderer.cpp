#include "frostpch.h"
#include "VulkanRenderer.h"

#include "Frost/Core/Application.h"
#include "Frost/Renderer/Renderer.h"

#include "Frost/Renderer/Pipeline.h"
#include "Frost/Renderer/ShaderLibrary.h"
#include "Frost/Renderer/SceneRenderPass.h"
#include "Frost/Renderer/Buffers/UniformBuffer.h"

#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanSyncObjects.h"
#include "Platform/Vulkan/VulkanCommandBuffer.h"

#include "Platform/Vulkan/SceneRenderPasses/VulkanRayTracingPass.h"

#include <imgui.h>
#include <examples/imgui_impl_vulkan.h>

namespace Frost
{


	struct RenderData
	{
		/* The engine can support multiple renderpasses */
		Ref<SceneRenderPassPipeline> SceneRenderPasses;

		/* Camera related */
		//Ref<EditorCamera> Camera;
		glm::mat4 ProjectionMatrix;
		glm::mat4 ViewMatrix;


		/* Rendering Part */
		Ref<Pipeline> Pipeline;
		//Ref<RenderPass> RenderPass;
		//Vector<Ref<Framebuffer>> Framebuffer;
		
		// TODO: Make 3 because we need frames in flight
		VulkanCommandPool CmdPool;
		VulkanCommandBuffer CmdBuffer[FRAMES_IN_FLIGHT];
		VulkanFence FencesInFlight[FRAMES_IN_FLIGHT];

		Vector<VkFence> FencesInCheck;

		//VulkanFence FencesInCheck[FRAMES_IN_FLIGHT];
		VulkanSemaphore AvailableSemapore[FRAMES_IN_FLIGHT], FinishedSemapore[FRAMES_IN_FLIGHT];


		Ref<Shader> QuadShader;
		Ref<Material> QuadDescriptor[FRAMES_IN_FLIGHT];
		Ref<VertexBuffer> QuadVertexBuffer;
		Ref<IndexBuffer> QuadIndexBuffer;

	};


	static uint32_t s_ImageIndex = 0;
	static uint32_t s_CurrentFrame = 0;
	// TODO: Make 3 because we need frames in flight (std::array<RenderQueue, 3> s_RenderQueue)
	static RenderQueue s_RenderQueue[FRAMES_IN_FLIGHT];
	static RenderData* s_Data;
	

	struct PipelinePushConstant
	{
		glm::vec3 Color{ 1.0f };
	};
	static PipelinePushConstant s_PushConstant;

	void VulkanRenderer::Init()
	{
		s_Data = new RenderData;

		s_Data->SceneRenderPasses = Ref<SceneRenderPassPipeline>::Create();
		s_Data->QuadShader = Shader::Create("assets/shader/main.glsl");


		{
			// Syncronization

			s_Data->CmdPool.CreateCommandBuffer();


			for (uint32_t i = 0; i < VulkanContext::GetSwapChain()->GetSwapChainImages().size(); i++)
				s_Data->FencesInCheck.push_back(VK_NULL_HANDLE);


			for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++)
			{

				s_Data->CmdBuffer[i].Allocate(s_Data->CmdPool.GetCommandPool());

				s_Data->FencesInFlight[i].Create();

				s_Data->AvailableSemapore[i].Create();
				s_Data->FinishedSemapore[i].Create();
			}

		}


#if 0
		{
			// Framebuffers/Renderpasses/SceneRenderpasses

			{
				FramebufferSpecification framebufferSpec{};
				framebufferSpec.Width = Application::Get().GetWindow().GetWidth();
				framebufferSpec.Height = Application::Get().GetWindow().GetHeight();
				framebufferSpec.Attachments = { FramebufferTextureFormat::SWAPCHAIN, FramebufferTextureFormat::Depth };

				s_Data->RenderPass = RenderPass::Create(framebufferSpec);

			}

			{

				auto& swapChain = VulkanContext::GetSwapChain();
				size_t numberOfSwapChainImages = swapChain->GetSwapChainImages().size();
				s_Data->Framebuffer.resize(numberOfSwapChainImages);


				for (uint32_t i = 0; i < swapChain->GetSwapChainImages().size(); i++)
				{
					void* swapChainImage = swapChain->GetSwapChainImages()[i];

					FramebufferSpecification framebufferSpec{};
					framebufferSpec.Width = Application::Get().GetWindow().GetWidth();
					framebufferSpec.Height = Application::Get().GetWindow().GetHeight();
					framebufferSpec.Attachments = {
						  FramebufferTextureFormat::SWAPCHAIN,
						  FramebufferTextureFormat::Depth
					};

					// TODO: Meh, should pass Frost API's renderpass (not the vk one)
					s_Data->Framebuffer[i] = Framebuffer::Create(s_Data->RenderPass, framebufferSpec);

				}
			}

		}
#endif




		{
			// Scene render passes
			s_Data->SceneRenderPasses->AddRenderPass(Ref<VulkanRayTracingPass>::Create());

		}






		{
			float vertices[] = {
				//    X      Y      Z        U    V
					-1.0f, -1.0f,  0.0f,   0.0f, 0.0f,
					 1.0f, -1.0f,  0.0f,   1.0f, 0.0f,
					 1.0f,  1.0f,  0.0f,   1.0f, 1.0f,
					-1.0f,  1.0f,  0.0f,   0.0f, 1.0f,
			};
			s_Data->QuadVertexBuffer = VertexBuffer::Create(vertices, sizeof(vertices));


			uint32_t indicies[] = {
				0, 1, 2,
				2, 3, 0,
			};
			s_Data->QuadIndexBuffer = IndexBuffer::Create(indicies, sizeof(indicies) / sizeof(uint32_t));


			for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++)
			{
				s_Data->QuadDescriptor[i] = Material::Create(s_Data->QuadShader, "Main");
				s_Data->QuadDescriptor[i]->Set("u_FullScreenTexture", s_Data->SceneRenderPasses->GetRenderPassData<VulkanRayTracingPass>()->DisplayTexture[i]);
				s_Data->QuadDescriptor[i]->UpdateVulkanDescriptor();
			}

			//s_Data->SceneRenderPasses->GetRenderPassData<VulkanRayTracingPass>();
			


			FramebufferSpecification framebufferSpec{};
			framebufferSpec.Width = Application::Get().GetWindow().GetWidth();
			framebufferSpec.Height = Application::Get().GetWindow().GetHeight();

			// DRAW QUAD
			BufferLayout bufferLayout = {
				{ "a_Position", ShaderDataType::Float3 },
				{ "a_TexCoord",	ShaderDataType::Float2 }
			};


			Pipeline::CreateInfo pipelineCreateInfo{};
			pipelineCreateInfo.FramebufferSpecification = framebufferSpec;
			pipelineCreateInfo.Shader = s_Data->QuadShader;
			pipelineCreateInfo.RenderPass = VulkanContext::GetRenderPass();
			pipelineCreateInfo.VertexBufferLayout = bufferLayout;
			pipelineCreateInfo.PushConstant.PushConstantRange = sizeof(PipelinePushConstant);
			pipelineCreateInfo.PushConstant.PushConstantShaderStages = { ShaderType::Fragment };


			s_Data->Pipeline = Pipeline::Create(pipelineCreateInfo);

		}

		Application::Get().GetImGuiLayer()->OnInit(VulkanContext::GetRenderPass()->GetVulkanRenderPass());


	}

	static void SubmitToGraphicsQueue()
	{
		VkCommandBuffer cmdBuf = s_Data->CmdBuffer[s_CurrentFrame].GetCommandBuffer();
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		VkFence fenceInFlight = s_Data->FencesInFlight[s_CurrentFrame].GetFence();
		
		VkSemaphore availableSemaphore = s_Data->AvailableSemapore[s_CurrentFrame].GetSemaphore();
		VkSemaphore finishedSemaphore = s_Data->FinishedSemapore[s_CurrentFrame].GetSemaphore();


	


		
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

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
		FROST_VKCHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, fenceInFlight), "Failed to submit draw command buffer!");

	}


	void VulkanRenderer::BeginScene(const EditorCamera& camera)
	{
		Renderer::Submit([=]() mutable
		{
			s_Data->ViewMatrix = camera.GetViewMatrix();
			s_Data->ProjectionMatrix = camera.GetProjectionMatrix();

			s_RenderQueue[s_CurrentFrame].SetCamera(camera);
		});
	}

	void VulkanRenderer::EndScene()
	{
		Renderer::Submit([=]() mutable
		{
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
			VkCommandBuffer cmdBuf = s_Data->CmdBuffer[s_CurrentFrame].GetCommandBuffer();

			/* Acquire the image */
			s_Data->FencesInFlight[s_CurrentFrame].Wait();
			auto [result, imageIndex] = VulkanContext::AcquireNextSwapChainImage(s_Data->AvailableSemapore[s_CurrentFrame]);
			s_ImageIndex = imageIndex;



			/* Checking if the fence is flight is already being used to render */
			if (s_Data->FencesInCheck[s_ImageIndex] != VK_NULL_HANDLE)
				vkWaitForFences(device, 1, &s_Data->FencesInCheck[s_ImageIndex], VK_TRUE, UINT64_MAX);


			/* When the fence was finished being used by the renderer, we mark the fence as now being in use by this frame */
			s_Data->FencesInCheck[s_ImageIndex] = s_Data->FencesInFlight[s_CurrentFrame].GetFence();



			/* Begin recording the frame's commandbuffer */
			s_Data->CmdBuffer[s_CurrentFrame].Begin();


			/* Updating all the graphics Passes */
			s_Data->SceneRenderPasses->UpdateRenderPasses(s_RenderQueue[s_CurrentFrame], cmdBuf, s_CurrentFrame);
			s_RenderQueue[s_CurrentFrame].Reset();




			/* Rendering a quad for the swap chain image */
			VulkanContext::BeginFrame(cmdBuf, s_ImageIndex);


			s_Data->Pipeline->Bind(cmdBuf);
			s_Data->Pipeline->BindVulkanPushConstant(cmdBuf, (void*)&s_PushConstant);

			s_Data->QuadDescriptor[s_CurrentFrame]->Bind(cmdBuf, s_Data->Pipeline->GetVulkanPipelineLayout(), GraphicsType::Graphics);


			auto& vertexBuffer = s_Data->QuadVertexBuffer;
			auto& indexBuffer = s_Data->QuadIndexBuffer;

			{

				// TODO: DrawIndexed etc.
				//RenderCommand::DrawIndexed(vertexBuffer, indexBuffer, cmdBuf);
				uint32_t count = static_cast<uint32_t>(indexBuffer->GetBufferSize() / sizeof(uint32_t));
				vertexBuffer->Bind(cmdBuf);
				indexBuffer->Bind(cmdBuf);

				vkCmdDrawIndexed(cmdBuf, count, 1, 0, 0, 0);
			}



			{
				/* Rendering the UI over the quad */
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
				ImGui::Begin("Viewport");

				bool IsViewPortFocused = ImGui::IsWindowFocused();
				Application::Get().GetImGuiLayer()->BlockEvents(!IsViewPortFocused);

				auto texture = s_Data->SceneRenderPasses->GetRenderPassData<VulkanRayTracingPass>()->DisplayTexture[s_CurrentFrame];
				ImVec2 viewPortSizePanel = ImGui::GetContentRegionAvail();
				ImGui::Image(ImGuiLayer::GetTextureIDFromVulkanTexture(texture), ImVec2{ viewPortSizePanel.x, viewPortSizePanel.y });

				ImGui::End();
				ImGui::PopStyleVar();

				Application::Get().GetImGuiLayer()->Render(cmdBuf);
			}

			VulkanContext::EndFrame(cmdBuf);
			s_Data->CmdBuffer[s_CurrentFrame].End();


			SubmitToGraphicsQueue();
		});

	}

	void VulkanRenderer::Submit(const Ref<Mesh>& mesh, const glm::mat4& transform)
	{
		Renderer::Submit([=]() mutable
		{
			s_RenderQueue[s_CurrentFrame].Add(mesh, transform);
		});
	}

	void VulkanRenderer::Submit(const Ref<Mesh>& mesh, Ref<Material> material, const glm::mat4& transform)
	{
		// TODO: Materials dont acutally work
	
		//s_RenderQueue.Add(mesh, material, transform);
	}

	void VulkanRenderer::ShutDown()
	{
		s_Data->SceneRenderPasses->ShutDown();
		

		s_Data->Pipeline->Destroy();

		s_Data->QuadVertexBuffer->Destroy();
		s_Data->QuadIndexBuffer->Destroy();
		s_Data->QuadShader->Destroy();

		for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++)
			s_Data->QuadDescriptor[i]->Destroy();



		for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			s_Data->AvailableSemapore[i].Destroy();
			s_Data->FinishedSemapore[i].Destroy();

			s_Data->FencesInFlight[i].Destroy();
			s_Data->CmdBuffer[i].Destroy();
			
		}

		s_Data->CmdPool.Destroy();
	}

	void VulkanRenderer::Update()
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		VkCommandBuffer cmdBuf = s_Data->CmdBuffer[s_CurrentFrame].GetCommandBuffer();

		/* Run the render submitted queue */
		s_RenderSubmitQueue.Run();


		
		VulkanContext::Present(s_Data->FinishedSemapore[s_CurrentFrame].GetSemaphore(), s_ImageIndex);


		s_CurrentFrame = (s_CurrentFrame + 1) % FRAMES_IN_FLIGHT;
	}

	void VulkanRenderer::Resize(uint32_t width, uint32_t height)
	{
		s_Data->SceneRenderPasses->ResizeRenderPasses(width, height);

		for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			s_Data->QuadDescriptor[i]->Set("u_FullScreenTexture", s_Data->SceneRenderPasses->GetRenderPassData<VulkanRayTracingPass>()->DisplayTexture[i]);
			s_Data->QuadDescriptor[i]->UpdateVulkanDescriptor();
		}
	}

}