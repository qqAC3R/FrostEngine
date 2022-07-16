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

		m_Data->FroxelPopulateShader = Renderer::GetShaderLibrary()->Get("FroxelVolumePopulate");
		m_Data->VolumetricComputeShader = Renderer::GetShaderLibrary()->Get("VolumetricCompute");
		m_Data->VolumetricBlurShader = Renderer::GetShaderLibrary()->Get("VolumetricBlur");

		FroxelVolumetricInitData(1600, 900);
		VolumetricComputeInitData(1600, 900);
		VolumetricBlurInitData(1600, 900);
	}

//#define INIT_IF_VALID(x, struc, ...) if(!x) { x = struc::Create(__VA_ARGS__); }

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
			imageSpec.Format = ImageFormat::RGBA16F;
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

	void VulkanVolumetricPass::VolumetricBlurInitData(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;

		// Pipeline creation
		ComputePipeline::CreateInfo createInfoCP{};
		createInfoCP.Shader = m_Data->VolumetricBlurShader;
		if (!m_Data->VolumetricBlurPipeline)
			m_Data->VolumetricBlurPipeline = ComputePipeline::Create(createInfoCP);


		m_Data->VolumetricBlurTexture_DirX.resize(framesInFlight);
		m_Data->VolumetricBlurTexture_DirY.resize(framesInFlight);
		m_Data->VolumetricBlurTexture_Upsample.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			// Compute the volumetrics at half the res because it is very expensive
			ImageSpecification imageSpec{};
			imageSpec.Width = width / 2.0f;
			imageSpec.Height = height / 2.0f;
			imageSpec.Sampler.SamplerFilter = ImageFilter::Linear;
			imageSpec.Sampler.SamplerWrap = ImageWrap::ClampToEdge;
			imageSpec.Format = ImageFormat::RGBA8;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.UseMipChain = false;

			m_Data->VolumetricBlurTexture_DirX[i] = Image2D::Create(imageSpec);
			m_Data->VolumetricBlurTexture_DirY[i] = Image2D::Create(imageSpec);


			imageSpec.Width = width;
			imageSpec.Height = height;
			m_Data->VolumetricBlurTexture_Upsample[i] = Image2D::Create(imageSpec);
		}



		m_Data->VolumetricBlurXDescriptor.resize(framesInFlight);
		m_Data->VolumetricBlurYDescriptor.resize(framesInFlight);
		m_Data->VolumetricBlurUpsampleDescriptor.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			if (!m_Data->VolumetricBlurXDescriptor[i])
				m_Data->VolumetricBlurXDescriptor[i] = Material::Create(m_Data->VolumetricBlurShader, "VolumetricBlurXMaterial");

			if (!m_Data->VolumetricBlurYDescriptor[i])
				m_Data->VolumetricBlurYDescriptor[i] = Material::Create(m_Data->VolumetricBlurShader, "VolumetricBlurYMaterial");

			if (!m_Data->VolumetricBlurUpsampleDescriptor[i])
				m_Data->VolumetricBlurUpsampleDescriptor[i] = Material::Create(m_Data->VolumetricBlurShader, "VolumetricBlurUpSampleMaterial");


			auto descriptorVolumetricBlurX = m_Data->VolumetricBlurXDescriptor[i].As<VulkanMaterial>();
			auto descriptorVolumetricBlurY = m_Data->VolumetricBlurYDescriptor[i].As<VulkanMaterial>();
			auto descriptorVolumetricBlurUpSample = m_Data->VolumetricBlurUpsampleDescriptor[i].As<VulkanMaterial>();


			descriptorVolumetricBlurX->Set("u_SrcTex", m_Data->VolumetricComputeTexture[i]);
			descriptorVolumetricBlurX->Set("u_DstTex", m_Data->VolumetricBlurTexture_DirX[i]);
			descriptorVolumetricBlurX->UpdateVulkanDescriptorIfNeeded();


			descriptorVolumetricBlurY->Set("u_SrcTex", m_Data->VolumetricBlurTexture_DirX[i]);
			descriptorVolumetricBlurY->Set("u_DstTex", m_Data->VolumetricBlurTexture_DirY[i]);
			descriptorVolumetricBlurY->UpdateVulkanDescriptorIfNeeded();

			descriptorVolumetricBlurUpSample->Set("u_SrcTex", m_Data->VolumetricBlurTexture_DirY[i]);
			descriptorVolumetricBlurUpSample->Set("u_DstTex", m_Data->VolumetricBlurTexture_Upsample[i]);
			descriptorVolumetricBlurUpSample->Set("u_DepthBuffer", Renderer::GetWhiteLUT()); // In the upsample pass, we don't need the depth buffer
			descriptorVolumetricBlurUpSample->UpdateVulkanDescriptorIfNeeded();
		}
	}

	void VulkanVolumetricPass::InitLate()
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;

		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			auto descriptorVolumetricCompute = m_Data->VolumetricComputeDescriptor[i].As<VulkanMaterial>();
			auto descriptorVolumetricBlurX = m_Data->VolumetricBlurXDescriptor[i].As<VulkanMaterial>();
			auto descriptorVolumetricBlurY = m_Data->VolumetricBlurYDescriptor[i].As<VulkanMaterial>();

			int32_t lastFrame = i - 1;
			if (i == 0)
				lastFrame = framesInFlight - 1;

			Ref<Image2D> depthTexture = m_RenderPassPipeline->GetRenderPassData<VulkanPostFXPass>()->DepthPyramid[lastFrame];

			// Set the volumetric compute descriptor
			descriptorVolumetricCompute->Set("u_DepthTexture", depthTexture);
			descriptorVolumetricCompute->UpdateVulkanDescriptorIfNeeded();

			// Set the volumetric blur descriptor
			descriptorVolumetricBlurX->Set("u_DepthBuffer", depthTexture);
			descriptorVolumetricBlurX->UpdateVulkanDescriptorIfNeeded();

			descriptorVolumetricBlurY->Set("u_DepthBuffer", depthTexture);
			descriptorVolumetricBlurY->UpdateVulkanDescriptorIfNeeded();

		}
	}

	void VulkanVolumetricPass::OnUpdate(const RenderQueue& renderQueue)
	{
		FroxelVolumetricUpdate(renderQueue);
		VolumetricComputeUpdate(renderQueue);
		VolumetricBlurUpdate(renderQueue);
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


	struct VolumetricBlurPushConstant {
		glm::vec2 BlurDirection; // For the blur, we should do 2 blur passes, one vertical and one horizontal
		uint32_t DepthMipSample;
		uint32_t Mode;
	};
	static VolumetricBlurPushConstant s_VolumetricPushConstant;

	void VulkanVolumetricPass::VolumetricBlurUpdate(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		auto vulkanPipeline = m_Data->VolumetricBlurPipeline.As<VulkanComputePipeline>();
		auto vulkanBlurXDescriptor = m_Data->VolumetricBlurXDescriptor[currentFrameIndex].As<VulkanMaterial>();
		auto vulkanBlurYDescriptor = m_Data->VolumetricBlurYDescriptor[currentFrameIndex].As<VulkanMaterial>();
		auto vulkanBlurUpsampleDescriptor = m_Data->VolumetricBlurUpsampleDescriptor[currentFrameIndex].As<VulkanMaterial>();

		
		/// We firstly blur in the X direction
		s_VolumetricPushConstant.BlurDirection = { 1.0f, 0.0f };
		s_VolumetricPushConstant.DepthMipSample = 1;
		s_VolumetricPushConstant.Mode = 0; // 0 - MODE_BLUR

		vulkanBlurXDescriptor->Bind(cmdBuf, m_Data->VolumetricBlurPipeline);
		vulkanPipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &s_VolumetricPushConstant);

		
		// Dispatch
		float width = renderQueue.ViewPortWidth;
		float height = renderQueue.ViewPortHeight;
		uint32_t groupX = std::ceil((width / 2.0f) / 16.0f);
		uint32_t groupY = std::ceil((height / 2.0f) / 16.0f);
		vulkanPipeline->Dispatch(cmdBuf, groupX, groupY, 1);


		// Image barrier for the volumetric direction X blur texture
		Ref<VulkanImage2D> volumetricBlurDirXTex = m_Data->VolumetricBlurTexture_DirX[currentFrameIndex].As<VulkanImage2D>();
		volumetricBlurDirXTex->TransitionLayout(cmdBuf, volumetricBlurDirXTex->GetVulkanImageLayout(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);


		/// Then blur in the Y direction
		s_VolumetricPushConstant.BlurDirection = { 0.0f, 1.0f };

		vulkanBlurYDescriptor->Bind(cmdBuf, m_Data->VolumetricBlurPipeline);
		vulkanPipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &s_VolumetricPushConstant);

		// Dispatch
		groupX = std::ceil((width / 2.0f) / 16.0f);
		groupY = std::ceil((height / 2.0f) / 16.0f);
		vulkanPipeline->Dispatch(cmdBuf, groupX, groupY, 1);

		// Image barrier for the volumetric direction X blur texture
		Ref<VulkanImage2D> volumetricBlurDirYTex = m_Data->VolumetricBlurTexture_DirY[currentFrameIndex].As<VulkanImage2D>();
		volumetricBlurDirYTex->TransitionLayout(cmdBuf, volumetricBlurDirYTex->GetVulkanImageLayout(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);


		/// Upsample!
		s_VolumetricPushConstant.Mode = 1; // 1 - MODE_UPSAMPLE

		vulkanBlurUpsampleDescriptor->Bind(cmdBuf, m_Data->VolumetricBlurPipeline);
		vulkanPipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &s_VolumetricPushConstant);

		// Dispatch
		groupX = std::ceil(width / 16.0f);
		groupY = std::ceil(height / 16.0f);
		vulkanPipeline->Dispatch(cmdBuf, groupX, groupY, 1);

		// Image barrier for the volumetric direction X blur texture
		Ref<VulkanImage2D> volumetricBlurUpsample = m_Data->VolumetricBlurTexture_Upsample[currentFrameIndex].As<VulkanImage2D>();
		volumetricBlurUpsample->TransitionLayout(cmdBuf, volumetricBlurUpsample->GetVulkanImageLayout(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	}

	void VulkanVolumetricPass::OnRenderDebug()
	{
	}

	void VulkanVolumetricPass::OnResize(uint32_t width, uint32_t height)
	{
		FroxelVolumetricInitData(width, height);
		VolumetricComputeInitData(width, height);
		VolumetricBlurInitData(width, height);
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