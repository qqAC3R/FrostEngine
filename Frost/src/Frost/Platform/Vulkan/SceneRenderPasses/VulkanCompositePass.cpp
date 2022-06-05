#include "frostpch.h"
#include "VulkanCompositePass.h"

#include "Frost/Platform/Vulkan/VulkanImage.h"
#include "Frost/Platform/Vulkan/VulkanTexture.h"
#include "Frost/Platform/Vulkan/VulkanContext.h"
#include "Frost/Platform/Vulkan/VulkanRenderer.h"
#include "Frost/Platform/Vulkan/VulkanPipeline.h"
#include "Frost/Platform/Vulkan/VulkanMaterial.h"
#include "Frost/Platform/Vulkan/VulkanRenderPass.h"
#include "Frost/Platform/Vulkan/VulkanFramebuffer.h"
#include "Frost/Platform/Vulkan/VulkanPipelineCompute.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanBufferDevice.h"
#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanGeometryPass.h"
#include "Frost/Platform/Vulkan/VulkanSceneEnvironment.h"

#include <imgui.h>

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

		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;

		m_Data->CompositeShader = Renderer::GetShaderLibrary()->Get("PBRDeffered");
		m_Data->LightCullingShader = Renderer::GetShaderLibrary()->Get("TiledLightCulling");


		
		RenderPassSpecification renderPassSpec =
		{
			1600, 900, 3,
			{
				// Color Attachment
				{
					FramebufferTextureFormat::RGBA32F, ImageUsage::Storage,
					OperationLoad::Clear,  OperationStore::Store,
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
		m_Data->RenderPass = RenderPass::Create(renderPassSpec);

		// Init the scene enviorment maps
		Ref<VulkanSceneEnvironment> vulkanSceneEnvironment = Renderer::GetSceneEnvironment().As<VulkanSceneEnvironment>();
		vulkanSceneEnvironment->InitSkyBoxPipeline(m_Data->RenderPass);


		// Point lights
		{
			uint32_t maxPointLightCount = Renderer::GetRendererConfig().MaxPointLightCount;


			m_Data->PointLightBufferData.resize(framesInFlight);
			for (auto& pointLightBufferData : m_Data->PointLightBufferData)
			{
				// sizeof(uint32_t) - Number of point lights in the scene 
				// (maxPointLightCount * sizeof(RenderQueue::LightData::PointLight) - Size of 1024 point lights
				uint32_t lightDataSize = maxPointLightCount * sizeof(RenderQueue::LightData::PointLight);

				pointLightBufferData.DeviceBuffer = BufferDevice::Create(lightDataSize, { BufferUsage::Storage });
				pointLightBufferData.HostBuffer.Allocate(lightDataSize);
			}

			TiledLightCulling_DataInit();
		}


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
			m_Data->Descriptor.resize(framesInFlight);
			for (auto& descriptor : m_Data->Descriptor)
			{
				descriptor = Material::Create(m_Data->CompositeShader, "CompositePass-Material");

				Ref<Image2D> positionTexture = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->RenderPass->GetColorAttachment(0, index);
				Ref<Image2D> normalTexture = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->RenderPass->GetColorAttachment(1, index);
				Ref<Image2D> albedoTexture = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->RenderPass->GetColorAttachment(2, index);

				//Ref<Image2D> compositeTexture = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->RenderPass->GetColorAttachment(3, index);
				//Ref<TextureCubeMap> prefilteredMap = Renderer::GetSceneEnvironment()->GetPrefilteredMap();
				//Ref<TextureCubeMap> irradianceMap = Renderer::GetSceneEnvironment()->GetIrradianceMap();
				
				//Ref<TextureCubeMap> prefilteredMap = Renderer::GetSceneEnvironment().As<VulkanSceneEnvironment>()->GetAtmoshperePrefilterMap();
				//Ref<TextureCubeMap> irradianceMap = Renderer::GetSceneEnvironment().As<VulkanSceneEnvironment>()->GetAtmoshpereIrradianceMap();

				Ref<Texture2D> brdfLut = Renderer::GetBRDFLut();

				descriptor->Set("CameraData.Gamma", 2.2f);
				
				descriptor->Set("u_PositionTexture", positionTexture);
				descriptor->Set("u_NormalTexture", normalTexture);
				descriptor->Set("u_AlbedoTexture", albedoTexture);
				//descriptor->Set("u_CompositeTexture", compositeTexture);

				//descriptor->Set("u_RadianceFilteredMap", prefilteredMap);
				//descriptor->Set("u_IrradianceMap", irradianceMap);
				descriptor->Set("u_BRDFLut", brdfLut);
				
				descriptor->Set("u_LightData", m_Data->PointLightBufferData[index].DeviceBuffer);
				descriptor->Set("u_VisibleLightData", m_Data->PointLightIndices[index]);

				descriptor.As<VulkanMaterial>()->UpdateVulkanDescriptorIfNeeded();

				index++;
			}
		}

		Renderer::GetSceneEnvironment()->SetEnvironmentMapCallback(std::bind(&VulkanCompositePass::OnEnvMapChangeCallback, this, std::placeholders::_1, std::placeholders::_2));
	}

	void VulkanCompositePass::OnEnvMapChangeCallback(const Ref<TextureCubeMap>& prefiltered, const Ref<TextureCubeMap>& irradiance)
	{
		for (auto& descriptor : m_Data->Descriptor)
		{
			descriptor->Set("u_RadianceFilteredMap", prefiltered);
			descriptor->Set("u_IrradianceMap", irradiance);
			descriptor.As<VulkanMaterial>()->UpdateVulkanDescriptorIfNeeded();
		}
	}

	void VulkanCompositePass::InitLate()
	{
	}

	void VulkanCompositePass::TiledLightCulling_DataInit()
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;

		ComputePipeline::CreateInfo computePipelineCI{};
		computePipelineCI.Shader = m_Data->LightCullingShader;
		m_Data->LightCullingPipeline = ComputePipeline::Create(computePipelineCI);

		m_Data->PointLightIndices.resize(framesInFlight);
		m_Data->CullingDataBuffer.resize(framesInFlight);
		m_Data->LightCullingDescriptor.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			auto& descriptor = m_Data->LightCullingDescriptor[i];
			descriptor = Material::Create(m_Data->LightCullingShader, "Tiled_LightCulling");

			// Light indices buffer
			uint32_t workGroupsX = (1600 + ( 1600 % 16 )) / 16.0f;
			uint32_t workGroupsY = (900  + ( 900 % 16  )) / 16.0f;
			uint64_t lightIndicesBufferSize = workGroupsX * workGroupsY * 1024 * sizeof(int32_t); // 16x16 (tiles) * 1024 (lights per tile)
			m_Data->PointLightIndices[i] = BufferDevice::Create(lightIndicesBufferSize, { BufferUsage::Storage });

			// Renderer Data
			m_Data->CullingDataBuffer[i] = BufferDevice::Create(sizeof(RendererData), { BufferUsage::Storage });

			// Depth buffer neccesary for the `i` frmae
			const Ref<Image2D>& depthBuffer = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->RenderPass->GetDepthAttachment(i);

			// Setting up the data into the descriptor
			descriptor->Set("u_LightData", m_Data->PointLightBufferData[i].DeviceBuffer);
			descriptor->Set("u_VisibleLightsBuffer", m_Data->PointLightIndices[i]);
			descriptor->Set("u_RendererData", m_Data->CullingDataBuffer[i]);
			descriptor->Set("u_DepthBuffer", depthBuffer);

			auto& vulkanDescriptor = descriptor.As<VulkanMaterial>();
			vulkanDescriptor->UpdateVulkanDescriptorIfNeeded();
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
		//Ref<VulkanPipeline> vulkanSkyboxPipeline = m_Data->SkyboxPipeline.As<VulkanPipeline>();

		// Updating the push constant data from the renderQueue
		m_PushConstantData.CameraPosition = glm::vec4(renderQueue.CameraPosition, 1.0f);
		m_PushConstantData.DirectionalLightDir = renderQueue.m_LightData.DirectionalLight.Direction;

		{
			auto vulkanDepthImage = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->RenderPass->GetDepthAttachment(currentFrameIndex).As<VulkanImage2D>();
			vulkanDepthImage->TransitionLayout(cmdBuf, vulkanDepthImage->GetVulkanImageLayout(), VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		}


		TiledLightCulling_OnUpdate(renderQueue);

		{
			// From the GBuffer, blit the depth texture to render the environment cubemap
			auto vulkanSrcDepthImage = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->RenderPass->GetDepthAttachment(currentFrameIndex);
			auto vulkanDstDepthImage = m_Data->RenderPass->GetDepthAttachment(currentFrameIndex).As<VulkanImage2D>();
			vulkanDstDepthImage->BlitImage(cmdBuf, vulkanSrcDepthImage);
		}


		// Setting up the light data
		// Gathering the data
		auto pointLightData = renderQueue.m_LightData.PointLights.data();
		void* pointLightDataCPUPointer = m_Data->PointLightBufferData[currentFrameIndex].HostBuffer.Data;
		uint32_t pointLightCount = static_cast<uint32_t>(renderQueue.m_LightData.PointLights.size());
		uint32_t pointLightDataSize = (pointLightCount * sizeof(RenderQueue::LightData::PointLight));

		// Writting into a cpu buffer
		m_Data->PointLightBufferData[currentFrameIndex].HostBuffer.Write((void*)pointLightData, pointLightDataSize, 0);

		// Copying the cpu buffer into the gpu
		m_Data->PointLightBufferData[currentFrameIndex].DeviceBuffer->SetData(pointLightDataSize, pointLightDataCPUPointer);

		m_PushConstantData.CameraPosition.w = static_cast<float>(pointLightCount);


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
		m_Data->Descriptor[currentFrameIndex]->Set("CameraData.Exposure", renderQueue.m_Camera.GetExposure());
		m_Data->Descriptor[currentFrameIndex]->Set("CameraData.PointLightCount", static_cast<float>(pointLightCount));

		uint32_t width = static_cast<uint32_t>(m_RendererData.ScreenSize.x);
		float workGroupsX = static_cast<float>(width + (width % 16)) / 16.0f;
		m_Data->Descriptor[currentFrameIndex]->Set("CameraData.LightCullingWorkgroup", workGroupsX);


		// Drawing a quad
		m_Data->Descriptor[currentFrameIndex]->Bind(m_Data->CompositePipeline);
		vkCmdDraw(cmdBuf, 3, 1, 0, 0);


		Renderer::GetSceneEnvironment()->RenderSkyBox(renderQueue);

		m_Data->RenderPass->Unbind();
	}

	void VulkanCompositePass::OnRenderDebug()
	{
		if (ImGui::CollapsingHeader("Deffered Tiled Pipeline"))
		{
			ImGui::SliderFloat("Light's HeatMap", &m_PushConstantData.UseLightHeatMap, 0.0f, 1.0f, nullptr, 1.0f);
		}
	}

	void VulkanCompositePass::TiledLightCulling_OnUpdate(const RenderQueue& renderQueue)
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		auto& vulkanDescriptor = m_Data->LightCullingDescriptor[currentFrameIndex].As<VulkanMaterial>();
		auto& vulkanComputePipeline = m_Data->LightCullingPipeline.As<VulkanComputePipeline>();
		auto& vulkanLightIndicesBuffer = m_Data->PointLightIndices[currentFrameIndex].As<VulkanBufferDevice>();


		// Updating the renderData for the tiled light culling shader
		m_RendererData.ScreenSize = { renderQueue.ViewPortWidth, renderQueue.ViewPortHeight };
		m_RendererData.PointLightCount = static_cast<uint32_t>(renderQueue.m_LightData.PointLights.size());
		m_RendererData.ProjectionMatrix = renderQueue.CameraProjectionMatrix;
		m_RendererData.ProjectionMatrix[1][1] *= -1;

		m_RendererData.ViewMatrix = renderQueue.CameraViewMatrix;
		m_RendererData.ViewProjectionMatrix = m_RendererData.ProjectionMatrix * m_RendererData.ViewMatrix;


		// Setting up the rendererData
		m_Data->CullingDataBuffer[currentFrameIndex]->SetData((void*)&m_RendererData);


		vulkanDescriptor->Bind(cmdBuf, m_Data->LightCullingPipeline);

		uint32_t screenSizeX = m_RendererData.ScreenSize.x;
		uint32_t screenSizeY = m_RendererData.ScreenSize.y;

		vulkanComputePipeline->Dispatch(cmdBuf,
			(screenSizeX + (screenSizeX % 16)) / 16.0f,
			(screenSizeY + (screenSizeY % 16)) / 16.0f,
			1
		);

		// Putting a memory barrier to wait till the tiled compute shader finishes
		vulkanLightIndicesBuffer->SetMemoryBarrier(cmdBuf,
			VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
		);
	}

	void VulkanCompositePass::LightBufferBlur_OnUpdate(const RenderQueue& renderQueue)
	{

	}

	void VulkanCompositePass::OnResize(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;

		RenderPassSpecification renderPassSpec =
		{
			width, height, 3,
			{
				// Color Attachment
				{
					FramebufferTextureFormat::RGBA32F, ImageUsage::Storage,
					OperationLoad::Clear,    OperationStore::Store,
					OperationLoad::DontCare, OperationStore::DontCare,
				},

				// Depth Attachment
				{
					FramebufferTextureFormat::Depth, ImageUsage::DepthStencil,
					OperationLoad::Load,      OperationStore::Store,
					OperationLoad::DontCare,  OperationStore::DontCare,
				}
			}
		};
		m_Data->RenderPass = RenderPass::Create(renderPassSpec);


		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			auto& vulkanComputeDescriptor = m_Data->LightCullingDescriptor[i].As<VulkanMaterial>();

			// Depth buffer neccesary for the `i` frmae
			const Ref<Image2D>& depthBuffer = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->RenderPass->GetDepthAttachment(i);

			// Light indices buffer
			uint32_t workGroupsX = (width  + (width  % 16)) / 16.0f;
			uint32_t workGroupsY = (height + (height % 16)) / 16.0f;
			uint64_t lightIndicesBufferSize = workGroupsX * workGroupsY * 1024 * sizeof(int32_t); // 16x16 (tiles) * 1024 (lights per tile)
			m_Data->PointLightIndices[i] = BufferDevice::Create(lightIndicesBufferSize, { BufferUsage::Storage });

			// Setting up the data into the descriptor
			vulkanComputeDescriptor->Set("u_DepthBuffer", depthBuffer);
			vulkanComputeDescriptor->Set("u_VisibleLightsBuffer", m_Data->PointLightIndices[i]);

			// Update them
			vulkanComputeDescriptor->UpdateVulkanDescriptorIfNeeded();
		}


		for(uint32_t i = 0; i < m_Data->Descriptor.size(); i++)
		{
			auto& descriptor = m_Data->Descriptor[i];

			Ref<Image2D> positionTexture = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->RenderPass->GetColorAttachment(0, i);
			Ref<Image2D> normalTexture = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->RenderPass->GetColorAttachment(1, i);
			Ref<Image2D> albedoTexture = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->RenderPass->GetColorAttachment(2, i);

			descriptor->Set("u_PositionTexture", positionTexture);
			descriptor->Set("u_NormalTexture", normalTexture);
			descriptor->Set("u_AlbedoTexture", albedoTexture);
			descriptor->Set("u_VisibleLightData", m_Data->PointLightIndices[i]); // This is obtained from the light culling compute shader

			descriptor.As<VulkanMaterial>()->UpdateVulkanDescriptorIfNeeded();
		}

	}

	void VulkanCompositePass::OnResizeLate(uint32_t width, uint32_t height)
	{
	}

	void VulkanCompositePass::ShutDown()
	{
		delete m_Data;
	}
}