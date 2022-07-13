#include "frostpch.h"
#include "VulkanVolumetricPass.h"

#include "Frost/Platform/Vulkan/VulkanContext.h"

#include "Frost/Platform/Vulkan/VulkanImage.h"
#include "Frost/Platform/Vulkan/VulkanMaterial.h"
#include "Frost/Platform/Vulkan/VulkanPipelineCompute.h"
#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanShadowPass.h"
#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanGeometryPass.h"
#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanPostFXPass.h"

#include <imgui.h>

namespace Frost
{
	VulkanVolumetricPass::VulkanVolumetricPass()
		: m_Name("Volumetrics Pass")
	{
	}

	VulkanVolumetricPass::~VulkanVolumetricPass()
	{
	}

	void VulkanVolumetricPass::Init(SceneRenderPassPipeline* renderPassPipeline)
	{
		m_RenderPassPipeline = renderPassPipeline;
		m_Data = new InternalData();

		m_Data->VolumetricsShader = Renderer::GetShaderLibrary()->Get("Volumetrics");
		m_Data->GaussianBlurShader = Renderer::GetShaderLibrary()->Get("GaussianBlur");
		m_Data->FroxelPopulateShader = Renderer::GetShaderLibrary()->Get("FroxelVolumePopulate");
		m_Data->VolumetricComputeShader = Renderer::GetShaderLibrary()->Get("VolumetricCompute");

		VolumetricsInitData(1600, 900);
		GaussianBlurInitData(1600, 900);
		FroxelVolumetricInitData(1600, 900);
		VolumetricComputeInitData(1600, 900);
	}

//#define INIT_IF_VALID(x, struc, ...) if(!x) { x = struc::Create(__VA_ARGS__); }

	void VulkanVolumetricPass::VolumetricsInitData(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;

		// Pipeline creation
		ComputePipeline::CreateInfo createInfoCP{};
		createInfoCP.Shader = m_Data->VolumetricsShader;
		if (!m_Data->VolumetricsPipeline)
			m_Data->VolumetricsPipeline = ComputePipeline::Create(createInfoCP);



		m_Data->VolumetricsTexture.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			ImageSpecification imageSpec{};
			imageSpec.Width = width;
			imageSpec.Height = height;
			imageSpec.Sampler.SamplerFilter = ImageFilter::Nearest;
			imageSpec.Sampler.SamplerWrap = ImageWrap::ClampToEdge;
			imageSpec.Format = ImageFormat::RGBA8;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.UseMipChain = false;

			m_Data->VolumetricsTexture[i] = Image2D::Create(imageSpec);
		}

		m_Data->VolumetricsDescriptor.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			//INIT_IF_VALID(m_Data->VolumetricsDescriptor[i], Material, m_Data->VolumetricsShader, "VolumetricsMaterial");
			if (!m_Data->VolumetricsDescriptor[i])
				m_Data->VolumetricsDescriptor[i] = Material::Create(m_Data->VolumetricsShader, "VolumetricsMaterial");
			
			auto descriptor = m_Data->VolumetricsDescriptor[i].As<VulkanMaterial>();

			auto posTexture = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->GeometryRenderPass->GetColorAttachment(0, i);
			auto shadowDepthTexture = m_RenderPassPipeline->GetRenderPassData<VulkanShadowPass>()->ShadowDepthRenderPass->GetColorAttachment(0, i);

			descriptor->Set("u_PositionTex", posTexture);
			descriptor->Set("u_ShadowDepthTexture", shadowDepthTexture);
			descriptor->Set("u_VolumetricTex", m_Data->VolumetricsTexture[i]);

