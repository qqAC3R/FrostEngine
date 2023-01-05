#include "frostpch.h"
#include "VulkanBatchRenderingPass.h"

#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanCompositePass.h"
#include "Frost/Platform/Vulkan/VulkanContext.h"
#include "Frost/Platform/Vulkan/VulkanImage.h"
#include "Frost/Platform/Vulkan/VulkanBindlessAllocator.h"
#include "Frost/Platform/Vulkan/VulkanMaterial.h"
#include "Frost/Platform/Vulkan/VulkanPipeline.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanBufferDevice.h"
#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanGeometryPass.h"
#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanPostFXPass.h"

namespace Frost
{
	VulkanBatchRenderingPass::VulkanBatchRenderingPass()
		: m_Name("BatchRenderingPass")
	{
	}

	VulkanBatchRenderingPass::~VulkanBatchRenderingPass()
	{
	}

	void VulkanBatchRenderingPass::Init(SceneRenderPassPipeline* renderPassPipeline)
	{
		m_RenderPassPipeline = renderPassPipeline;
		m_Data = new InternalData();

		m_Data->BatchRendererShader = Renderer::GetShaderLibrary()->Get("BatchRenderer");

		BatchRendererInitData(1600, 900);

		Renderer::SubmitImageToOutputImageMap("FinalImage_With2D", [this]() -> Ref<Image2D>
		{
			uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
			return this->m_Data->BatchRendererRenderPass->GetColorAttachment(0, currentFrameIndex);
		});
	}

	void VulkanBatchRenderingPass::BatchRendererInitData(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;

		//Ref<RenderPass> compositeRenderpass = m_RenderPassPipeline->GetRenderPassData<VulkanCompositePass>()->RenderPass;

		RenderPassSpecification renderPassSpec =
		{
			width, height, framesInFlight,
			{
				// Color Attachment
				{
					FramebufferTextureFormat::RGBA8, ImageUsage::Storage,
					OperationLoad::Load,  OperationStore::Store,
					OperationLoad::Load,   OperationStore::DontCare,
				},
				// Depth Attachment
				{
					FramebufferTextureFormat::Depth, ImageUsage::DepthStencil,
					OperationLoad::Load,        OperationStore::Store,
					OperationLoad::DontCare,    OperationStore::DontCare,
				}
			}
		};
		m_Data->BatchRendererRenderPass = RenderPass::Create(renderPassSpec);



		// Composite pipeline
		BufferLayout bufferLayout = {
			{ "a_Position",  ShaderDataType::Float3 },
			{ "a_Color",  ShaderDataType::Float4 },
			{ "a_TexCoord",  ShaderDataType::Float2 },
			{ "a_TexIndex",  ShaderDataType::UInt },
		};
		bufferLayout.m_InputType = InputType::Vertex;
		Pipeline::CreateInfo pipelineCreateInfo{};
		pipelineCreateInfo.Shader = m_Data->BatchRendererShader;
		pipelineCreateInfo.UseDepthTest = true;
		pipelineCreateInfo.UseDepthWrite = false;
		pipelineCreateInfo.RenderPass = m_Data->BatchRendererRenderPass;
		pipelineCreateInfo.VertexBufferLayout = bufferLayout;
		pipelineCreateInfo.Topology = PrimitiveTopology::Triangles;
		if (!m_Data->BatchRendererPipeline)
			m_Data->BatchRendererPipeline = Pipeline::Create(pipelineCreateInfo);


		m_Data->BatchRendererMaterial.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			m_Data->BatchRendererMaterial[i] = Material::Create(m_Data->BatchRendererShader, "BatchRendererMaterial");
		}

