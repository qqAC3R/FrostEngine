#include "frostpch.h"
#include "VulkanGeometryPass.h"

#include "Frost/Platform/Vulkan/VulkanFramebuffer.h"
#include "Frost/Platform/Vulkan/VulkanRenderPass.h"
#include "Frost/Platform/Vulkan/VulkanRenderer.h"
#include "Frost/Platform/Vulkan/VulkanPipeline.h"
#include "Frost/Platform/Vulkan/VulkanContext.h"
#include "Frost/Platform/Vulkan/VulkanTexture.h"
#include "Frost/Platform/Vulkan/VulkanImage.h"
#include "Frost/Platform/Vulkan/VulkanMaterial.h"

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

		m_Data.Shader = Renderer::GetShaderLibrary()->Get("GeometryPass");

		RenderPassSpecification renderPassSpec = 
		{
			1600, 900, 3,
			{
				// Position Attachment
				{
					FramebufferTextureFormat::RGBA16F, ImageUsage::Storage,
					OperationLoad::Clear,    OperationStore::Store,    // Color attachment
					OperationLoad::DontCare, OperationStore::DontCare, // Depth attachment
				},

				// Normals Attachment
				{
					FramebufferTextureFormat::RGBA16F, ImageUsage::Storage,
					OperationLoad::Clear,    OperationStore::Store,    // Color attachment
					OperationLoad::DontCare, OperationStore::DontCare, // Depth attachment
				},

				// Albedo Attachment
				{
					FramebufferTextureFormat::RGBA16F, ImageUsage::Storage,
					OperationLoad::Clear,    OperationStore::Store,    // Color attachment
					OperationLoad::DontCare, OperationStore::DontCare, // Depth attachment
				},

				// Composite Attachment
				{
					FramebufferTextureFormat::RGBA16F, ImageUsage::Storage,
					OperationLoad::Clear,    OperationStore::Store,    // Color attachment
					OperationLoad::DontCare, OperationStore::DontCare, // Depth attachment
				},

				// Depth Attachment
				{
					FramebufferTextureFormat::Depth, ImageUsage::DepthStencil,
					OperationLoad::Clear,    OperationStore::Store,    // Color attachment
					OperationLoad::DontCare, OperationStore::DontCare, // Depth attachment
				}
			}
		};
		m_Data.RenderPass = RenderPass::Create(renderPassSpec);


		BufferLayout bufferLayout = {
			{ "a_Position",  ShaderDataType::Float3 },
			{ "a_TexCoord",	 ShaderDataType::Float2 },
			{ "a_Normal",	 ShaderDataType::Float3 },
			{ "a_Tangent",	 ShaderDataType::Float3 },
			{ "a_Bitangent", ShaderDataType::Float3 },
			{ "a_MeshIndex", ShaderDataType::Float }
		};
		Pipeline::CreateInfo pipelineCreateInfo{};
		pipelineCreateInfo.Shader = m_Data.Shader;
		pipelineCreateInfo.UseDepth = true;
		pipelineCreateInfo.RenderPass = m_Data.RenderPass;
		pipelineCreateInfo.VertexBufferLayout = bufferLayout;
		m_Data.Pipeline = Pipeline::Create(pipelineCreateInfo);

		//for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++)
		//	m_Data.Descriptor[i] = Material::Create(m_Data.Shader, "GeometryPassDescriptor");
	}

	void VulkanGeometryPass::OnUpdate(const RenderQueue& renderQueue)
	{
		// If we have 0 meshes, we shouldnt render this pass
		if (renderQueue.m_DataSize == 0) return;

		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		Ref<Framebuffer> framebuffer = m_Data.RenderPass->GetFramebuffer(currentFrameIndex);
		Ref<VulkanPipeline> vulkanPipeline = m_Data.Pipeline.As<VulkanPipeline>();

		Ref<VulkanImage2D> texture = framebuffer->GetColorAttachment(0).As<VulkanImage2D>();

		m_Data.RenderPass->Bind();
		vulkanPipeline->Bind();

		VkViewport viewport{};
		viewport.width = (float)framebuffer->GetSpecification().Width;
		viewport.height = (float)framebuffer->GetSpecification().Height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.extent = { framebuffer->GetSpecification().Width, framebuffer->GetSpecification().Height };
		scissor.offset = { 0, 0 };
		vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

		glm::mat4 projectioMatrix = renderQueue.CameraProjectionMatrix;
		projectioMatrix[1][1] *= -1;
		glm::mat4 viewProjectionMatrix = projectioMatrix * renderQueue.CameraViewMatrix;

		for (uint32_t i = 0; i < renderQueue.m_DataSize; i++)
		{
			auto mesh = renderQueue.m_Data[i].Mesh;

			mesh->GetVertexBuffer()->Bind();
			mesh->GetIndexBuffer()->Bind();

			for (auto& submesh : mesh->GetSubMeshes())
			{
				Ref<VulkanMaterial> material = mesh->GetVulkanMaterial()[submesh.MaterialIndex].As<VulkanMaterial>();
				material->Bind(m_Data.Pipeline);

				PushConstant pushConstant;
				pushConstant.TransformMatrix = viewProjectionMatrix * submesh.Transform * renderQueue.m_Data[i].Transform;
				pushConstant.ModelMatrix = renderQueue.m_Data[i].Transform * submesh.Transform;
				vulkanPipeline->BindVulkanPushConstant("u_PushConstant", (void*)&pushConstant);

				vkCmdDrawIndexed(cmdBuf, submesh.IndexCount, 1, submesh.BaseIndex, submesh.BaseVertex, 0);
			}
		}

		m_Data.RenderPass->Unbind();
	}

	void VulkanGeometryPass::OnResize(uint32_t width, uint32_t height)
	{

		m_Data.RenderPass->Destroy();

		RenderPassSpecification renderPassSpec =
		{
			width, height, 3,
			{
				// Position Attachment
				{
					FramebufferTextureFormat::RGBA16F, ImageUsage::Storage,
					OperationLoad::Clear,    OperationStore::Store,    // Color attachment
					OperationLoad::DontCare, OperationStore::DontCare, // Depth attachment
				},

				// Normals Attachment
				{
					FramebufferTextureFormat::RGBA16F, ImageUsage::Storage,
					OperationLoad::Clear,    OperationStore::Store,    // Color attachment
					OperationLoad::DontCare, OperationStore::DontCare, // Depth attachment
				},

				// Albedo Attachment
				{
					FramebufferTextureFormat::RGBA16F, ImageUsage::Storage,
					OperationLoad::Clear,    OperationStore::Store,    // Color attachment
					OperationLoad::DontCare, OperationStore::DontCare, // Depth attachment
				},

				// Composite Attachment
				{
					FramebufferTextureFormat::RGBA16F, ImageUsage::Storage,
					OperationLoad::Clear,    OperationStore::Store,    // Color attachment
					OperationLoad::DontCare, OperationStore::DontCare, // Depth attachment
				},

				// Depth Attachment
				{
					FramebufferTextureFormat::Depth, ImageUsage::DepthStencil,
					OperationLoad::Clear,    OperationStore::Store,    // Color attachment
					OperationLoad::DontCare, OperationStore::DontCare, // Depth attachment
				}
			}
		};
		m_Data.RenderPass = RenderPass::Create(renderPassSpec);

#if 0
		for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			m_Data.Descriptor[i]->Destroy();
			m_Data.Descriptor[i] = Material::Create(m_Data.Shader, "GeometryPassDescriptor");
		}
#endif
	}

	void VulkanGeometryPass::ShutDown()
	{
		m_Data.RenderPass->Destroy();
		m_Data.Pipeline->Destroy();

#if 0
		for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			m_Data.Descriptor[i]->Destroy();
		}
#endif
	}

}