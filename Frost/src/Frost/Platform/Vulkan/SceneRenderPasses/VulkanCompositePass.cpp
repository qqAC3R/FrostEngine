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
#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanVoxelizationPass.h"
#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanShadowPass.h"
#include "Frost/Platform/Vulkan/VulkanSceneEnvironment.h"

//#include "Frost/Renderer/AreaLightLUT.h"
#include "../../FrostEditor/Resources/LUT/AreaLightLUT.h"

#include <imgui.h>
#include <imgui_internal.h>

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
		m_Data->PointLightCullingShader = Renderer::GetShaderLibrary()->Get("TiledPointLightCulling");
		m_Data->RectLightCullingShader = Renderer::GetShaderLibrary()->Get("TiledRectangularLightCulling");

		ImageSpecification imageSpec{};
		imageSpec.Format = ImageFormat::RGBA32F;
		imageSpec.Width = 64;
		imageSpec.Height = 64;
		imageSpec.Sampler.SamplerWrap = ImageWrap::ClampToEdge;
		imageSpec.Sampler.SamplerFilter = ImageFilter::Linear;
		imageSpec.UseMipChain = false;
		m_Data->LTC1_Lut = Image2D::Create(imageSpec, &LTC1);
		m_Data->LTC2_Lut = Image2D::Create(imageSpec, &LTC2);

		// Initialize the renderpass
		TiledPointLightCullingInitData(1600, 900);
		TiledRectLightCullingInitData(1600, 900);
		PBRInitData(1600, 900);

		// Init the scene enviorment maps
		Ref<VulkanSceneEnvironment> vulkanSceneEnvironment = Renderer::GetSceneEnvironment().As<VulkanSceneEnvironment>();
		vulkanSceneEnvironment->InitSkyBoxPipeline(m_Data->RenderPass);

		Renderer::GetSceneEnvironment()->SetEnvironmentMapCallback(std::bind(&VulkanCompositePass::OnEnvMapChangeCallback, this, std::placeholders::_1, std::placeholders::_2));
	}

	void VulkanCompositePass::PBRInitData(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;

		RenderPassSpecification renderPassSpec =
		{
			width, height, framesInFlight,
			{
				// Color Attachment
				{
					FramebufferTextureFormat::RGBA16F, ImageUsage::Storage,
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


		// Composite pipeline
		BufferLayout bufferLayout = {};
		Pipeline::CreateInfo pipelineCreateInfo{};
		pipelineCreateInfo.Shader = m_Data->CompositeShader;
		pipelineCreateInfo.UseDepthTest = false;
		pipelineCreateInfo.UseDepthWrite = false;
		pipelineCreateInfo.RenderPass = m_Data->RenderPass;
		pipelineCreateInfo.VertexBufferLayout = bufferLayout;
		pipelineCreateInfo.Topology = PrimitiveTopology::TriangleStrip;
		if(!m_Data->CompositePipeline)
			m_Data->CompositePipeline = Pipeline::Create(pipelineCreateInfo);

		// Composite pipeline descriptor
		m_Data->Descriptor.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			if(!m_Data->Descriptor[i])
				m_Data->Descriptor[i] = Material::Create(m_Data->CompositeShader, "CompositePass-Material");

			auto descriptor = m_Data->Descriptor[i];

			Ref<Image2D> positionTexture = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->GeometryRenderPass->GetColorAttachment(0, i);
			Ref<Image2D> normalTexture = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->GeometryRenderPass->GetColorAttachment(1, i);
			Ref<Image2D> albedoTexture = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->GeometryRenderPass->GetColorAttachment(2, i);
			Ref<Image2D> shadowTexture = m_RenderPassPipeline->GetRenderPassData<VulkanShadowPass>()->ShadowComputeTexture[i];
			Ref<Image2D> voxelIndirectDiffuseTex = m_RenderPassPipeline->GetRenderPassData<VulkanVoxelizationPass>()->VCT_IndirectDiffuseTexture[i];
			Ref<Image2D> voxelIndirectSpecularTex = m_RenderPassPipeline->GetRenderPassData<VulkanVoxelizationPass>()->VCT_IndirectSpecularTexture[i];

			Ref<Texture2D> brdfLut = Renderer::GetBRDFLut();

			descriptor->Set("UniformBuffer.CameraGamma", 2.2f);

			descriptor->Set("u_PositionTexture", positionTexture);
			descriptor->Set("u_NormalTexture", normalTexture);
			descriptor->Set("u_AlbedoTexture", albedoTexture);
			descriptor->Set("u_ShadowTexture", shadowTexture);
			descriptor->Set("u_VoxelIndirectDiffuseTex", voxelIndirectDiffuseTex);
			descriptor->Set("u_VoxelIndirectSpecularTex", voxelIndirectSpecularTex);
			descriptor->Set("u_BRDFLut", brdfLut);
			descriptor->Set("u_LTC1Lut", m_Data->LTC1_Lut);
			descriptor->Set("u_LTC2Lut", m_Data->LTC2_Lut);

			// Point Lights
			descriptor->Set("u_PointLightData",        m_Data->PointLightBufferData[i]);
			descriptor->Set("u_VisiblePointLightData", m_Data->PointLightIndices[i]);

			// Rectangular Lights
			descriptor->Set("u_RectangularLightData", m_Data->RectLightBufferData[i]);
			descriptor->Set("u_VisibleRectLightData", m_Data->RectLightIndices[i]);

			descriptor.As<VulkanMaterial>()->UpdateVulkanDescriptorIfNeeded();
		}
	}

	void VulkanCompositePass::InitLate()
	{
	}

	void VulkanCompositePass::TiledPointLightCullingInitData(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;
		uint32_t maxPointLightCount = Renderer::GetRendererConfig().MaxPointLightCount;

		// Allocating data for lights
		if (m_Data->PointLightBufferData.empty())
		{
			m_Data->PointLightBufferData.resize(framesInFlight);
			for (auto& pointLightBufferData : m_Data->PointLightBufferData)
			{
				// sizeof(uint32_t) - Number of point lights in the scene 
				// (maxPointLightCount * sizeof(RenderQueue::LightData::PointLight) - Size of 1024 point lights
				uint32_t lightDataSize = maxPointLightCount * sizeof(RenderQueue::LightData::PointLight);

				pointLightBufferData = BufferDevice::Create(lightDataSize, { BufferUsage::Storage });
			}
		}

		// Pipeline
		ComputePipeline::CreateInfo computePipelineCI{};
		computePipelineCI.Shader = m_Data->PointLightCullingShader;
		if(!m_Data->PointLightCullingPipeline)
			m_Data->PointLightCullingPipeline = ComputePipeline::Create(computePipelineCI);

		// Descriptor
		m_Data->PointLightCullingDescriptor.resize(framesInFlight);
		m_Data->PointLightIndices.resize(framesInFlight);
		m_Data->PointLightIndicesVolumetric.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			if (!m_Data->PointLightCullingDescriptor[i])
				m_Data->PointLightCullingDescriptor[i] = Material::Create(m_Data->PointLightCullingShader, "Tiled_PointLightCulling");

			auto descriptor = m_Data->PointLightCullingDescriptor[i].As<VulkanMaterial>();

			// Light indices buffer
			uint32_t workGroupsX = std::ceil(width / 16.0f);
			uint32_t workGroupsY = std::ceil(height / 16.0f);
			uint64_t lightIndicesBufferSize = workGroupsX * workGroupsY * maxPointLightCount * sizeof(int32_t); // 16x16 (tiles) * 1024 (lights per tile)
			// For Point Lights
			m_Data->PointLightIndices[i] = BufferDevice::Create(lightIndicesBufferSize, { BufferUsage::Storage });
			m_Data->PointLightIndicesVolumetric[i] = BufferDevice::Create(lightIndicesBufferSize, { BufferUsage::Storage });

			// Depth buffer neccesary for the `i` frmae
			const Ref<Image2D>& depthBuffer = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->GeometryRenderPass->GetDepthAttachment(i);

			// Setting up the data into the descriptor
			descriptor->Set("u_LightData", m_Data->PointLightBufferData[i]);
			descriptor->Set("u_VisibleLightsBuffer", m_Data->PointLightIndices[i]);
			descriptor->Set("u_VisibleLightsVolumetricBuffer", m_Data->PointLightIndicesVolumetric[i]);
			descriptor->Set("u_DepthBuffer", depthBuffer);
			descriptor->UpdateVulkanDescriptorIfNeeded();
		}
	}

	void VulkanCompositePass::TiledRectLightCullingInitData(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;
		uint32_t maxRectLightCount = Renderer::GetRendererConfig().MaxRectangularLightCount;

		// Allocating data for lights
		if (m_Data->RectLightBufferData.empty())
		{
			m_Data->RectLightBufferData.resize(framesInFlight);
			for (auto& rectLightBufferData : m_Data->RectLightBufferData)
			{
				// sizeof(uint32_t) - Number of rectangular lights in the scene 
				// (maxRectLightCount * sizeof(RenderQueue::LightData::RectangularLightData) - Size of 64 rectangular lights
				uint32_t lightDataSize = maxRectLightCount * sizeof(RenderQueue::LightData::RectangularLightData);

				rectLightBufferData = BufferDevice::Create(lightDataSize, { BufferUsage::Storage });
			}
		}

		// Pipeline
		ComputePipeline::CreateInfo computePipelineCI{};
		computePipelineCI.Shader = m_Data->RectLightCullingShader;
		if (!m_Data->RectLightCullingPipeline)
			m_Data->RectLightCullingPipeline = ComputePipeline::Create(computePipelineCI);

		// Descriptor
		m_Data->RectLightCullingDescriptor.resize(framesInFlight);
		m_Data->RectLightIndices.resize(framesInFlight);
		m_Data->RectLightIndicesVolumetric.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			if (!m_Data->RectLightCullingDescriptor[i])
				m_Data->RectLightCullingDescriptor[i] = Material::Create(m_Data->RectLightCullingShader, "Tiled_RectangularLightCulling");

			auto descriptor = m_Data->RectLightCullingDescriptor[i].As<VulkanMaterial>();

			// Light indices buffer
			uint32_t workGroupsX = std::ceil(width / 16.0f);
			uint32_t workGroupsY = std::ceil(height / 16.0f);
			uint64_t lightIndicesBufferSize = workGroupsX * workGroupsY * maxRectLightCount * sizeof(int32_t); // 16x16 (tiles) * 64 (lights per tile)
			// For Rectangular Lights
			m_Data->RectLightIndices[i] = BufferDevice::Create(lightIndicesBufferSize, { BufferUsage::Storage });
			m_Data->RectLightIndicesVolumetric[i] = BufferDevice::Create(lightIndicesBufferSize, { BufferUsage::Storage });

			// Depth buffer neccesary for the `i` frmae
			const Ref<Image2D>& depthBuffer = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->GeometryRenderPass->GetDepthAttachment(i);

			// Setting up the data into the descriptor
			descriptor->Set("u_LightData", m_Data->RectLightBufferData[i]);
			descriptor->Set("u_VisibleLightsBuffer", m_Data->RectLightIndices[i]);
			descriptor->Set("u_VisibleLightsVolumetricBuffer", m_Data->RectLightIndicesVolumetric[i]);
			descriptor->Set("u_DepthBuffer", depthBuffer);
			descriptor->UpdateVulkanDescriptorIfNeeded();
		}
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
		m_PushConstantData.DirectionalLightDir = renderQueue.m_LightData.DirLight.Direction;
		m_PushConstantData.DirectionalLightIntensity = renderQueue.m_LightData.DirLight.Specification.Intensity;

		{
			auto vulkanDepthImage = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->GeometryRenderPass->GetDepthAttachment(currentFrameIndex).As<VulkanImage2D>();
			vulkanDepthImage->TransitionLayout(cmdBuf, vulkanDepthImage->GetVulkanImageLayout(), VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		}

		{
			// From the GBuffer, blit the depth texture to render the environment cubemap
			auto vulkanSrcDepthImage = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->GeometryRenderPass->GetDepthAttachment(currentFrameIndex);
			auto vulkanDstDepthImage = m_Data->RenderPass->GetDepthAttachment(currentFrameIndex).As<VulkanImage2D>();
			vulkanDstDepthImage->BlitImage(cmdBuf, vulkanSrcDepthImage);
		}





		// Setting up the light data
		// Gathering the data
		auto pointLightData = renderQueue.m_LightData.PointLights.data();
		uint32_t pointLightCount = static_cast<uint32_t>(renderQueue.m_LightData.PointLights.size());
		uint32_t pointLightDataSize = (pointLightCount * sizeof(RenderQueue::LightData::PointLight));

		auto rectLightData = renderQueue.m_LightData.RectangularLights.data();
		uint32_t rectLightCount = static_cast<uint32_t>(renderQueue.m_LightData.RectangularLights.size());
		uint32_t rectLightDataSize = (rectLightCount * sizeof(RenderQueue::LightData::RectangularLightData));

		// Copying the cpu buffer into the gpu
		m_Data->PointLightBufferData[currentFrameIndex]->SetData(pointLightDataSize, (void*)pointLightData);
		m_Data->RectLightBufferData[currentFrameIndex]->SetData(rectLightDataSize, (void*)rectLightData);


		VulkanRenderer::BeginTimeStampPass("Light Culling Pass (Point Light)");
		TiledPointLightCullingUpdate(renderQueue);
		VulkanRenderer::EndTimeStampPass("Light Culling Pass (Point Light)");

		VulkanRenderer::BeginTimeStampPass("Light Culling Pass (Rectangular Light)");
		TiledRectLightCullingUpdate(renderQueue);
		VulkanRenderer::EndTimeStampPass("Light Culling Pass (Rectangular Light)");

		

		VulkanRenderer::BeginTimeStampPass("Lightning Pass (PBR)");

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
		m_Data->Descriptor[currentFrameIndex]->Set("UniformBuffer.CameraExposure", renderQueue.m_Camera->GetExposure());
		m_Data->Descriptor[currentFrameIndex]->Set("UniformBuffer.PointLightCount", static_cast<float>(pointLightCount));
		m_Data->Descriptor[currentFrameIndex]->Set("UniformBuffer.RectangularLightCount", static_cast<float>(rectLightCount));

		uint32_t width = static_cast<uint32_t>(renderQueue.ViewPortWidth);
		float workGroupX = std::ceil(renderQueue.ViewPortWidth / 16.0f);
		m_Data->Descriptor[currentFrameIndex]->Set("UniformBuffer.LightCullingWorkgroup", workGroupX);


		// Drawing a quad
		m_Data->Descriptor[currentFrameIndex]->Bind(m_Data->CompositePipeline);
		vkCmdDraw(cmdBuf, 3, 1, 0, 0);


		Renderer::GetSceneEnvironment()->RenderSkyBox(renderQueue);

		m_Data->RenderPass->Unbind();

		VulkanRenderer::EndTimeStampPass("Lightning Pass (PBR)");
	}

	void VulkanCompositePass::TiledPointLightCullingUpdate(const RenderQueue& renderQueue)
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		auto& vulkanDescriptor = m_Data->PointLightCullingDescriptor[currentFrameIndex].As<VulkanMaterial>();
		auto& vulkanComputePipeline = m_Data->PointLightCullingPipeline.As<VulkanComputePipeline>();
		auto& vulkanLightIndicesBuffer = m_Data->PointLightIndices[currentFrameIndex].As<VulkanBufferDevice>();


		// Updating the data for the tiled light culling shader
		m_TiledLightCullPushConstant.ScreenSize = { renderQueue.ViewPortWidth, renderQueue.ViewPortHeight };
		m_TiledLightCullPushConstant.NumberOfLights = static_cast<uint32_t>(renderQueue.m_LightData.PointLights.size());
		
		m_TiledLightCullPushConstant.ProjectionMatrix = renderQueue.CameraProjectionMatrix;
		m_TiledLightCullPushConstant.ProjectionMatrix[1][1] *= -1;
		m_TiledLightCullPushConstant.ViewMatrix = renderQueue.CameraViewMatrix;
		m_TiledLightCullPushConstant.ViewProjectionMatrix = m_TiledLightCullPushConstant.ProjectionMatrix * renderQueue.CameraViewMatrix;

		//vulkanDescriptor->Set("UniformBuffer.NumberOfLights", static_cast<uint32_t>(renderQueue.m_LightData.PointLights.size()));

		// Setting up the rendererData
		vulkanComputePipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &m_TiledLightCullPushConstant);

		vulkanDescriptor->Bind(cmdBuf, m_Data->PointLightCullingPipeline);

		uint32_t groupX = std::ceil(renderQueue.ViewPortWidth / 16.0f);
		uint32_t groupY = std::ceil(renderQueue.ViewPortHeight / 16.0f);
		vulkanComputePipeline->Dispatch(cmdBuf, groupX, groupY, 1);

		// Putting a memory barrier to wait till the tiled compute shader finishes
		vulkanLightIndicesBuffer->SetMemoryBarrier(cmdBuf,
			VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
		);
	}

	void VulkanCompositePass::TiledRectLightCullingUpdate(const RenderQueue& renderQueue)
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		auto& vulkanDescriptor = m_Data->RectLightCullingDescriptor[currentFrameIndex].As<VulkanMaterial>();
		auto& vulkanComputePipeline = m_Data->RectLightCullingPipeline.As<VulkanComputePipeline>();
		auto& vulkanLightIndicesBuffer = m_Data->RectLightIndices[currentFrameIndex].As<VulkanBufferDevice>();


		// Updating the data for the tiled light culling shader
		//m_TiledLightCullPushConstant.ScreenSize = { renderQueue.ViewPortWidth, renderQueue.ViewPortHeight };
		m_TiledLightCullPushConstant.NumberOfLights = static_cast<uint32_t>(renderQueue.m_LightData.RectangularLights.size());
		//
		//m_TiledLightCullPushConstant.ProjectionMatrix = renderQueue.CameraProjectionMatrix;
		//m_TiledLightCullPushConstant.ProjectionMatrix[1][1] *= -1;
		//m_TiledLightCullPushConstant.ViewMatrix = renderQueue.CameraViewMatrix;
		//m_TiledLightCullPushConstant.ViewProjectionMatrix = m_TiledLightCullPushConstant.ProjectionMatrix * renderQueue.CameraViewMatrix;

		// Setting up the rendererData
		vulkanComputePipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &m_TiledLightCullPushConstant);

		vulkanDescriptor->Bind(cmdBuf, m_Data->RectLightCullingPipeline);

		uint32_t groupX = std::ceil(renderQueue.ViewPortWidth / 16.0f);
		uint32_t groupY = std::ceil(renderQueue.ViewPortHeight / 16.0f);
		vulkanComputePipeline->Dispatch(cmdBuf, groupX, groupY, 1);

		// Putting a memory barrier to wait till the tiled compute shader finishes
		vulkanLightIndicesBuffer->SetMemoryBarrier(cmdBuf,
			VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
		);
	}

	void VulkanCompositePass::PBRUpdate()
	{

	}

	void VulkanCompositePass::OnRenderDebug()
	{
		if (ImGui::CollapsingHeader("Deffered Tiled Pipeline"))
		{
			ImGui::SliderInt("Light HeatMap", &m_PushConstantData.UseLightHeatMap, 0, 1);
		}
	}

	void VulkanCompositePass::OnResize(uint32_t width, uint32_t height)
	{
		TiledPointLightCullingInitData(width, height);
		TiledRectLightCullingInitData(width, height);
		PBRInitData(width, height);
	}

	void VulkanCompositePass::OnResizeLate(uint32_t width, uint32_t height)
	{
	}

	void VulkanCompositePass::ShutDown()
	{
		delete m_Data;
	}
}