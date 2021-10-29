#include "frostpch.h"
#include "VulkanCompositePass.h"

#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanGeometryPass.h"
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

	VulkanCompositePass::VulkanCompositePass()
		: m_Name("CompositePass")
	{
	}

	VulkanCompositePass::~VulkanCompositePass()
	{
	}

	void VulkanCompositePass::Init(SceneRenderPassPipeline* renderPassPipeline)
	{
		m_RenderPassPipeline = renderPassPipeline;
		m_Data = new InternalData();

		m_Data->Shader = Renderer::GetShaderLibrary()->Get("PBRDeffered");

		RenderPassSpecification renderPassSpec =
		{
			1600, 900, 3,
			{
				// Color Attachment
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
		m_Data->RenderPass = RenderPass::Create(renderPassSpec);

		BufferLayout bufferLayout = {};
		Pipeline::CreateInfo pipelineCreateInfo{};
		pipelineCreateInfo.Shader = m_Data->Shader;
		pipelineCreateInfo.UseDepth = true;
		pipelineCreateInfo.RenderPass = m_Data->RenderPass;
		pipelineCreateInfo.VertexBufferLayout = bufferLayout;
		pipelineCreateInfo.Topology = PrimitiveTopology::TriangleStrip;
		m_Data->Pipeline = Pipeline::Create(pipelineCreateInfo);


		m_PointLightData = new PointLightData();
		m_PointLightData->LightCount = 1;
		m_PointLightData->PointLights.resize(1);

		m_PointLightData->PointLights[0].Position = glm::vec3(5.0f, 10.0f, 5.0f);
		m_PointLightData->PointLights[0].Radiance = glm::vec3(1.0f);
		m_PointLightData->PointLights[0].Radius = 2.0f;
		m_PointLightData->PointLights[0].Falloff = 1.0f;

		uint32_t uboSize = sizeof(uint32_t) + sizeof(PointLightProperties);

		m_Data->m_PointLightUniformBuffer = UniformBuffer::Create(uboSize);
		m_Data->m_PointLightUniformBuffer->SetData(&m_PointLightData);


		uint32_t index = 0;
		m_Data->Descriptor.resize(Renderer::GetRendererConfig().FramesInFlight);
		for (auto& descriptor : m_Data->Descriptor)
		{
			descriptor = Material::Create(m_Data->Shader, "CompositePass-Material");

			Ref<Image2D> positionTexture =  m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->RenderPass->GetColorAttachment(0, index);
			Ref<Image2D> normalTexture =    m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->RenderPass->GetColorAttachment(1, index);
			Ref<Image2D> albedoTexture =    m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->RenderPass->GetColorAttachment(2, index);
			Ref<Image2D> compositeTexture = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->RenderPass->GetColorAttachment(3, index);
			
			descriptor->Set("u_PositionTexture", positionTexture);
			descriptor->Set("u_NormalTexture", normalTexture);
			descriptor->Set("u_AlbedoTexture", albedoTexture);
			descriptor->Set("u_CompositeTexture", compositeTexture);
			//descriptor->Set("LightData", m_PointLightUniformBuffer);

			descriptor.As<VulkanMaterial>()->UpdateVulkanDescriptorIfNeeded();

			index++;
		}

	}

	void VulkanCompositePass::OnUpdate(const RenderQueue& renderQueue)
	{
		// If we have 0 meshes, we shouldnt render this pass
		if (renderQueue.GetQueueSize() == 0) return;

		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		Ref<Framebuffer> framebuffer = m_Data->RenderPass->GetFramebuffer(currentFrameIndex);
		Ref<VulkanPipeline> vulkanPipeline = m_Data->Pipeline.As<VulkanPipeline>();

		Ref<VulkanImage2D> texture = framebuffer->GetColorAttachment(0).As<VulkanImage2D>();

		// Updating the push constant data from the renderQueue
		m_PushConstantData.CameraPosition = renderQueue.CameraPosition;

		m_Data->RenderPass->Bind(); 
		vulkanPipeline->Bind();
		vulkanPipeline->BindVulkanPushConstant("u_PushConstant", &m_PushConstantData);

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

		// Drawing a quad
		m_Data->Descriptor[currentFrameIndex]->Bind(m_Data->Pipeline);
		vkCmdDraw(cmdBuf, 3, 1, 0, 0);

		m_Data->RenderPass->Unbind();
	}

	void VulkanCompositePass::OnResize(uint32_t width, uint32_t height)
	{
		//m_Data->RenderPass->Destroy();

		RenderPassSpecification renderPassSpec =
		{
			width, height, 3,
			{
				// Color Attachment
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
		m_Data->RenderPass = RenderPass::Create(renderPassSpec);


		uint32_t index = 0;
		for (auto& descriptor : m_Data->Descriptor)
		{
			Ref<Image2D> positionTexture =  m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->RenderPass->GetColorAttachment(0, index);
			Ref<Image2D> normalTexture =    m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->RenderPass->GetColorAttachment(1, index);
			Ref<Image2D> albedoTexture =    m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->RenderPass->GetColorAttachment(2, index);
			Ref<Image2D> compositeTexture = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->RenderPass->GetColorAttachment(3, index);

			descriptor->Set("u_PositionTexture", positionTexture);
			descriptor->Set("u_NormalTexture", normalTexture);
			descriptor->Set("u_AlbedoTexture", albedoTexture);
			descriptor->Set("u_CompositeTexture", compositeTexture);

			descriptor.As<VulkanMaterial>()->UpdateVulkanDescriptorIfNeeded();

			index++;
		}

	}

	void VulkanCompositePass::ShutDown()
	{
		delete m_PointLightData;
		delete m_Data;
	}
}