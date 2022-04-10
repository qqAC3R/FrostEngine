#include "frostpch.h"
#include "VulkanDynamicSkyPass.h"

#include "Frost/Renderer/Renderer.h"
#include "Frost/Platform/Vulkan/VulkanImage.h"
#include "Frost/Platform/Vulkan/VulkanContext.h"
#include "Frost/Platform/Vulkan/VulkanMaterial.h"
#include "Frost/Platform/Vulkan/VulkanPipelineCompute.h"
#include "Frost/Platform/Vulkan/VulkanTexture.h"

#include "Frost/ImGui/ImGuiLayer.h"
#include <imgui.h>

namespace Frost
{
	VulkanDynamicSkyPass::VulkanDynamicSkyPass()
		: m_Name("DynamicSkyPass")
	{
	}

	VulkanDynamicSkyPass::~VulkanDynamicSkyPass()
	{
	}

	void VulkanDynamicSkyPass::Init(SceneRenderPassPipeline* renderPassPipeline)
	{
		m_RenderPassPipeline = renderPassPipeline;
		m_Data = new InternalData();

		m_Data->TransmittanceShader = Renderer::GetShaderLibrary()->Get("Transmittance");
		m_Data->MultiScatterShader = Renderer::GetShaderLibrary()->Get("MultiScatter");
		m_Data->SkyViewShader = Renderer::GetShaderLibrary()->Get("SkyViewBuilder");
		m_Data->SkyIrradianceShader = Renderer::GetShaderLibrary()->Get("SkyViewIrradiance");
		m_Data->SkyPrefilterShader = Renderer::GetShaderLibrary()->Get("SkyViewFilter");

		TransmittanceLUT_InitData(1600, 900);
		MultiScatterLUT_InitData(1600, 900);
		SkyViewLUT_InitData(1600, 900);
		SkyIrradiance_InitData(1600, 900);
		SkyPrefilter_InitData(1600, 900);


		//TransmittanceLUT_Update({});
		//MultiScatterLUT_Update({});
		//SkyViewLUT_Update({});
		//SkyIrradiance_Update({});
		//SkyPrefilter_Update({});

	}

	void VulkanDynamicSkyPass::TransmittanceLUT_InitData(uint32_t width, uint32_t height)
	{
		// Transmittance pipeline creation
		ComputePipeline::CreateInfo computePipelineCreateInfo{};
		computePipelineCreateInfo.Shader = m_Data->TransmittanceShader;
		if (!m_Data->TransmittancePipeline)
			m_Data->TransmittancePipeline = ComputePipeline::Create(computePipelineCreateInfo);

		// Transmittance LUT
		ImageSpecification imageSpec{};
		imageSpec.Width = 256;
		imageSpec.Height = 64;
		imageSpec.Sampler.SamplerFilter = ImageFilter::Linear;
		imageSpec.Sampler.SamplerWrap = ImageWrap::ClampToEdge;
		imageSpec.Format = ImageFormat::RGBA16F;
		imageSpec.Usage = ImageUsage::Storage;

		m_Data->TransmittanceLUT = Image2D::Create(imageSpec);


		// Descriptor data
		if (!m_Data->TransmittanceDescriptor)
			m_Data->TransmittanceDescriptor = Material::Create(m_Data->TransmittanceShader, "TransmittanceShader");

		m_Data->TransmittanceDescriptor->Set("u_TransmittanceLUT", m_Data->TransmittanceLUT);
	}

	void VulkanDynamicSkyPass::MultiScatterLUT_InitData(uint32_t width, uint32_t height)
	{
		// MultiScatter pipeline creation
		ComputePipeline::CreateInfo computePipelineCreateInfo{};
		computePipelineCreateInfo.Shader = m_Data->MultiScatterShader;
		if (!m_Data->MultiScatterPipeline)
			m_Data->MultiScatterPipeline = ComputePipeline::Create(computePipelineCreateInfo);

		// Multi Scatter LUT
		ImageSpecification imageSpec{};
		imageSpec.Width = 32;
		imageSpec.Height = 32;
		imageSpec.Sampler.SamplerFilter = ImageFilter::Linear;
		imageSpec.Sampler.SamplerWrap = ImageWrap::ClampToEdge;
		imageSpec.Format = ImageFormat::RGBA16F;
		imageSpec.Usage = ImageUsage::Storage;

		m_Data->MultiScatterLUT = Image2D::Create(imageSpec);


		// Descriptor data
		if (!m_Data->MultiScatterDescriptor)
			m_Data->MultiScatterDescriptor = Material::Create(m_Data->MultiScatterShader, "MultiScatterShader");

		m_Data->MultiScatterDescriptor->Set("u_TransmittanceLUT", m_Data->TransmittanceLUT);
		m_Data->MultiScatterDescriptor->Set("u_MultiScatterLUT", m_Data->MultiScatterLUT);
	}