			descriptor->UpdateVulkanDescriptorIfNeeded();
		}
	}

	void VulkanVolumetricPass::GaussianBlurInitData(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;

		// Pipeline creation
		ComputePipeline::CreateInfo createInfoCP{};
		createInfoCP.Shader = m_Data->GaussianBlurShader;
		if (!m_Data->GaussianBlurPipeline)
			m_Data->GaussianBlurPipeline = ComputePipeline::Create(createInfoCP);


		m_Data->GaussianBlurTexture.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			ImageSpecification imageSpec{};
			imageSpec.Width = width;
			imageSpec.Height = height;
			imageSpec.Sampler.SamplerFilter = ImageFilter::Nearest;
			imageSpec.Sampler.SamplerWrap = ImageWrap::ClampToEdge;
			imageSpec.Format = ImageFormat::RGBA8;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.UseMipChain = false;

			m_Data->GaussianBlurTexture[i] = Image2D::Create(imageSpec);
		}

		m_Data->GaussianBlurDescriptor.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			if (!m_Data->GaussianBlurDescriptor[i])
				m_Data->GaussianBlurDescriptor[i] = Material::Create(m_Data->GaussianBlurShader, "GaussianBlurMaterial");

			auto descriptor = m_Data->GaussianBlurDescriptor[i].As<VulkanMaterial>();

			descriptor->Set("i_SrcImage", m_Data->VolumetricsTexture[i]);
			descriptor->Set("o_DstImage", m_Data->GaussianBlurTexture[i]);

			descriptor->UpdateVulkanDescriptorIfNeeded();
		}
	}

	void VulkanVolumetricPass::FroxelVolumetricInitData(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;
		uint32_t froxel_ZSlices = 256;

		// Pipeline creation
		ComputePipeline::CreateInfo createInfoCP{};
		createInfoCP.Shader = m_Data->FroxelPopulateShader;
		if (!m_Data->FroxelPopulatePipeline)
			m_Data->FroxelPopulatePipeline = ComputePipeline::Create(createInfoCP);


		m_Data->ScatExtinctionFroxelTexture.resize(framesInFlight);
		m_Data->EmissionPhaseFroxelTexture.resize(framesInFlight);
		m_Data->FogVolumesDataBuffer.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			ImageSpecification imageSpec{};
			imageSpec.Width = uint32_t(width / 8.0f);
			imageSpec.Height = uint32_t(height / 8.0f);
			imageSpec.Depth = froxel_ZSlices;
			imageSpec.Sampler.SamplerFilter = ImageFilter::Nearest;
			imageSpec.Sampler.SamplerWrap = ImageWrap::ClampToEdge;
			imageSpec.Format = ImageFormat::RGBA8;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.UseMipChain = false;

			m_Data->ScatExtinctionFroxelTexture[i] = Texture3D::Create(imageSpec);
			m_Data->EmissionPhaseFroxelTexture[i] = Texture3D::Create(imageSpec);

			if(!m_Data->FogVolumesDataBuffer[i])
				m_Data->FogVolumesDataBuffer[i] = BufferDevice::Create(1024 * sizeof(FogVolumeParams), { BufferUsage::Storage });
		}

		m_Data->FroxelPopulateDescriptor.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			if (!m_Data->FroxelPopulateDescriptor[i])
				m_Data->FroxelPopulateDescriptor[i] = Material::Create(m_Data->FroxelPopulateShader, "FroxelVolumePopulateMaterial");

			auto descriptor = m_Data->FroxelPopulateDescriptor[i].As<VulkanMaterial>();

			descriptor->Set("FogVolumeData", m_Data->FogVolumesDataBuffer[i]);
			descriptor->Set("u_ScatExtinctionFroxel", m_Data->ScatExtinctionFroxelTexture[i]);
			descriptor->Set("u_EmissionPhaseFroxel", m_Data->EmissionPhaseFroxelTexture[i]);

			descriptor->UpdateVulkanDescriptorIfNeeded();
		}
	}

	void VulkanVolumetricPass::VolumetricComputeInitData(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;

		// Pipeline creation
		ComputePipeline::CreateInfo createInfoCP{};
		createInfoCP.Shader = m_Data->VolumetricComputeShader;
		if (!m_Data->VolumetricComputePipeline)
			m_Data->VolumetricComputePipeline = ComputePipeline::Create(createInfoCP);


		m_Data->VolumetricComputeTexture.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			// Compute the volumetrics at half the res because it is very expensive
			ImageSpecification imageSpec{};
			imageSpec.Width = width / 2.0f;
			imageSpec.Height = height / 2.0f;
			imageSpec.Sampler.SamplerFilter = ImageFilter::Nearest;
			imageSpec.Sampler.SamplerWrap = ImageWrap::ClampToEdge;
			imageSpec.Format = ImageFormat::RGBA8;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.UseMipChain = false;

			m_Data->VolumetricComputeTexture[i] = Image2D::Create(imageSpec);
		}

		

		m_Data->VolumetricComputeDescriptor.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			if (!m_Data->VolumetricComputeDescriptor[i])
				m_Data->VolumetricComputeDescriptor[i] = Material::Create(m_Data->VolumetricComputeShader, "VolumetricComputeMaterial");

			auto descriptor = m_Data->VolumetricComputeDescriptor[i].As<VulkanMaterial>();

			auto blueNoiseLUT = Renderer::GetBlueNoiseLut();
			auto shadowDepthTexture = m_RenderPassPipeline->GetRenderPassData<VulkanShadowPass>()->ShadowDepthRenderPass->GetColorAttachment(0, i);

			descriptor->Set("u_BlueNoiseLUT", blueNoiseLUT);
			descriptor->Set("u_ShadowDepthTexture", shadowDepthTexture);
			descriptor->Set("u_ScatExtinctionFroxel", m_Data->ScatExtinctionFroxelTexture[i]);
			descriptor->Set("u_EmissionPhaseFroxel", m_Data->EmissionPhaseFroxelTexture[i]);
			descriptor->Set("u_VolumetricTex", m_Data->VolumetricComputeTexture[i]);

			descriptor->UpdateVulkanDescriptorIfNeeded();
		}
	}

	void VulkanVolumetricPass::InitLate()
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;

		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			auto descriptor = m_Data->VolumetricComputeDescriptor[i].As<VulkanMaterial>();

			int32_t lastFrame = i - 1;
			if (i == 0)
				lastFrame = framesInFlight - 1;

			Ref<Image2D> depthTexture = m_RenderPassPipeline->GetRenderPassData<VulkanPostFXPass>()->DepthPyramid[lastFrame];

			descriptor->Set("u_DepthTexture", depthTexture);
			descriptor->UpdateVulkanDescriptorIfNeeded();
		}
	}

	void VulkanVolumetricPass::OnUpdate(const RenderQueue& renderQueue)
	{
		VolumetricsUpdate(renderQueue);
		GaussianBlurUpdate(renderQueue);
		FroxelVolumetricUpdate(renderQueue);
		VolumetricComputeUpdate(renderQueue);
	}

	struct VolumetricsPushConstant
	{
		glm::mat4 CameraViewMatrix;

		glm::vec3 DirectionalLightDir;
		float FogA = 0.02f;

		glm::vec3 CameraPosition;
		float FogB = 0.001f;
	};
	static VolumetricsPushConstant s_VolumetricsPushConstant;

	void VulkanVolumetricPass::VolumetricsUpdate(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		auto vulkanPipeline = m_Data->VolumetricsPipeline.As<VulkanComputePipeline>();
		auto vulkanDescriptor = m_Data->VolumetricsDescriptor[currentFrameIndex].As<VulkanMaterial>();
		auto volumetricTexture = m_Data->VolumetricsTexture[currentFrameIndex].As<VulkanImage2D>();

		// Shadow Cascades information
		auto shadowPassInternalData = m_RenderPassPipeline->GetRenderPassData<VulkanShadowPass>();

		glm::vec4 cascadeDepthSplit = {
			shadowPassInternalData->CascadeDepthSplit[0],
			shadowPassInternalData->CascadeDepthSplit[1],
			shadowPassInternalData->CascadeDepthSplit[2],
			shadowPassInternalData->CascadeDepthSplit[3]
		};

		vulkanDescriptor->Set("DirectionaLightData.LightViewProjMatrix0", shadowPassInternalData->CascadeViewProjMatrix[0]);
		vulkanDescriptor->Set("DirectionaLightData.LightViewProjMatrix1", shadowPassInternalData->CascadeViewProjMatrix[1]);
		vulkanDescriptor->Set("DirectionaLightData.LightViewProjMatrix2", shadowPassInternalData->CascadeViewProjMatrix[2]);
		vulkanDescriptor->Set("DirectionaLightData.LightViewProjMatrix3", shadowPassInternalData->CascadeViewProjMatrix[3]);
		vulkanDescriptor->Set("DirectionaLightData.CascadeDepthSplit", cascadeDepthSplit);

		vulkanDescriptor->Bind(cmdBuf, m_Data->VolumetricsPipeline);

		s_VolumetricsPushConstant.CameraPosition = renderQueue.CameraPosition;
		s_VolumetricsPushConstant.CameraViewMatrix = renderQueue.CameraViewMatrix;
		s_VolumetricsPushConstant.DirectionalLightDir = renderQueue.m_LightData.DirectionalLight.Direction;

		vulkanPipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &s_VolumetricsPushConstant);


		float width = renderQueue.ViewPortWidth;
		float height = renderQueue.ViewPortHeight;

		uint32_t groupX = std::ceil(width / 32.0f);
		uint32_t groupY = std::ceil(height / 32.0f);
		vulkanPipeline->Dispatch(cmdBuf, groupX, groupY, 1);

		volumetricTexture->TransitionLayout(cmdBuf, volumetricTexture->GetVulkanImageLayout(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	}

	void VulkanVolumetricPass::GaussianBlurUpdate(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		auto vulkanPipeline = m_Data->GaussianBlurPipeline.As<VulkanComputePipeline>();
		auto vulkanDescriptor = m_Data->GaussianBlurDescriptor[currentFrameIndex].As<VulkanMaterial>();

		vulkanDescriptor->Bind(cmdBuf, m_Data->GaussianBlurPipeline);

		float width = renderQueue.ViewPortWidth;
		float height = renderQueue.ViewPortHeight;

		glm::vec4 pushConstant = { width, height, 0, 0 };
		vulkanPipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &pushConstant);
		
		uint32_t groupX = std::ceil(width / 32.0f);
		uint32_t groupY = std::ceil(height / 32.0f);
		vulkanPipeline->Dispatch(cmdBuf, groupX, groupY, 1);
	}


	struct FroxelPopulatePushConstant
	{

		glm::mat4 InvViewProjMatrix;
		glm::vec3 CameraPosition;

		float FogVolumesCount;
	};
	static FroxelPopulatePushConstant s_FroxelPopulatePushConstant;

	void VulkanVolumetricPass::FroxelVolumetricUpdate(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		auto vulkanPipeline = m_Data->FroxelPopulatePipeline.As<VulkanComputePipeline>();
		auto vulkanDescriptor = m_Data->FroxelPopulateDescriptor[currentFrameIndex].As<VulkanMaterial>();

		
		m_Data->FogVolumesDataBuffer[currentFrameIndex]->SetData(sizeof(RenderQueue::FogVolume) * renderQueue.m_FogVolumeData.size(), (void*)renderQueue.m_FogVolumeData.data());


		vulkanDescriptor->Bind(cmdBuf, m_Data->FroxelPopulatePipeline);
		
		glm::mat4 cameraViewMatrix = renderQueue.m_Camera.GetViewMatrix();
		glm::mat4 cameraProjMatrix = renderQueue.m_Camera.GetProjectionMatrix();
		cameraProjMatrix[1][1] *= -1.0f;

		s_FroxelPopulatePushConstant.CameraPosition = renderQueue.CameraPosition;
		s_FroxelPopulatePushConstant.InvViewProjMatrix = glm::inverse(cameraProjMatrix * cameraViewMatrix);
		s_FroxelPopulatePushConstant.FogVolumesCount = renderQueue.m_FogVolumeData.size();
		vulkanPipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &s_FroxelPopulatePushConstant);


		float width = (renderQueue.ViewPortWidth / 8.0f);
		float height = (renderQueue.ViewPortHeight / 8.0f);

		uint32_t groupX = std::ceil(width / 8.0f);
		uint32_t groupY = std::ceil(height / 8.0f);
		uint32_t froxel_ZSlices = 256;
		vulkanPipeline->Dispatch(cmdBuf, groupX, groupY, froxel_ZSlices);
	}

	struct VolumetricComputePushConstant
	{
		glm::mat4 InvViewProjMatrix;
		glm::vec3 CameraPosition;
		float FogVolumesCount;
		glm::vec3 DirectionalLightDir;
	};
	static VolumetricComputePushConstant s_VolumetricComputePushConstant;

	void VulkanVolumetricPass::VolumetricComputeUpdate(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		auto vulkanPipeline = m_Data->VolumetricComputePipeline.As<VulkanComputePipeline>();
		auto vulkanDescriptor = m_Data->VolumetricComputeDescriptor[currentFrameIndex].As<VulkanMaterial>();

		// Push constant information
		s_VolumetricComputePushConstant.CameraPosition = renderQueue.CameraPosition;
		s_VolumetricComputePushConstant.InvViewProjMatrix = s_FroxelPopulatePushConstant.InvViewProjMatrix;
		s_VolumetricComputePushConstant.FogVolumesCount = s_FroxelPopulatePushConstant.FogVolumesCount; // Remove
		s_VolumetricComputePushConstant.DirectionalLightDir = renderQueue.m_LightData.DirectionalLight.Direction;

		// Shadow Cascades information
		auto shadowPassInternalData = m_RenderPassPipeline->GetRenderPassData<VulkanShadowPass>();

		glm::vec4 cascadeDepthSplit = {
			shadowPassInternalData->CascadeDepthSplit[0],
			shadowPassInternalData->CascadeDepthSplit[1],
			shadowPassInternalData->CascadeDepthSplit[2],
			shadowPassInternalData->CascadeDepthSplit[3]
		};

		vulkanDescriptor->Set("DirectionaLightData.LightViewProjMatrix0", shadowPassInternalData->CascadeViewProjMatrix[0]);
		vulkanDescriptor->Set("DirectionaLightData.LightViewProjMatrix1", shadowPassInternalData->CascadeViewProjMatrix[1]);
		vulkanDescriptor->Set("DirectionaLightData.LightViewProjMatrix2", shadowPassInternalData->CascadeViewProjMatrix[2]);
		vulkanDescriptor->Set("DirectionaLightData.LightViewProjMatrix3", shadowPassInternalData->CascadeViewProjMatrix[3]);
		vulkanDescriptor->Set("DirectionaLightData.CascadeDepthSplit", cascadeDepthSplit);

		// Binding the descriptor and compute pipeline
		vulkanDescriptor->Bind(cmdBuf, m_Data->VolumetricComputePipeline);
		vulkanPipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &s_VolumetricComputePushConstant);

		// Dispatch
		float width = renderQueue.ViewPortWidth;
		float height = renderQueue.ViewPortHeight;
		uint32_t groupX = std::ceil(width / 32.0f);
		uint32_t groupY = std::ceil(height / 32.0f);
		vulkanPipeline->Dispatch(cmdBuf, groupX, groupY, 1);
	}

	void VulkanVolumetricPass::OnRenderDebug()
	{
		if (ImGui::CollapsingHeader("Volumetric Lightning"))
		{
			ImGui::DragFloat("Fog Property A", &s_VolumetricsPushConstant.FogA, 0.001f, 0.0f, 1.0f);
			ImGui::DragFloat("Fog Property B", &s_VolumetricsPushConstant.FogB, 0.001f, 0.0f, 1.0f);
		}
	}

	void VulkanVolumetricPass::OnResize(uint32_t width, uint32_t height)
	{
		VolumetricsInitData(width, height);
		GaussianBlurInitData(width, height);
		FroxelVolumetricInitData(width, height);
		VolumetricComputeInitData(width, height);
	}

	void VulkanVolumetricPass::OnResizeLate(uint32_t width, uint32_t height)
	{
		InitLate();
	}

	void VulkanVolumetricPass::ShutDown()
	{
		delete m_Data;
	}
}