#include "frostpch.h"
#include "VulkanBatchRenderingPass.h"

#include "Frost/Core/Application.h"
#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanCompositePass.h"
#include "Frost/Platform/Vulkan/VulkanContext.h"
#include "Frost/Platform/Vulkan/VulkanImage.h"
#include "Frost/Platform/Vulkan/VulkanBindlessAllocator.h"
#include "Frost/Platform/Vulkan/VulkanMaterial.h"
#include "Frost/Platform/Vulkan/VulkanPipeline.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanBufferDevice.h"
#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanGeometryPass.h"
#include "Frost/Platform/Vulkan/VulkanPipelineCompute.h"
#include "Frost/Platform/Vulkan/VulkanRenderer.h"
#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanPostFXPass.h"

#include "Frost/Renderer/UserInterface/MSDFData.h"

#include <codecvt>

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

		m_Data->BatchQuadRendererShader = Renderer::GetShaderLibrary()->Get("BatchRendererQuad");
		m_Data->BatchLineRendererShader = Renderer::GetShaderLibrary()->Get("BatchRendererLine");
		m_Data->RenderWireframeShader = Renderer::GetShaderLibrary()->Get("Wireframe");
		m_Data->RenderGridShader = Renderer::GetShaderLibrary()->Get("SceneGrid");
		m_Data->LineDetectionShader = Renderer::GetShaderLibrary()->Get("LineDetection");
		m_Data->GlowSelectedEntityShader = Renderer::GetShaderLibrary()->Get("EntityGlow");

		BatchRendererInitData(1600, 900);
		RenderWireframeInitData(1600, 900);
		RenderGridInitData(1600, 900);
		SelectEntityInitData(1600, 900);
		GlowSelectedEntityInitData(1600, 900);

		Renderer::SubmitImageToOutputImageMap("FinalImage_With2D", [this]() -> Ref<Image2D>
		{
			uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
			return this->m_Data->BatchRendererRenderPass->GetColorAttachment(0, currentFrameIndex);
		});
	}

	void VulkanBatchRenderingPass::BatchRendererInitData(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;
		uint64_t maxQuadsVerticies = Renderer::GetRendererConfig().Renderer2D.MaxQuads * 4;
		uint64_t maxQuadsIndices = Renderer::GetRendererConfig().Renderer2D.MaxQuads * 6;
		uint64_t maxTextVerticies = maxQuadsVerticies;
		uint64_t maxLinesVerticies = Renderer::GetRendererConfig().Renderer2D.MaxLines * 2;

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

		{

			// Batched Quads rendering pipeline
			BufferLayout bufferLayout = {
				{ "a_Position",  ShaderDataType::Float3 },
				{ "a_Color",     ShaderDataType::Float4 },
				{ "a_TexCoord",  ShaderDataType::Float2 },
				{ "a_TexIndex",  ShaderDataType::UInt },
			};
			bufferLayout.m_InputType = InputType::Vertex;
			Pipeline::CreateInfo pipelineCreateInfo{};
			pipelineCreateInfo.Shader = m_Data->BatchQuadRendererShader;
			pipelineCreateInfo.UseDepthTest = true;
			pipelineCreateInfo.UseDepthWrite = false;
			pipelineCreateInfo.RenderPass = m_Data->BatchRendererRenderPass;
			pipelineCreateInfo.VertexBufferLayout = bufferLayout;
			pipelineCreateInfo.Topology = PrimitiveTopology::Triangles;
			if (!m_Data->BatchQuadRendererPipeline)
				m_Data->BatchQuadRendererPipeline = Pipeline::Create(pipelineCreateInfo);
		}

		{
			// Batched Lines rendering pipeline
			BufferLayout bufferLayout = {
				{ "a_Position",  ShaderDataType::Float3 },
				{ "a_Color",     ShaderDataType::Float4 },
			};
			bufferLayout.m_InputType = InputType::Vertex;
			Pipeline::CreateInfo pipelineCreateInfo{};
			pipelineCreateInfo.Shader = m_Data->BatchLineRendererShader;
			pipelineCreateInfo.UseDepthTest = true;
			pipelineCreateInfo.UseDepthWrite = false;
			pipelineCreateInfo.RenderPass = m_Data->BatchRendererRenderPass;
			pipelineCreateInfo.VertexBufferLayout = bufferLayout;
			pipelineCreateInfo.Topology = PrimitiveTopology::Lines;
			if (!m_Data->BatchLineRendererPipeline)
				m_Data->BatchLineRendererPipeline = Pipeline::Create(pipelineCreateInfo);
		}
		
		m_Data->BatchRendererMaterial.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			m_Data->BatchRendererMaterial[i] = Material::Create(m_Data->BatchQuadRendererShader, "BatchQuadRendererMaterial");
		}

		/// Quads
		m_Data->QuadVertexBuffer.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			m_Data->QuadVertexBuffer[i] = BufferDevice::Create(maxQuadsVerticies * sizeof(QuadVertex), {BufferUsage::Vertex});
		}
		m_Data->QuadVertexBufferBase = new QuadVertex[maxQuadsVerticies];

		uint32_t* quadIndices = new uint32_t[maxQuadsIndices];
		uint32_t offset = 0;
		for (uint32_t i = 0; i < maxQuadsIndices; i += 6)
		{
			quadIndices[i + 0] = offset + 0;
			quadIndices[i + 1] = offset + 1;
			quadIndices[i + 2] = offset + 2;

			quadIndices[i + 3] = offset + 2;
			quadIndices[i + 4] = offset + 3;
			quadIndices[i + 5] = offset + 0;

			offset += 4;
		}
		m_Data->QuadIndexBuffer = IndexBuffer::Create(quadIndices, sizeof(uint32_t) * maxQuadsIndices);
		delete[] quadIndices;


		/// Lines
		m_Data->LineVertexBuffer.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			m_Data->LineVertexBuffer[i] = BufferDevice::Create(maxLinesVerticies * sizeof(LineVertex), { BufferUsage::Vertex });
		}
		m_Data->LineVertexBufferBase = new LineVertex[maxLinesVerticies];
		m_Data->LineVertexBufferPtr = m_Data->LineVertexBufferBase;

		/// Text
		m_Data->TextVertexBuffer.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			m_Data->TextVertexBuffer[i] = BufferDevice::Create(maxTextVerticies * sizeof(TextVertex), { BufferUsage::Vertex });
		}
		m_Data->TextVertexBufferBase = new TextVertex[maxTextVerticies];
		m_Data->TextVertexBufferPtr = m_Data->TextVertexBufferBase;

	}

	void VulkanBatchRenderingPass::RenderWireframeInitData(uint32_t width, uint32_t height)
	{
		// Composite pipeline
		BufferLayout bufferLayout = {
			{ "a_Position",      ShaderDataType::Float3 },
			{ "a_TexCoord",	     ShaderDataType::Float2 },
			{ "a_Normal",	     ShaderDataType::Float3 },
			{ "a_Tangent",	     ShaderDataType::Float3 },
			{ "a_Bitangent",     ShaderDataType::Float3 },
			{ "a_MaterialIndex", ShaderDataType::Float }
		};
		bufferLayout.m_InputType = InputType::Vertex;
		Pipeline::CreateInfo pipelineCreateInfo{};
		pipelineCreateInfo.Shader = m_Data->RenderWireframeShader;
		pipelineCreateInfo.UseDepthTest = true;
		pipelineCreateInfo.UseDepthWrite = false;
		pipelineCreateInfo.RenderPass = m_Data->BatchRendererRenderPass;
		pipelineCreateInfo.VertexBufferLayout = bufferLayout;
		pipelineCreateInfo.Topology = PrimitiveTopology::Triangles;
		pipelineCreateInfo.Wireframe = true;
		pipelineCreateInfo.LineWidth = 2.0f;
		if (!m_Data->RenderWireframePipeline)
			m_Data->RenderWireframePipeline = Pipeline::Create(pipelineCreateInfo);
	}

	void VulkanBatchRenderingPass::RenderGridInitData(uint32_t width, uint32_t height)
	{
		// Composite pipeline
		BufferLayout bufferLayout = {
			{ "a_Position",      ShaderDataType::Float3 },
			{ "a_TexCoord",	     ShaderDataType::Float2 },
		};
		bufferLayout.m_InputType = InputType::Vertex;
		Pipeline::CreateInfo pipelineCreateInfo{};
		pipelineCreateInfo.Shader = m_Data->RenderGridShader;
		pipelineCreateInfo.UseDepthTest = true;
		pipelineCreateInfo.UseDepthWrite = false;
		pipelineCreateInfo.RenderPass = m_Data->BatchRendererRenderPass;
		pipelineCreateInfo.VertexBufferLayout = bufferLayout;
		pipelineCreateInfo.Topology = PrimitiveTopology::Triangles;
		//pipelineCreateInfo.Wireframe = true;
		pipelineCreateInfo.LineWidth = 2.0f;
		if (!m_Data->RenderGridPipeline)
			m_Data->RenderGridPipeline = Pipeline::Create(pipelineCreateInfo);

		const float GridVertexPositions[] = {
			-0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
			 0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
			 0.5f,  0.5f, 0.0f, 0.0f, 1.0f,
			-0.5f,  0.5f, 0.0f, 1.0f, 1.0f
		};

		const uint32_t GridIndices[] = {
			0, 1, 2, 2, 3, 0
		};

		m_Data->GridVertexBuffer = VertexBuffer::Create((void*)&GridVertexPositions[0], sizeof(GridVertexPositions));
		m_Data->GridIndexBuffer = IndexBuffer::Create((void*)&GridIndices[0], sizeof(GridIndices));
	}

	void VulkanBatchRenderingPass::SelectEntityInitData(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;


		ImageSpecification imageSpec{};
		imageSpec.Width = width;
		imageSpec.Height = height;
		imageSpec.Format = ImageFormat::R32I;
		imageSpec.Usage = ImageUsage::ReadOnly;
		imageSpec.Tiling = ImageTiling::Linear; // Linear because we need to read it from CPU
		imageSpec.MemoryProperties = ImageMemoryProperties::GPU_TO_CPU;
		imageSpec.UseMipChain = false;
		imageSpec.MutableFormat = true;

		m_Data->EntityIDTexture_CPU.resize(framesInFlight);
		for (int i = 0; i < framesInFlight; i++)
		{
			m_Data->EntityIDTexture_CPU[i] = Image2D::Create(imageSpec);
		}

		m_Data->ViewportSize = { width, height };
	}

	void VulkanBatchRenderingPass::GlowSelectedEntityInitData(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;

		{
			ComputePipeline::CreateInfo computePipelineCreateInfo{};
			computePipelineCreateInfo.Shader = m_Data->LineDetectionShader;

			// Create Gaussian Blur Pipeline
			if (!m_Data->LineDetectionPipeline)
				m_Data->LineDetectionPipeline = ComputePipeline::Create(computePipelineCreateInfo);

			// Create Glow Selected Entity Pipeline
			computePipelineCreateInfo.Shader = m_Data->GlowSelectedEntityShader;
			if (!m_Data->GlowSelectedEntityPipeline)
				m_Data->GlowSelectedEntityPipeline = ComputePipeline::Create(computePipelineCreateInfo);
		}

		m_Data->GlowSelectedEntityTexturePing.resize(framesInFlight);
		m_Data->GlowSelectedEntityTexturePong.resize(framesInFlight);
		m_Data->GlowSelectedEntityMaterial.resize(framesInFlight);
		for(uint32_t i = 0; i < framesInFlight; i++)
		{
			ImageSpecification imageSpec{};
			imageSpec.Format = ImageFormat::RGBA8;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.Sampler.SamplerFilter = ImageFilter::Linear;
			imageSpec.Sampler.SamplerWrap = ImageWrap::ClampToEdge;
			imageSpec.Width = width;
			imageSpec.Height = height;
			imageSpec.UseMipChain = false;
			m_Data->GlowSelectedEntityTexturePing[i] = Image2D::Create(imageSpec);
			m_Data->GlowSelectedEntityTexturePong[i] = Image2D::Create(imageSpec);

			if (!m_Data->GlowSelectedEntityMaterial[i])
				m_Data->GlowSelectedEntityMaterial[i] = Material::Create(m_Data->GlowSelectedEntityShader, "GlowSelectedEntityMaterial");

			auto vulkanMaterial = m_Data->GlowSelectedEntityMaterial[i].As<VulkanMaterial>();

			Ref<Image2D> entityIDTexture_GeometryPass = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->GeometryRenderPass->GetColorAttachment(4, i);

			vulkanMaterial->Set("u_CaptureEntityTexture", m_Data->GlowSelectedEntityTexturePing[i]);
			vulkanMaterial->Set("u_FinalTexture", m_Data->BatchRendererRenderPass->GetColorAttachment(0, i));
			vulkanMaterial->Set("u_GaussianBlurTexture", m_Data->GlowSelectedEntityTexturePong[i]);
			vulkanMaterial->Set("u_EntityIDTexture", entityIDTexture_GeometryPass);

			vulkanMaterial->UpdateVulkanDescriptorIfNeeded();
		}

		m_Data->LineDetectionMaterial.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			if (!m_Data->LineDetectionMaterial[i])
				m_Data->LineDetectionMaterial[i] = Material::Create(m_Data->LineDetectionShader, "LineDetectionMaterial");

			auto vulkanMaterial = m_Data->LineDetectionMaterial[i].As<VulkanMaterial>();

			vulkanMaterial->Set("i_SrcImage", m_Data->GlowSelectedEntityTexturePing[i]);
			vulkanMaterial->Set("o_DstImage", m_Data->GlowSelectedEntityTexturePong[i]);

			vulkanMaterial->UpdateVulkanDescriptorIfNeeded();
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

		m_Data->LineVertexCount = 0;
		m_Data->LineVertexBufferPtr = m_Data->LineVertexBufferBase;

		m_Data->TextIndexCount = 0;
		m_Data->TextCount = 0;
		m_Data->TextVertexBufferPtr = m_Data->TextVertexBufferBase;


		for (const auto& object2d : renderQueue.m_BatchRendererData)
		{
			switch (object2d.Type)
			{
				case RenderQueue::Object2D::ObjectType::Billboard: SubmitBillboard(renderQueue, object2d); break;
				case RenderQueue::Object2D::ObjectType::Quad:      SubmitQuad(object2d); break;
				case RenderQueue::Object2D::ObjectType::Line:      SubmitLine(object2d); break;
				default: FROST_CORE_ERROR("2D Object type is unknown!");  break;
			}
		}

		for (const auto& textObject2d : renderQueue.m_TextRendererData)
		{
			SubmitText(textObject2d);
		}

		m_Data->QuadVertexBuffer[currentFrameIndex]->SetData(m_Data->QuadCount * 4 * sizeof(QuadVertex), m_Data->QuadVertexBufferBase);
		m_Data->LineVertexBuffer[currentFrameIndex]->SetData(m_Data->LineVertexCount * sizeof(LineVertex), m_Data->LineVertexBufferBase);
		m_Data->TextVertexBuffer[currentFrameIndex]->SetData(m_Data->TextCount * 4 * sizeof(TextVertex), m_Data->TextVertexBufferBase);

		BatchRendererUpdate(renderQueue);
		SelectEntityUpdate(renderQueue);
	}

	void VulkanBatchRenderingPass::BatchRendererUpdate(const RenderQueue& renderQueue)
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
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
		
		
		m_Data->BatchRendererRenderPass->Bind();

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


		{
			////////////////// Render the batched quads/text //////////////////////////////
			Ref<VulkanPipeline> batchQuadRendererPipeline = m_Data->BatchQuadRendererPipeline.As<VulkanPipeline>();
			Ref<VulkanMaterial> batchRendererMaterial = m_Data->BatchRendererMaterial[currentFrameIndex].As<VulkanMaterial>();

			m_BatchQuadRenderPushConstant.UseAtlas = false;
			m_BatchQuadRenderPushConstant.ViewProjectionMatrix = viewProj;

			batchQuadRendererPipeline->Bind();
			batchQuadRendererPipeline->BindVulkanPushConstant("u_PushConstant", &m_BatchQuadRenderPushConstant);

			// TODO: This is so bad, pls fix this
			VkPipelineLayout pipelineLayout = batchQuadRendererPipeline->GetVulkanPipelineLayout();
			Vector<VkDescriptorSet> descriptorSets = batchRendererMaterial->GetVulkanDescriptorSets();
			descriptorSets[1] = VulkanBindlessAllocator::GetVulkanDescriptorSet(currentFrameIndex);
			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);

			m_Data->QuadIndexBuffer->Bind();
			Ref<VulkanBufferDevice> batchQuadVertexBuffer = m_Data->QuadVertexBuffer[currentFrameIndex].As<VulkanBufferDevice>();

			//  Render Batched Quads
			uint64_t offset = 0;
			VkBuffer batchQuadVertexBufferInternal = batchQuadVertexBuffer->GetVulkanBuffer();
			vkCmdBindVertexBuffers(cmdBuf, 0, 1, &batchQuadVertexBufferInternal, &offset);

			vkCmdDrawIndexed(cmdBuf, m_Data->QuadIndexCount, 1, 0, 0, 0);




			m_BatchQuadRenderPushConstant.UseAtlas = true;
			batchQuadRendererPipeline->BindVulkanPushConstant("u_PushConstant", &m_BatchQuadRenderPushConstant);

			// Render Batched Text Quads
			Ref<VulkanBufferDevice> batchTextVertexBuffer = m_Data->TextVertexBuffer[currentFrameIndex].As<VulkanBufferDevice>();

			VkBuffer batchTextVertexBufferInternal = batchTextVertexBuffer->GetVulkanBuffer();
			vkCmdBindVertexBuffers(cmdBuf, 0, 1, &batchTextVertexBufferInternal, &offset);

			vkCmdDrawIndexed(cmdBuf, m_Data->TextIndexCount, 1, 0, 0, 0);

		}

		{
			////////////////// Render the batched lines //////////////////////////////
			Ref<VulkanPipeline> batchLineRendererPipeline = m_Data->BatchLineRendererPipeline.As<VulkanPipeline>();
			Ref<VulkanBufferDevice> batchVertexBuffer = m_Data->LineVertexBuffer[currentFrameIndex].As<VulkanBufferDevice>();

			batchLineRendererPipeline->Bind();
			batchLineRendererPipeline->BindVulkanPushConstant("u_PushConstant", &viewProj);

			uint64_t offset = 0;
			VkBuffer batchVertexBufferInternal = batchVertexBuffer->GetVulkanBuffer();
			vkCmdBindVertexBuffers(cmdBuf, 0, 1, &batchVertexBufferInternal, &offset);

			vkCmdDraw(cmdBuf, m_Data->LineVertexCount, 1, 0, 0);
		}

		////////////////// Render Wireframed mesh //////////////////////////////
		RenderWireframeUpdate(renderQueue);

		////////////////// Render Grid //////////////////////////////
		RenderGridUpdate(renderQueue);

		m_Data->BatchRendererRenderPass->Unbind();

		////////////////// Glow Seleceted mesh(Compute) //////////////////////////////
		GlowSelectedEntityUpdate(renderQueue);
	}

	void VulkanBatchRenderingPass::SubmitBillboard(const RenderQueue& renderQueue, const RenderQueue::Object2D& object2d)
	{
		if (m_Data->QuadIndexCount >= (Renderer::GetRendererConfig().Renderer2D.MaxQuads * 6))
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
			textureSlot = BindlessAllocator::GetWhiteTextureID();
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
		if (m_Data->QuadIndexCount >= (Renderer::GetRendererConfig().Renderer2D.MaxQuads * 6))
		{
			FROST_CORE_ERROR("The maximum number of indices has been reached! The batch renderer cannot render more objects!");
			return;
		}
	}

	// From https://stackoverflow.com/questions/31302506/stdu32string-conversion-to-from-stdstring-and-stdu16string
	static std::u32string To_UTF32(const std::string& s)
	{
		std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> conv;
		return conv.from_bytes(s);
	}

	static bool NextLine(int index, const std::vector<int>& lines)
	{
		for (int line : lines)
		{
			if (line == index)
				return true;
		}
		return false;
	}

	void VulkanBatchRenderingPass::SubmitText(const RenderQueue::TextObject2D& textObject2D)
	{
		if (textObject2D.String.empty())
			return;

		std::u32string utf32string = To_UTF32(textObject2D.String);

		Ref<Texture2D> fontAtlas = textObject2D.Font->GetFontAtlas();
		FROST_ASSERT_INTERNAL(fontAtlas);

		uint32_t atlasTextureSlot;
		Ref<VulkanImage2D> texture = fontAtlas->GetImage2D().As<VulkanImage2D>();
		if (m_BindlessAllocatedTextures.find(texture->GetVulkanImage()) == m_BindlessAllocatedTextures.end())
		{
			atlasTextureSlot = VulkanBindlessAllocator::AddTexture(fontAtlas);
			m_BindlessAllocatedTextures[texture->GetVulkanImage()] = atlasTextureSlot;
		}
		else
		{
			atlasTextureSlot = m_BindlessAllocatedTextures[texture->GetVulkanImage()];
		}


		auto& fontGeometry = textObject2D.Font->GetMSDFData()->FontGeometry;
		const auto& metrics = fontGeometry.getMetrics();

		
		// Computing the new lines in the text
		Vector<int> nextLines;
		{
			double xOffset = 0.0;
			double fsScale = 1 / (metrics.ascenderY - metrics.descenderY);
			double yOffset = -fsScale * metrics.ascenderY;
			int lastSpace = -1;

			for (int i = 0; i < utf32string.size(); i++)
			{
				char32_t character = utf32string[i];
				if (character == '\n')
				{
					xOffset = 0;
					yOffset -= fsScale * metrics.lineHeight + textObject2D.LineHeightOffset;
					continue;
				}

				auto glyph = fontGeometry.getGlyph(character);
				if (!glyph)
				{
					glyph = fontGeometry.getGlyph('?');
					if (!glyph)
					{
						continue;
					}
				}

				if (character != ' ')
				{
					// Calculate geometry for the glyph
					double pl, pb, pr, pt;
					glyph->getQuadPlaneBounds(pl, pb, pr, pt);
					glm::vec2 quadMin((float)pl, (float)pb);
					glm::vec2 quadMax((float)pr, (float)pt);

					quadMin *= fsScale;
					quadMax *= fsScale;
					
					quadMin += glm::vec2(xOffset, yOffset);
					quadMax += glm::vec2(xOffset, yOffset);

					if (quadMax.x > textObject2D.MaxWidth && lastSpace != -1)
					{
						i = lastSpace;
						nextLines.emplace_back(lastSpace);
						lastSpace = -1;
						
						xOffset = 0;
						yOffset -= fsScale * metrics.lineHeight + textObject2D.LineHeightOffset;
					}
				}
				else
				{
					lastSpace = i;
				}

				double advance = glyph->getAdvance();
				fontGeometry.getAdvance(advance, character, utf32string[i + 1]);
				xOffset += fsScale * advance + textObject2D.KerningOffset;
			}
		}

		{
			double xOffset = 0.0;
			double fsScale = 1 / (metrics.ascenderY - metrics.descenderY);
			double yOffset = 0.0;
			for (int i = 0; i < utf32string.size(); i++)
			{
				char32_t character = utf32string[i];
				if (character == '\n' || NextLine(i, nextLines))
				{
					xOffset = 0;
					yOffset -= fsScale * metrics.lineHeight + textObject2D.LineHeightOffset;
					continue;
				}

				auto glyph = fontGeometry.getGlyph(character);
				if (!glyph)
				{
					glyph = fontGeometry.getGlyph('?');
					if (!glyph)
					{
						continue;
					}
				}

				// Atlas Quad Bounds
				double al, ab, ar, at;
				glyph->getQuadAtlasBounds(al, ab, ar, at);

				double pl, pb, pr, pt;
				glyph->getQuadPlaneBounds(pl, pb, pr, pt);

				pl = (pl * fsScale) + xOffset;
				pb = (pb * fsScale) + yOffset;
				pr = (pr * fsScale) + xOffset;
				pt = (pt * fsScale) + yOffset;

				double texelWidth = 1.0 / fontAtlas->GetWidth();
				double texelHeight = 1.0 / fontAtlas->GetHeight();

				al *= texelWidth;
				ab *= texelHeight;
				ar *= texelWidth;
				at *= texelHeight;

				m_Data->TextVertexBufferPtr->Position = textObject2D.Transform * glm::vec4(pl, pb, 0.0f, 1.0f);
				m_Data->TextVertexBufferPtr->Color = textObject2D.Color;
				m_Data->TextVertexBufferPtr->TexCoord = { al, ab };
				m_Data->TextVertexBufferPtr->TexIndex = atlasTextureSlot;
				m_Data->TextVertexBufferPtr++;
				
				m_Data->TextVertexBufferPtr->Position = textObject2D.Transform * glm::vec4(pl, pt, 0.0f, 1.0f);
				m_Data->TextVertexBufferPtr->Color = textObject2D.Color;
				m_Data->TextVertexBufferPtr->TexCoord = { al, at };
				m_Data->TextVertexBufferPtr->TexIndex = atlasTextureSlot;
				m_Data->TextVertexBufferPtr++;
				
				m_Data->TextVertexBufferPtr->Position = textObject2D.Transform * glm::vec4(pr, pt, 0.0f, 1.0f);
				m_Data->TextVertexBufferPtr->Color = textObject2D.Color;
				m_Data->TextVertexBufferPtr->TexCoord = { ar, at };
				m_Data->TextVertexBufferPtr->TexIndex = atlasTextureSlot;
				m_Data->TextVertexBufferPtr++;
				
				m_Data->TextVertexBufferPtr->Position = textObject2D.Transform * glm::vec4(pr, pb, 0.0f, 1.0f);
				m_Data->TextVertexBufferPtr->Color = textObject2D.Color;
				m_Data->TextVertexBufferPtr->TexCoord = { ar, ab };
				m_Data->TextVertexBufferPtr->TexIndex = atlasTextureSlot;
				m_Data->TextVertexBufferPtr++;

				m_Data->TextIndexCount += 6;
				m_Data->TextCount++;

				double advance = glyph->getAdvance();
				fontGeometry.getAdvance(advance, character, utf32string[i + 1]);
				xOffset += fsScale * advance + textObject2D.KerningOffset;

			}
		}
	}

	void VulkanBatchRenderingPass::SubmitLine(const RenderQueue::Object2D& object2d)
	{
		if (m_Data->QuadIndexCount >= (Renderer::GetRendererConfig().Renderer2D.MaxLines * 2))
		{
			FROST_CORE_ERROR("The maximum number of verticies has been reached! The batch renderer cannot render more objects!");
			return;
		}

		m_Data->LineVertexBufferPtr->Position = object2d.Position;
		m_Data->LineVertexBufferPtr->Color = object2d.Color;
		m_Data->LineVertexBufferPtr++;

		m_Data->LineVertexBufferPtr->Position = object2d.Position_SecondPos_Line;
		m_Data->LineVertexBufferPtr->Color = object2d.Color;
		m_Data->LineVertexBufferPtr++;

		m_Data->LineVertexCount += 2;
	}

	void VulkanBatchRenderingPass::RenderWireframeUpdate(const RenderQueue& renderQueue)
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		Ref<VulkanPipeline> renderWireframePipeline = m_Data->RenderWireframePipeline.As<VulkanPipeline>();

		renderWireframePipeline->Bind();

		for (uint32_t i = 0; i < renderQueue.m_WireframeRenderData.size(); i++)
		{


			// Get the mesh
			const Ref<MeshAsset>& mesh = renderQueue.m_WireframeRenderData[i].Mesh;

			if (!mesh) continue;

			const Vector<Submesh>& submeshes = mesh->GetSubMeshes();

			// Bind the vertex and index buffer
			mesh->GetIndexBuffer()->Bind();
			mesh->GetVertexBuffer()->Bind();

			for (const Submesh& submesh : submeshes)
			{
				m_WireframePushConstant.WorldSpaceMatrix = renderQueue.m_Camera->GetViewProjectionVK() * (renderQueue.m_WireframeRenderData[i].Transform * submesh.Transform);
				m_WireframePushConstant.Color = renderQueue.m_WireframeRenderData[i].Color;

				renderWireframePipeline->BindVulkanPushConstant("u_PushConstant", &m_WireframePushConstant);

				vkCmdSetLineWidth(cmdBuf, renderQueue.m_WireframeRenderData[i].LineWidth);

				vkCmdDrawIndexed(cmdBuf, submesh.IndexCount, 1, submesh.BaseIndex, submesh.BaseVertex, 0);
			}
		}
	}

	void VulkanBatchRenderingPass::RenderGridUpdate(const RenderQueue& renderQueue)
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		Ref<VulkanPipeline> renderGridPipeline = m_Data->RenderGridPipeline.As<VulkanPipeline>();

		renderGridPipeline->Bind();

		m_Data->GridVertexBuffer->Bind();
		m_Data->GridIndexBuffer->Bind();

		m_GridPushConstant.WorldSpaceMatrix = renderQueue.m_Camera->GetViewProjectionVK() * glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(m_GridPushConstant.GridScale));
		renderGridPipeline->BindVulkanPushConstant("u_PushConstant", &m_GridPushConstant);

		vkCmdDrawIndexed(cmdBuf, 6, 1, 0, 0, 0);
	}

	void VulkanBatchRenderingPass::SelectEntityUpdate(const RenderQueue& renderQueue)
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		Ref<VulkanImage2D> entityIDTexture_GeometryPass = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->GeometryRenderPass->GetColorAttachment(4, currentFrameIndex).As<VulkanImage2D>();
		Ref<VulkanImage2D> entityIDTexture_CPU = m_Data->EntityIDTexture_CPU[currentFrameIndex].As<VulkanImage2D>();

		entityIDTexture_CPU->CopyImage(cmdBuf, entityIDTexture_GeometryPass.As<Image2D>());
	}

	uint32_t VulkanBatchRenderingPass::ReadPixelFromTextureEntityID(uint32_t x, uint32_t y)
	{
#if 0
		// Recording a temporary commandbuffer for transitioning
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetCurrentDevice()->AllocateCommandBuffer(RenderQueueType::Graphics, true);
		//VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);


		//uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();

		Ref<VulkanImage2D> entityIDTexture_GeometryPass = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->GeometryRenderPass->GetColorAttachment(4, currentFrameIndex).As<VulkanImage2D>();
		Ref<VulkanImage2D> entityIDTexture_CPU = m_Data->EntityIDTexture_CPU[currentFrameIndex].As<VulkanImage2D>();

		//entityIDTexture_GeometryPass->TransitionLayout(cmdBuf, entityIDTexture_GeometryPass->GetVulkanImageLayout(), VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

		entityIDTexture_CPU->CopyImage(cmdBuf, entityIDTexture_GeometryPass.As<Image2D>());
#endif


		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();

		Ref<VulkanImage2D> entityIDTexture_GeometryPass = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->GeometryRenderPass->GetColorAttachment(4, currentFrameIndex).As<VulkanImage2D>();
		Ref<VulkanImage2D> entityIDTexture_CPU = m_Data->EntityIDTexture_CPU[currentFrameIndex].As<VulkanImage2D>();

		// Get layout of the image (including row pitch)
		VkImageSubresource subResource{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
		VkSubresourceLayout subResourceLayout;
		vkGetImageSubresourceLayout(device, entityIDTexture_CPU->GetVulkanImage(), &subResource, &subResourceLayout);

		entityIDTexture_CPU->MapMemory((void**)&m_Data->TextureDataCPU);
		uint32_t* data = (uint32_t*)m_Data->TextureDataCPU;
		data += subResourceLayout.offset;

		uint32_t result = data[
			(y * (subResourceLayout.rowPitch / sizeof(uint32_t))) + x
		];
		entityIDTexture_CPU->UnMapMemory();

#if 0
		// Ending the temporary commandbuffer for transitioning
		VulkanContext::GetCurrentDevice()->FlushCommandBuffer(cmdBuf);
#endif

		return result;
	}

	void VulkanBatchRenderingPass::GlowSelectedEntityUpdate(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		auto vulkanPipeline = m_Data->GlowSelectedEntityPipeline.As<VulkanComputePipeline>();
		auto vulkanDescriptor = m_Data->GlowSelectedEntityMaterial[currentFrameIndex].As<VulkanMaterial>();
		auto vulkanCaputedEntityTex = m_Data->GlowSelectedEntityTexturePing[currentFrameIndex].As<VulkanImage2D>();

		vulkanDescriptor->Bind(cmdBuf, m_Data->GlowSelectedEntityPipeline);

		m_GlowSelectedEntityPushConstant.CosTime = Application::Get().GetWindow().GetTime();
		m_GlowSelectedEntityPushConstant.FilterMode = 0.0f;
		m_GlowSelectedEntityPushConstant.SelectedEntityID = renderQueue.GetActiveEntity();
		vulkanPipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &m_GlowSelectedEntityPushConstant);

		float width = renderQueue.ViewPortWidth;
		float height = renderQueue.ViewPortHeight;

		uint32_t groupX = std::ceil(width / 32.0f);
		uint32_t groupY = std::ceil(height / 32.0f);
		vulkanPipeline->Dispatch(cmdBuf, groupX, groupY, 1);

		// Barrier
		VkImageSubresourceRange imageSubrange{};
		imageSubrange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageSubrange.baseArrayLayer = 0;
		imageSubrange.baseMipLevel = 0;
		imageSubrange.layerCount = 1;
		imageSubrange.levelCount = 1;

		// Set a barrier for the prefiltered thresholded texture
		Utils::InsertImageMemoryBarrier(cmdBuf, vulkanCaputedEntityTex->GetVulkanImage(),
			VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
			vulkanCaputedEntityTex->GetVulkanImageLayout(), vulkanCaputedEntityTex->GetVulkanImageLayout(),
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			imageSubrange
		);



		BlurSelectedEntityUpdate(renderQueue);



		vulkanDescriptor->Bind(cmdBuf, m_Data->GlowSelectedEntityPipeline);

		//m_GlowSelectedEntityPushConstant.CosTime = 0.0f;
		m_GlowSelectedEntityPushConstant.FilterMode = 1.0f;
		vulkanPipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &m_GlowSelectedEntityPushConstant);

		vulkanPipeline->Dispatch(cmdBuf, groupX, groupY, 1);


	}

	void VulkanBatchRenderingPass::BlurSelectedEntityUpdate(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		auto vulkanPipeline = m_Data->LineDetectionPipeline.As<VulkanComputePipeline>();
		auto vulkanDescriptor = m_Data->LineDetectionMaterial[currentFrameIndex].As<VulkanMaterial>();
		auto vulkanLineDetectionTex = m_Data->GlowSelectedEntityTexturePong[currentFrameIndex].As<VulkanImage2D>();

		float workGroupSize = 32.0f;
		float width = renderQueue.ViewPortWidth;
		float height = renderQueue.ViewPortHeight;

		vulkanDescriptor->Bind(cmdBuf, m_Data->LineDetectionPipeline);

		uint32_t groupX = std::ceil(width / workGroupSize);
		uint32_t groupY = std::ceil(height / workGroupSize);
		vulkanPipeline->Dispatch(cmdBuf, groupX, groupY, 1);


		// Barrier
		VkImageSubresourceRange imageSubrange{};
		imageSubrange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageSubrange.baseArrayLayer = 0;
		imageSubrange.baseMipLevel = 0;
		imageSubrange.layerCount = 1;
		imageSubrange.levelCount = 1;

		// Set a barrier for the prefiltered thresholded texture
		Utils::InsertImageMemoryBarrier(cmdBuf, vulkanLineDetectionTex->GetVulkanImage(),
			VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
			vulkanLineDetectionTex->GetVulkanImageLayout(), vulkanLineDetectionTex->GetVulkanImageLayout(),
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			imageSubrange
		);
#if 0

#define GAUSSIAN_MODE_ONE_PASS 0.0f
#define GAUSSIAN_MODE_VERTICAL_PASS 1.0f
#define GAUSSIAN_MODE_HORIZONTAL_PASS 2.0f

		glm::vec4 pushConstant = { 0.0f, 0.0f, 0.0f, 0.0f };

		uint32_t gaussianPasses = 5.0f;
		float workGroupSize = 32.0f;
		bool horizontalPass = true;

		Ref<VulkanImage2D> inputTexture = nullptr;
		Ref<VulkanImage2D> outputTexture = nullptr;

		for (uint32_t i = 0; i < 1; i++)
		{
			if (horizontalPass)
			{
				inputTexture = m_Data->GlowSelectedEntityTexturePing[currentFrameIndex].As<VulkanImage2D>();
				outputTexture = m_Data->GlowSelectedEntityTexturePong[currentFrameIndex].As<VulkanImage2D>();
				pushConstant.w = 3.0;
			}
			else
			{
				inputTexture = m_Data->GlowSelectedEntityTexturePong[currentFrameIndex].As<VulkanImage2D>();
				outputTexture = m_Data->GlowSelectedEntityTexturePing[currentFrameIndex].As<VulkanImage2D>();
				pushConstant.w = 3.0;
			}
			horizontalPass = !horizontalPass;


			VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = &vulkanDescriptor->GetVulkanDescriptorLayout()[0];
			VkDescriptorSet descriptorSet = VulkanRenderer::AllocateDescriptorSet(allocInfo);

			// Input texture
			{
				auto inputImageInfo = inputTexture->GetVulkanDescriptorInfo(DescriptorImageType::Sampled);

				VkWriteDescriptorSet wds{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				wds.dstBinding = 0;
				wds.dstArrayElement = 0;
				wds.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				wds.pImageInfo = &inputImageInfo;
				wds.descriptorCount = 1;
				wds.dstSet = descriptorSet;

				vkUpdateDescriptorSets(device, 1, &wds, 0, nullptr);
			}

			// Output texture
			{
				auto outputImageInfo = outputTexture->GetVulkanDescriptorInfo(DescriptorImageType::Storage);

				VkWriteDescriptorSet wds{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				wds.dstBinding = 1;
				wds.dstArrayElement = 0;
				wds.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				wds.pImageInfo = &outputImageInfo;
				wds.descriptorCount = 1;
				wds.dstSet = descriptorSet;

				vkUpdateDescriptorSets(device, 1, &wds, 0, nullptr);
			}

			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, vulkanPipeline->GetVulkanPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);

			vulkanPipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &pushConstant);

			uint32_t width = renderQueue.ViewPortWidth;
			uint32_t height = renderQueue.ViewPortHeight;

			uint32_t workGroupsX = std::ceil(width / workGroupSize);
			uint32_t workGroupsY = std::ceil(height / workGroupSize);
			vulkanPipeline->Dispatch(cmdBuf, workGroupsX, workGroupsY, 1);

			VkImageSubresourceRange imageSubrange{};
			imageSubrange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageSubrange.baseArrayLayer = 0;
			imageSubrange.baseMipLevel = 0;
			imageSubrange.layerCount = 1;
			imageSubrange.levelCount = 1;

			// Set a barrier for the prefiltered thresholded texture
			Utils::InsertImageMemoryBarrier(cmdBuf, outputTexture->GetVulkanImage(),
				VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
				outputTexture->GetVulkanImageLayout(), outputTexture->GetVulkanImageLayout(),
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				imageSubrange
			);
			
		}

#endif
	}

	void VulkanBatchRenderingPass::OnRenderDebug()
	{

	}

	void VulkanBatchRenderingPass::OnResize(uint32_t width, uint32_t height)
	{
		BatchRendererInitData(width, height);
		SelectEntityInitData(width, height);
		GlowSelectedEntityInitData(width, height);

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