	void VulkanDynamicSkyPass::SkyViewLUT_InitData(uint32_t width, uint32_t height)
	{
		// SkyView pipeline creation
		ComputePipeline::CreateInfo computePipelineCreateInfo{};
		computePipelineCreateInfo.Shader = m_Data->SkyViewShader;
		if (!m_Data->SkyViewPipeline)
			m_Data->SkyViewPipeline = ComputePipeline::Create(computePipelineCreateInfo);

		// SkyView LUT
		ImageSpecification imageSpec{};
		imageSpec.Width = 256;
		imageSpec.Height = 128;
		imageSpec.Sampler.SamplerFilter = ImageFilter::Linear;
		imageSpec.Sampler.SamplerWrap = ImageWrap::ClampToEdge;
		imageSpec.Format = ImageFormat::RGBA16F;
		imageSpec.Usage = ImageUsage::Storage;

		m_Data->SkyViewLUT = Image2D::Create(imageSpec);


		// Descriptor data
		if (!m_Data->SkyViewDescriptor)
			m_Data->SkyViewDescriptor = Material::Create(m_Data->SkyViewShader, "SkyViewBuilder_Shader");

		m_Data->SkyViewDescriptor->Set("u_TransmittanceLUT", m_Data->TransmittanceLUT);
		m_Data->SkyViewDescriptor->Set("u_MultiScatterLUT", m_Data->MultiScatterLUT);
		m_Data->SkyViewDescriptor->Set("u_SkyViewImage", m_Data->SkyViewLUT);
	}

	void VulkanDynamicSkyPass::SkyIrradiance_InitData(uint32_t width, uint32_t height)
	{
		// SkyView Irradiance pipeline creation
		ComputePipeline::CreateInfo computePipelineCreateInfo{};
		computePipelineCreateInfo.Shader = m_Data->SkyIrradianceShader;
		if (!m_Data->SkyIrradiancePipeline)
			m_Data->SkyIrradiancePipeline = ComputePipeline::Create(computePipelineCreateInfo);

		// SkyView Irradiance Cubemap
		ImageSpecification imageSpec{};
		imageSpec.Width = 32;
		imageSpec.Height = 32;
		imageSpec.Format = ImageFormat::RGBA16F;
		imageSpec.Usage = ImageUsage::Storage;
		imageSpec.UseMipChain = false;

		m_Data->SkyIrradianceMap = TextureCubeMap::Create(imageSpec);


		// Descriptor data
		if (!m_Data->SkyIrradianceDescriptor)
			m_Data->SkyIrradianceDescriptor = Material::Create(m_Data->SkyIrradianceShader, "SkyViewIrradiance_Shader");

		m_Data->SkyIrradianceDescriptor->Set("u_SkyViewLUT", m_Data->SkyViewLUT);
		m_Data->SkyIrradianceDescriptor->Set("u_IrradianceMap", m_Data->SkyIrradianceMap);
	}