		{
			m_Data->QuadVertexBuffer.resize(framesInFlight);
			for (uint32_t i = 0; i < framesInFlight; i++)
			{
				m_Data->QuadVertexBuffer[i] = BufferDevice::Create(MaxVertices * sizeof(QuadVertex), {BufferUsage::Vertex});
			}

			m_Data->QuadVertexBufferBase = new QuadVertex[MaxVertices];

			uint32_t* quadIndices = new uint32_t[MaxIndices];

			uint32_t offset = 0;
			for (uint32_t i = 0; i < MaxIndices; i += 6)
			{
				quadIndices[i + 0] = offset + 0;
				quadIndices[i + 1] = offset + 1;
				quadIndices[i + 2] = offset + 2;

				quadIndices[i + 3] = offset + 2;
				quadIndices[i + 4] = offset + 3;
				quadIndices[i + 5] = offset + 0;

				offset += 4;
			}

			m_Data->QuadIndexBuffer = IndexBuffer::Create(quadIndices, MaxIndices);
			delete[] quadIndices;
		}



	}
	
	void VulkanBatchRenderingPass::InitLate()
	{
	}

	void VulkanBatchRenderingPass::OnUpdate(const RenderQueue& renderQueue)
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();

		m_Data->QuadIndexCount = 0;
		m_Data->QuadCount = 0;
		m_Data->QuadVertexBufferPtr = m_Data->QuadVertexBufferBase;

		for (const auto& object2d : renderQueue.m_BatchRendererData)
		{
			switch (object2d.Type)
			{
				case RenderQueue::Object2D::ObjectType::Billboard: SubmitBillboard(renderQueue, object2d); break;
				case RenderQueue::Object2D::ObjectType::Quad:      SubmitQuad(object2d); break;
				default: FROST_CORE_ERROR("2D Object type is unknown!");  break;
			}
		}

		m_Data->QuadVertexBuffer[currentFrameIndex]->SetData(m_Data->QuadCount * 4 * sizeof(QuadVertex), m_Data->QuadVertexBufferBase);

		BatchRendererUpdate(renderQueue);

	}

	void VulkanBatchRenderingPass::BatchRendererUpdate(const RenderQueue& renderQueue)
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		Ref<VulkanPipeline> batchRendererPipeline = m_Data->BatchRendererPipeline.As<VulkanPipeline>();
		Ref<Framebuffer> framebuffer = m_Data->BatchRendererRenderPass->GetFramebuffer(currentFrameIndex);

		{
			Ref<VulkanImage2D> postFxFinalImage = m_RenderPassPipeline->GetRenderPassData<VulkanPostFXPass>()->FinalTexture[currentFrameIndex].As<VulkanImage2D>();
			postFxFinalImage->TransitionLayout(cmdBuf, postFxFinalImage->GetVulkanImageLayout(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
		}

		// Firstly blit the color/depth attachments
		auto vulkanSrcDepthImage = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->GeometryRenderPass->GetDepthAttachment(currentFrameIndex);
		auto vulkanDstDepthImage = m_Data->BatchRendererRenderPass->GetDepthAttachment(currentFrameIndex).As<VulkanImage2D>();
		vulkanDstDepthImage->BlitImage(cmdBuf, vulkanSrcDepthImage);

		auto vulkanSrcColorImage = m_RenderPassPipeline->GetRenderPassData<VulkanPostFXPass>()->FinalTexture[currentFrameIndex];
		auto vulkanDstColorImage = m_Data->BatchRendererRenderPass->GetColorAttachment(0, currentFrameIndex).As<VulkanImage2D>();
		vulkanDstColorImage->BlitImage(cmdBuf, vulkanSrcColorImage);




		glm::mat4 viewProj = renderQueue.m_Camera->GetViewProjectionVK();
		uint32_t indexCount = m_Data->QuadIndexCount;


		m_Data->BatchRendererRenderPass->Bind();
		batchRendererPipeline->Bind();
		batchRendererPipeline->BindVulkanPushConstant("u_PushConstant", &viewProj);


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

		// Render the batched quads
		Ref<VulkanMaterial> batchRendererMaterial = m_Data->BatchRendererMaterial[currentFrameIndex].As<VulkanMaterial>();
		
		// TODO: This is so bad, pls fix this
		VkPipelineLayout pipelineLayout = batchRendererPipeline->GetVulkanPipelineLayout();
		Vector<VkDescriptorSet> descriptorSets = batchRendererMaterial->GetVulkanDescriptorSets();
		descriptorSets[1] = VulkanBindlessAllocator::GetVulkanDescriptorSet(currentFrameIndex);
		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);

		m_Data->QuadIndexBuffer->Bind();
		Ref<VulkanBufferDevice> batchVertexBuffer = m_Data->QuadVertexBuffer[currentFrameIndex].As<VulkanBufferDevice>();

		uint64_t offset = 0;
		VkBuffer batchVertexBufferInternal = batchVertexBuffer->GetVulkanBuffer();
		vkCmdBindVertexBuffers(cmdBuf, 0, 1, &batchVertexBufferInternal, &offset);

		vkCmdDrawIndexed(cmdBuf, indexCount, 1, 0, 0, 0);

		m_Data->BatchRendererRenderPass->Unbind();
	}

	void VulkanBatchRenderingPass::SubmitBillboard(const RenderQueue& renderQueue, const RenderQueue::Object2D& object2d)
	{
		if (m_Data->QuadIndexCount >= MaxIndices)
		{
			FROST_CORE_ERROR("The maximum number of indices has been reached! The batch renderer cannot render more objects!");
			return;
		}

		uint32_t textureSlot;
		if (object2d.Texture)
		{
			Ref<VulkanImage2D> texture = object2d.Texture->GetImage2D().As<VulkanImage2D>();
			if (m_BindlessAllocatedTextures.find(texture->GetVulkanImage()) == m_BindlessAllocatedTextures.end())
			{
				textureSlot = VulkanBindlessAllocator::AddTexture(object2d.Texture);
				m_BindlessAllocatedTextures[texture->GetVulkanImage()] = textureSlot;
			}
			else
			{
				textureSlot = m_BindlessAllocatedTextures[texture->GetVulkanImage()];
			}
		}
		else
		{
			textureSlot = VulkanBindlessAllocator::GetWhiteTextureID();
		}

		glm::vec3 camRightWS = {
			renderQueue.CameraViewMatrix[0][0],
			renderQueue.CameraViewMatrix[1][0],
			renderQueue.CameraViewMatrix[2][0]
		};
		glm::vec3 camUpWS = {
			renderQueue.CameraViewMatrix[0][1],
			renderQueue.CameraViewMatrix[1][1],
			renderQueue.CameraViewMatrix[2][1]
		};

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), object2d.Position) * glm::scale(glm::mat4(1.0f), { object2d.Size.x, object2d.Size.y, 1.0f });

		
		m_Data->QuadVertexBufferPtr->Position = (
			object2d.Position +
			camRightWS * (QuadVertexPositions[0].x) * object2d.Size.x +
			camUpWS *    (QuadVertexPositions[0].y) * object2d.Size.y
		);
		m_Data->QuadVertexBufferPtr->Color = object2d.Color;
		m_Data->QuadVertexBufferPtr->TexCoord = { 0.0f, 0.0f };
		m_Data->QuadVertexBufferPtr->TexIndex = textureSlot;
		m_Data->QuadVertexBufferPtr++;

		
		m_Data->QuadVertexBufferPtr->Position = (
			object2d.Position +
			camRightWS * (QuadVertexPositions[1].x) * object2d.Size.x +
			camUpWS *    (QuadVertexPositions[1].y) * object2d.Size.y
		);
		m_Data->QuadVertexBufferPtr->Color = object2d.Color;
		m_Data->QuadVertexBufferPtr->TexCoord = { 0.0f, 1.0f };
		m_Data->QuadVertexBufferPtr->TexIndex = textureSlot;
		m_Data->QuadVertexBufferPtr++;

		
		m_Data->QuadVertexBufferPtr->Position = (
			object2d.Position +
			camRightWS * (QuadVertexPositions[2].x) * object2d.Size.x +
			camUpWS *    (QuadVertexPositions[2].y) * object2d.Size.y
		);
		m_Data->QuadVertexBufferPtr->Color = object2d.Color;
		m_Data->QuadVertexBufferPtr->TexCoord = { 1.0f, 1.0f };
		m_Data->QuadVertexBufferPtr->TexIndex = textureSlot;
		m_Data->QuadVertexBufferPtr++;


		m_Data->QuadVertexBufferPtr->Position = (
			object2d.Position +
			camRightWS * (QuadVertexPositions[3].x) * object2d.Size.x +
			camUpWS *    (QuadVertexPositions[3].y) * object2d.Size.y
		);
		m_Data->QuadVertexBufferPtr->Color = object2d.Color;
		m_Data->QuadVertexBufferPtr->TexCoord = { 1.0f, 0.0f };
		m_Data->QuadVertexBufferPtr->TexIndex = textureSlot;
		m_Data->QuadVertexBufferPtr++;

		m_Data->QuadIndexCount += 6;
		m_Data->QuadCount++;
	}

	void VulkanBatchRenderingPass::SubmitQuad(const RenderQueue::Object2D& object2d)
	{
		if (m_Data->QuadIndexCount >= MaxIndices)
		{
			FROST_CORE_ERROR("The maximum number of indices has been reached! The batch renderer cannot render more objects!");
			return;
		}
	}

	void VulkanBatchRenderingPass::OnRenderDebug()
	{

	}

	void VulkanBatchRenderingPass::OnResize(uint32_t width, uint32_t height)
	{
		BatchRendererInitData(width, height);

		Renderer::SubmitImageToOutputImageMap("FinalImage_With2D", [this]() -> Ref<Image2D>
		{
			uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
			return this->m_Data->BatchRendererRenderPass->GetColorAttachment(0, currentFrameIndex);
		});
	}

	void VulkanBatchRenderingPass::OnResizeLate(uint32_t width, uint32_t height)
	{

	}

	void VulkanBatchRenderingPass::ShutDown()
	{
		delete m_Data;
	}
}