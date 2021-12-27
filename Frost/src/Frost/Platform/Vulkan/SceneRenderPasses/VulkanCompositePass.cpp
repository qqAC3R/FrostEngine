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

		m_Data->CompositeShader = Renderer::GetShaderLibrary()->Get("PBRDeffered");
		m_Data->SkyboxShader = Renderer::GetShaderLibrary()->Get("RenderSkybox");

		RenderPassSpecification renderPassSpec =
		{
			1600, 900, 3,
			{
				// Color Attachment
				{
					FramebufferTextureFormat::RGBA16F, ImageUsage::Storage,
					OperationLoad::Clear,  OperationStore::Store,
					OperationLoad::Load,   OperationStore::DontCare,
				},
				// Depth Attachment
				{
					FramebufferTextureFormat::DepthStencil, ImageUsage::DepthStencil,
					OperationLoad::Load,        OperationStore::Store,
					OperationLoad::DontCare,    OperationStore::DontCare,
				}
			}
		};
		m_Data->RenderPass = RenderPass::Create(renderPassSpec);


		{

			// Skybox pipeline/descriptors
			BufferLayout bufferLayout = {
				{ "a_Position",      ShaderDataType::Float3 },
			};
			Pipeline::CreateInfo pipelineCreateInfo{};
			pipelineCreateInfo.Shader = m_Data->SkyboxShader;
			pipelineCreateInfo.RenderPass = m_Data->RenderPass;
			pipelineCreateInfo.VertexBufferLayout = bufferLayout;
			pipelineCreateInfo.UseDepthTest = true;
			pipelineCreateInfo.UseDepthWrite = true;
			pipelineCreateInfo.Topology = PrimitiveTopology::Triangles;
			pipelineCreateInfo.DepthCompareOperation = DepthCompare::LessOrEqual;
			m_Data->SkyboxPipeline = Pipeline::Create(pipelineCreateInfo);
			m_Data->SkyboxDescriptor = Material::Create(m_Data->SkyboxShader);

			auto envCubeMap = Renderer::GetSceneEnvironment()->GetPrefilteredMap();
			m_Data->SkyboxDescriptor->Set("u_EnvTexture", envCubeMap);
			m_Data->SkyboxDescriptor->Set("CameraData.Gamma", 2.2f);
			m_Data->SkyboxDescriptor->Set("CameraData.Exposure", 0.1f);
			m_Data->SkyboxDescriptor->Set("CameraData.Lod", 3.0f);


			// Skybox vertex buffer
			float vertices[] = {
					-1.0f, -1.0f, -1.0f,
					 1.0f,  1.0f, -1.0f,
					 1.0f, -1.0f, -1.0f,
					 1.0f,  1.0f, -1.0f,
					-1.0f, -1.0f, -1.0f,
					-1.0f,  1.0f, -1.0f,
					// front face		
					-1.0f, -1.0f,  1.0f,
					 1.0f, -1.0f,  1.0f,
					 1.0f,  1.0f,  1.0f,
					 1.0f,  1.0f,  1.0f,
					-1.0f,  1.0f,  1.0f,
					-1.0f, -1.0f,  1.0f,
					// left face		
					-1.0f,  1.0f,  1.0f,
					-1.0f,  1.0f, -1.0f,
					-1.0f, -1.0f, -1.0f,
					-1.0f, -1.0f, -1.0f,
					-1.0f, -1.0f,  1.0f,
					-1.0f,  1.0f,  1.0f,
					// right face		
					 1.0f,  1.0f,  1.0f,
					 1.0f, -1.0f, -1.0f,
					 1.0f,  1.0f, -1.0f,
					 1.0f, -1.0f, -1.0f,
					 1.0f,  1.0f,  1.0f,
					 1.0f, -1.0f,  1.0f,
					 // bottom face		
					 -1.0f, -1.0f, -1.0f,
					  1.0f, -1.0f, -1.0f,
					  1.0f, -1.0f,  1.0f,
					  1.0f, -1.0f,  1.0f,
					 -1.0f, -1.0f,  1.0f,
					 -1.0f, -1.0f, -1.0f,
					 // top face			
					 -1.0f,  1.0f, -1.0f,
					  1.0f,  1.0f , 1.0f,
					  1.0f,  1.0f, -1.0f,
					  1.0f,  1.0f,  1.0f,
					 -1.0f,  1.0f, -1.0f,
					 -1.0f,  1.0f,  1.0f,
			};
			m_Data->SkyBoxVertexBuffer = VertexBuffer::Create(&vertices, sizeof(vertices));
		}




		// Light data into a uniform buffer
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



		{
			// Composite pipeline
			BufferLayout bufferLayout = {};
			Pipeline::CreateInfo pipelineCreateInfo{};
			pipelineCreateInfo.Shader = m_Data->CompositeShader;
			pipelineCreateInfo.UseDepthTest = false;
			pipelineCreateInfo.UseDepthWrite = false;
			pipelineCreateInfo.RenderPass = m_Data->RenderPass;
			pipelineCreateInfo.VertexBufferLayout = bufferLayout;
			pipelineCreateInfo.Topology = PrimitiveTopology::TriangleStrip;
			m_Data->CompositePipeline = Pipeline::Create(pipelineCreateInfo);

			// Composite pipeline descriptor
			uint32_t index = 0;
			m_Data->Descriptor.resize(Renderer::GetRendererConfig().FramesInFlight);
			for (auto& descriptor : m_Data->Descriptor)
			{
				descriptor = Material::Create(m_Data->CompositeShader, "CompositePass-Material");

				Ref<Image2D> positionTexture = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->RenderPass->GetColorAttachment(0, index);
				Ref<Image2D> normalTexture = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->RenderPass->GetColorAttachment(1, index);
				Ref<Image2D> albedoTexture = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->RenderPass->GetColorAttachment(2, index);
				Ref<Image2D> compositeTexture = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->RenderPass->GetColorAttachment(3, index);
				Ref<TextureCubeMap> prefilteredMap = Renderer::GetSceneEnvironment()->GetPrefilteredMap();
				Ref<TextureCubeMap> irradianceMap = Renderer::GetSceneEnvironment()->GetIrradianceMap();
				Ref<Texture2D> brdfLut = Renderer::GetBRDFLut();

				descriptor->Set("CameraData.Gamma", 2.2f);
				descriptor->Set("u_PositionTexture", positionTexture);
				descriptor->Set("u_NormalTexture", normalTexture);
				descriptor->Set("u_AlbedoTexture", albedoTexture);
				descriptor->Set("u_CompositeTexture", compositeTexture);
				descriptor->Set("u_RadianceFilteredMap", prefilteredMap);
				descriptor->Set("u_IrradianceMap", irradianceMap);
				descriptor->Set("u_BRDFLut", brdfLut);

				//descriptor->Set("LightData", m_PointLightUniformBuffer);

				descriptor.As<VulkanMaterial>()->UpdateVulkanDescriptorIfNeeded();

				index++;
			}
		}

		m_Data->PreviousDepthbuffer.resize(Renderer::GetRendererConfig().FramesInFlight);
		for (auto& depthbuffer : m_Data->PreviousDepthbuffer)
		{
			ImageSpecification imageSpec{};
			imageSpec.Width = 1600;
			imageSpec.Height = 900;
			imageSpec.Format = ImageFormat::Depth24Stencil8;
			imageSpec.Usage = ImageUsage::DepthStencil;
			imageSpec.UseMipChain = true;
			depthbuffer = Image2D::Create(imageSpec);
		}

	}

	void VulkanCompositePass::OnUpdate(const RenderQueue& renderQueue)
	{
		// If we have 0 meshes, we shouldnt render this pass
		if (renderQueue.GetQueueSize() == 0) return;

		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		Ref<Framebuffer> framebuffer = m_Data->RenderPass->GetFramebuffer(currentFrameIndex);
		Ref<VulkanPipeline> vulkanPipeline = m_Data->CompositePipeline.As<VulkanPipeline>();
		Ref<VulkanPipeline> vulkanSkyboxPipeline = m_Data->SkyboxPipeline.As<VulkanPipeline>();

		// Updating the push constant data from the renderQueue
		m_PushConstantData.CameraPosition = renderQueue.CameraPosition;


		{
			// From the GBuffer blit the depth texture to render the environment cubemap
			auto vulkanSrcDepthImage = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->RenderPass->GetColorAttachment(5, currentFrameIndex);
			auto vulkanDstDepthImage = m_Data->RenderPass->GetColorAttachment(1, currentFrameIndex).As<VulkanImage2D>();
			vulkanDstDepthImage->BlitImage(cmdBuf, vulkanSrcDepthImage);
		}

#if 0
		{
			// Create the depth pyramid
			// Steps:
			// 1) Blit the depth buffer from the previous frame into a new texture
			// 2) Generate the mips for that texture
			int32_t previousFrameIndex = (int32_t)currentFrameIndex - 1;
			if (previousFrameIndex < 0)
				previousFrameIndex = Renderer::GetRendererConfig().FramesInFlight - 1;

			auto vulkanSrcDepthImage = m_Data->RenderPass->GetColorAttachment(1, currentFrameIndex);
			auto vulkanDstDepthImage = m_Data->PreviousDepthbuffer[previousFrameIndex].As<VulkanImage2D>();
			vulkanDstDepthImage->BlitImage(cmdBuf, vulkanSrcDepthImage);

			vulkanDstDepthImage->GenerateMipMaps(cmdBuf, vulkanDstDepthImage->GetVulkanImageLayout());
		}
#endif




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

		// Camera information
		m_Data->Descriptor[currentFrameIndex]->Set("CameraData.Exposure", renderQueue.Camera.GetExposure());

		// Drawing a quad
		m_Data->Descriptor[currentFrameIndex]->Bind(m_Data->CompositePipeline);
		vkCmdDraw(cmdBuf, 3, 1, 0, 0);


		// Drawing the skybox
		m_Data->SkyboxDescriptor->Set("CameraData.Exposure", renderQueue.Camera.GetExposure());
		m_Data->SkyboxDescriptor->Set("CameraData.Lod", renderQueue.Camera.GetDOF());

		vulkanSkyboxPipeline->Bind();

		m_Data->SkyboxDescriptor->Bind(m_Data->SkyboxPipeline);
		m_Data->SkyBoxVertexBuffer->Bind();
		Vector<glm::mat4> pushConstant(2);
		pushConstant[0] = renderQueue.Camera.GetProjectionMatrix();
		pushConstant[1] = renderQueue.Camera.GetViewMatrix();

		vulkanSkyboxPipeline->BindVulkanPushConstant("u_PushConstant", pushConstant.data());

		vkCmdDraw(cmdBuf, 36, 1, 0, 0);


		m_Data->RenderPass->Unbind();
	}

	void VulkanCompositePass::OnResize(uint32_t width, uint32_t height)
	{
		RenderPassSpecification renderPassSpec =
		{
			width, height, 3,
			{
				// Color Attachment
				{
					FramebufferTextureFormat::RGBA16F, ImageUsage::Storage,
					OperationLoad::Clear,    OperationStore::Store,
					OperationLoad::DontCare, OperationStore::DontCare,
				},

				// Depth Attachment
				{
					FramebufferTextureFormat::DepthStencil, ImageUsage::DepthStencil,
					OperationLoad::Load,      OperationStore::Store,        
					OperationLoad::DontCare,  OperationStore::DontCare, 
				}
			}
		};
		m_Data->RenderPass = RenderPass::Create(renderPassSpec);

		m_Data->PreviousDepthbuffer.resize(Renderer::GetRendererConfig().FramesInFlight);
		for (auto& depthbuffer : m_Data->PreviousDepthbuffer)
		{
			ImageSpecification imageSpec{};
			imageSpec.Width = width;
			imageSpec.Height = height;
			imageSpec.Format = ImageFormat::Depth24Stencil8;
			imageSpec.Usage = ImageUsage::DepthStencil;
			imageSpec.UseMipChain = true;
			depthbuffer = Image2D::Create(imageSpec);
		}


		uint32_t index = 0;
		for (auto& descriptor : m_Data->Descriptor)
		{
			Ref<Image2D> positionTexture = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->RenderPass->GetColorAttachment(0, index);
			Ref<Image2D> normalTexture = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->RenderPass->GetColorAttachment(1, index);
			Ref<Image2D> albedoTexture = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->RenderPass->GetColorAttachment(2, index);
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