	void VulkanDynamicSkyPass::SkyPrefilter_InitData(uint32_t width, uint32_t height)
	{
		// SkyView Prefilter pipeline creation
		ComputePipeline::CreateInfo computePipelineCreateInfo{};
		computePipelineCreateInfo.Shader = m_Data->SkyPrefilterShader;
		if (!m_Data->SkyPrefilterPipeline)
			m_Data->SkyPrefilterPipeline = ComputePipeline::Create(computePipelineCreateInfo);

		// SkyView Prefiltered Cubemap
		ImageSpecification imageSpec{};
		imageSpec.Width = 64;
		imageSpec.Height = 64;
		imageSpec.Format = ImageFormat::RGBA16F;
		imageSpec.Usage = ImageUsage::Storage;

		m_Data->SkyPrefilterMap = TextureCubeMap::Create(imageSpec);


		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		Ref<VulkanTextureCubeMap> vulkanPrefilteredMap = m_Data->SkyPrefilterMap.As<VulkanTextureCubeMap>();
		// Descriptor data
		m_Data->SkyPrefilterDescriptor.resize(7);
		for (uint32_t i = 0; i < 7; i++)
		{

			if (!m_Data->SkyPrefilterDescriptor[i])
				m_Data->SkyPrefilterDescriptor[i] = Material::Create(m_Data->SkyPrefilterShader, "SkyViewPrefilter_Shader");

			Ref<VulkanMaterial> vulkanMaterial = m_Data->SkyPrefilterDescriptor[i].As<VulkanMaterial>();
			VkDescriptorSet descriptorSet = vulkanMaterial->GetVulkanDescriptorSet(0);

			vulkanMaterial->Set("u_SkyViewLUT", m_Data->SkyViewLUT);

			VkDescriptorImageInfo imageInfo{};
			imageInfo.imageLayout = vulkanPrefilteredMap->GetVulkanImageLayout();
			imageInfo.imageView = vulkanPrefilteredMap->GetVulkanImageViewMip(i);
			imageInfo.sampler = vulkanPrefilteredMap->GetVulkanSampler();

			VkWriteDescriptorSet writeDescriptorSet{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			writeDescriptorSet.dstBinding = 1;
			writeDescriptorSet.dstArrayElement = 0;
			writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			writeDescriptorSet.pImageInfo = &imageInfo;
			writeDescriptorSet.descriptorCount = 1;
			writeDescriptorSet.dstSet = descriptorSet;

			vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, 0);
			vulkanMaterial->UpdateVulkanDescriptorIfNeeded();
		}
	}

	void VulkanDynamicSkyPass::InitLate()
	{
	}

	static bool IsDone = true;

	void VulkanDynamicSkyPass::OnUpdate(const RenderQueue& renderQueue)
	{
		if (IsDone)
		{

			TransmittanceLUT_Update(renderQueue);
			MultiScatterLUT_Update(renderQueue);
			SkyViewLUT_Update(renderQueue);
			SkyIrradiance_Update(renderQueue);
			SkyPrefilter_Update(renderQueue);

			IsDone = false;
		}
		//ImGui::Begin("Hillaire2020");
		//ImTextureID textureId = ImGuiLayer::GetTextureIDFromVulkanTexture(m_Data->SkyViewLUT);
		//ImGui::Image(textureId, { 256, 128 });
		//ImGui::End();
	}

