#include "frostpch.h"
#include "VulkanGeometryPass.h"

#include "Frost/Platform/Vulkan/VulkanFramebuffer.h"
#include "Frost/Platform/Vulkan/VulkanRenderPass.h"
#include "Frost/Platform/Vulkan/VulkanPipeline.h"
#include "Frost/Platform/Vulkan/VulkanContext.h"

namespace Frost
{

	VulkanGeometryPass::VulkanGeometryPass()
		: m_Name("GeometryPass")
	{
	}

	VulkanGeometryPass::~VulkanGeometryPass()
	{
	}

	void VulkanGeometryPass::Init(SceneRenderPassPipeline* renderPassPipeline)
	{
		m_RenderPassPipeline = renderPassPipeline;

		m_Data.Shader = Shader::Create("assets/shader/geometry_pass.glsl");

		FramebufferSpecification framebufferSpec{};
		framebufferSpec.Width = 1600;
		framebufferSpec.Height = 900;
		framebufferSpec.Attachments = { FramebufferTextureFormat::RGBA16F, FramebufferTextureFormat::Depth };

		m_Data.RenderPass = RenderPass::Create(framebufferSpec);


		BufferLayout bufferLayout = {
			{ "a_Position",  ShaderDataType::Float3 },
			{ "a_TexCoord",	 ShaderDataType::Float2 },
			{ "a_Normal",	 ShaderDataType::Float3 },
			{ "a_Tangent",	 ShaderDataType::Float3 },
			{ "a_Bitangent", ShaderDataType::Float3 }
		};
		Pipeline::CreateInfo pipelineCreateInfo{};
		pipelineCreateInfo.Shader = m_Data.Shader;
		pipelineCreateInfo.UseDepth = true;
		pipelineCreateInfo.RenderPass = m_Data.RenderPass;
		pipelineCreateInfo.VertexBufferLayout = bufferLayout;
		m_Data.Pipeline = Pipeline::Create(pipelineCreateInfo);

		for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			m_Data.Framebuffer[i] = Framebuffer::Create(m_Data.RenderPass, framebufferSpec);
			m_Data.Descriptor[i] = Material::Create(m_Data.Shader, "GeometryPassDescriptor");
		}
	}

	void VulkanGeometryPass::OnUpdate(const RenderQueue& renderQueue)
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		auto framebufferSpec = m_Data.Framebuffer[currentFrameIndex]->GetSpecification();
		Ref<VulkanPipeline> vulkanPipeline = m_Data.Pipeline.As<VulkanPipeline>();

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = m_Data.RenderPass.As<VulkanRenderPass>()->GetVulkanRenderPass();
		renderPassInfo.framebuffer = (VkFramebuffer)m_Data.Framebuffer[currentFrameIndex]->GetFramebufferHandle();
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = { framebufferSpec.Width, framebufferSpec.Height };

		std::array<VkClearValue, 2> vkClearValues;
		vkClearValues[0].color = { 0.0f, 0.0f, 0.0f, 0.0f };
		vkClearValues[1].depthStencil = { 1.0f, 0 };

		renderPassInfo.clearValueCount = static_cast<uint32_t>(vkClearValues.size());
		renderPassInfo.pClearValues = vkClearValues.data();

		vkCmdBeginRenderPass(cmdBuf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		vulkanPipeline->Bind();

		VkViewport viewport{};
		viewport.width = (float)framebufferSpec.Width;
		viewport.height = (float)framebufferSpec.Height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.extent = { framebufferSpec.Width, framebufferSpec.Height };
		scissor.offset = { 0, 0 };
		vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

		glm::mat4 projectioMatrix = renderQueue.CameraProjectionMatrix;
		projectioMatrix[1][1] *= -1;
		glm::mat4 viewProjectionMatrix = projectioMatrix * renderQueue.CameraViewMatrix;

		for (uint32_t i = 0; i < renderQueue.m_DataSize; i++)
		{
			auto mesh = renderQueue.m_Data[i];
			
			std::array<glm::mat4, 2> matricies;
			matricies[0] = viewProjectionMatrix * renderQueue.m_Data[i].Transform;
			matricies[1] = renderQueue.m_Data[i].Transform;

			vulkanPipeline->BindVulkanPushConstant("pushC", matricies.data());

			mesh.Mesh->GetVertexBuffer()->Bind();
			mesh.Mesh->GetIndexBuffer()->Bind();

			uint32_t count = mesh.Mesh->GetIndexBuffer()->GetCount() * 3;
			vkCmdDrawIndexed(cmdBuf, count, 1, 0, 0, 0);
		}

		vkCmdEndRenderPass(cmdBuf);

	}

	void VulkanGeometryPass::OnResize(uint32_t width, uint32_t height)
	{
		FramebufferSpecification framebufferSpec{};
		framebufferSpec.Width = width;
		framebufferSpec.Height = height;
		framebufferSpec.Attachments = { FramebufferTextureFormat::RGBA16F, FramebufferTextureFormat::Depth };

		for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			m_Data.Framebuffer[i]->Destroy();
			m_Data.Descriptor[i]->Destroy();

			m_Data.Framebuffer[i] = Framebuffer::Create(m_Data.RenderPass, framebufferSpec);
			m_Data.Descriptor[i] = Material::Create(m_Data.Shader, "GeometryPassDescriptor");
		}
	}

	void VulkanGeometryPass::ShutDown()
	{
		m_Data.RenderPass->Destroy();
		m_Data.Pipeline->Destroy();
		m_Data.Shader->Destroy();

		for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			m_Data.Framebuffer[i]->Destroy();
			m_Data.Descriptor[i]->Destroy();
		}
	}

}