	void VulkanDynamicSkyPass::TransmittanceLUT_Update(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		// Getting the vulkan transmittance pipeline
		Ref<VulkanComputePipeline> vulkanTransmittancePipeline = m_Data->TransmittancePipeline.As<VulkanComputePipeline>();
		Ref<VulkanMaterial> vulkanTransmittanceMaterial = m_Data->TransmittanceDescriptor.As<VulkanMaterial>();

		vulkanTransmittanceMaterial->Bind(cmdBuf, m_Data->TransmittancePipeline);
		vulkanTransmittancePipeline->BindVulkanPushConstant(cmdBuf, "m_SkyParams", &m_SkyParams);

		vulkanTransmittancePipeline->Dispatch(cmdBuf, 256 / 8, 64 / 8, 1);

		// Memory barrier
		Ref<VulkanImage2D> transmittanceLUT = m_Data->TransmittanceLUT.As<VulkanImage2D>();
		transmittanceLUT->TransitionLayout(cmdBuf, transmittanceLUT->GetVulkanImageLayout(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

	}

	void VulkanDynamicSkyPass::MultiScatterLUT_Update(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		// Getting the vulkan multi scatter pipeline
		Ref<VulkanComputePipeline> vulkanMultiScatterPipeline = m_Data->MultiScatterPipeline.As<VulkanComputePipeline>();
		Ref<VulkanMaterial> vulkanMultiScatterMaterial = m_Data->MultiScatterDescriptor.As<VulkanMaterial>();

		vulkanMultiScatterMaterial->Bind(cmdBuf, m_Data->MultiScatterPipeline);
		vulkanMultiScatterPipeline->BindVulkanPushConstant(cmdBuf, "m_SkyParams", &m_SkyParams);

		vulkanMultiScatterPipeline->Dispatch(cmdBuf, 32, 32, 1);

		// Memory barrier
		Ref<VulkanImage2D> multiScatterLUT = m_Data->MultiScatterLUT.As<VulkanImage2D>();
		multiScatterLUT->TransitionLayout(cmdBuf, multiScatterLUT->GetVulkanImageLayout(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

	}

	void VulkanDynamicSkyPass::SkyViewLUT_Update(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		// Getting the vulkan skyview_builder pipeline
		Ref<VulkanComputePipeline> vulkanSkyViewPipeline = m_Data->SkyViewPipeline.As<VulkanComputePipeline>();
		Ref<VulkanMaterial> vulkanSkyViewMaterial = m_Data->SkyViewDescriptor.As<VulkanMaterial>();

		vulkanSkyViewMaterial->Bind(cmdBuf, m_Data->SkyViewPipeline);
		vulkanSkyViewPipeline->BindVulkanPushConstant(cmdBuf, "m_SkyParams", &m_SkyParams);

		vulkanSkyViewPipeline->Dispatch(cmdBuf, 256 / 8, 128 / 8, 1);

		// Memory barrier
		Ref<VulkanImage2D> skyViewLUT = m_Data->SkyViewLUT.As<VulkanImage2D>();
		skyViewLUT->TransitionLayout(cmdBuf, skyViewLUT->GetVulkanImageLayout(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

		skyViewLUT->GenerateMipMaps(cmdBuf, skyViewLUT->GetVulkanImageLayout());

	}
	
	void VulkanDynamicSkyPass::SkyIrradiance_Update(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		// Getting the vulkan skyview_builder pipeline
		Ref<VulkanComputePipeline> vulkanSkyIrradiancePipeline = m_Data->SkyIrradiancePipeline.As<VulkanComputePipeline>();
		Ref<VulkanMaterial> vulkanSkyIrradianceMaterial = m_Data->SkyIrradianceDescriptor.As<VulkanMaterial>();

		vulkanSkyIrradianceMaterial->Bind(cmdBuf, m_Data->SkyIrradiancePipeline);
		vulkanSkyIrradiancePipeline->BindVulkanPushConstant(cmdBuf, "m_PushConstant", &m_SkyDiffuseParams);

		vulkanSkyIrradiancePipeline->Dispatch(cmdBuf, 512 / 8, 512 / 8, 6);

		Ref<VulkanImage2D> vulkanIrradianceMap = m_Data->SkyIrradianceMap.As<VulkanImage2D>();
		vulkanIrradianceMap->TransitionLayout(cmdBuf, vulkanIrradianceMap->GetVulkanImageLayout(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
	}

	void VulkanDynamicSkyPass::SkyPrefilter_Update(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		// Getting the vulkan skyview_builder pipeline
		Ref<VulkanComputePipeline> vulkanSkyPrefilterPipeline = m_Data->SkyPrefilterPipeline.As<VulkanComputePipeline>();
		Ref<VulkanTextureCubeMap> vulkanPrefilteredMap = m_Data->SkyPrefilterMap.As<VulkanTextureCubeMap>();

		for (uint32_t i = 0; i < 7; i++)
		{
			Ref<VulkanMaterial> vulkanSkyPrefilterMaterial = m_Data->SkyPrefilterDescriptor[i].As<VulkanMaterial>();

			// Compute the current mip levels.
			uint32_t mipWidth = vulkanPrefilteredMap->GetMipWidth(i);
			uint32_t mipHeight = vulkanPrefilteredMap->GetMipHeight(i);

			uint32_t groupX = static_cast<uint32_t>(glm::ceil(static_cast<float>(mipWidth) / 8.0f));
			uint32_t groupY = static_cast<uint32_t>(glm::ceil(static_cast<float>(mipHeight) / 8.0f));

			m_SkyDiffuseParams.Roughness = static_cast<float>(i) / 4.0f;
			m_SkyDiffuseParams.NrSamples = 64;

			vulkanSkyPrefilterMaterial->Bind(cmdBuf, m_Data->SkyPrefilterPipeline);
			vulkanSkyPrefilterPipeline->BindVulkanPushConstant(cmdBuf, "m_PushConstant", &m_SkyDiffuseParams);

			vulkanSkyPrefilterPipeline->Dispatch(cmdBuf, groupX, groupY, 6);

			vulkanPrefilteredMap->TransitionLayout(cmdBuf, vulkanPrefilteredMap->GetVulkanImageLayout(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		}
	}

	void VulkanDynamicSkyPass::OnResize(uint32_t width, uint32_t height)
	{
	}

	void VulkanDynamicSkyPass::OnResizeLate(uint32_t width, uint32_t height)
	{

	}

	void VulkanDynamicSkyPass::ShutDown()
	{
		delete m_Data;
	}
}