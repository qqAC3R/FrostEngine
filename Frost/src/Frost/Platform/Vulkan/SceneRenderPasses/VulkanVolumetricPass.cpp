#include "frostpch.h"
#include "VulkanVolumetricPass.h"

#include "Frost/Platform/Vulkan/VulkanContext.h"

#include "Frost/Platform/Vulkan/VulkanImage.h"
#include "Frost/Platform/Vulkan/VulkanTexture.h"
#include "Frost/Platform/Vulkan/VulkanMaterial.h"
#include "Frost/Platform/Vulkan/VulkanPipelineCompute.h"
#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanShadowPass.h"
#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanCompositePass.h"
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
		m_Data->FroxelLightInjectShader = Renderer::GetShaderLibrary()->Get("VolumetricInjectLight");
		m_Data->FroxelFinalComputeShader = Renderer::GetShaderLibrary()->Get("VolumetricGatherLight");
		m_Data->FroxelTAAShader = Renderer::GetShaderLibrary()->Get("VolumetricTAA");

		m_Data->VolumetricComputeShader = Renderer::GetShaderLibrary()->Get("VolumetricCompute");
		m_Data->VolumetricBlurShader = Renderer::GetShaderLibrary()->Get("VolumetricBlur");

		m_Data->WoorleyNoiseShader = Renderer::GetShaderLibrary()->Get("CloudWoorleyNoise");
		m_Data->PerlinNoiseShader = Renderer::GetShaderLibrary()->Get("CloudPerlinNoise");
		m_Data->CloudComputeShader = Renderer::GetShaderLibrary()->Get("CloudComputeVolumetric");

		CloudNoiseCompute(1600, 900);
		CloudComputeInitData(1600, 900);

		FroxelPopulateInitData(1600, 900);
		FroxelLightInjectInitData(1600, 900);
		FroxelTAAInitData(1600, 900);
		FroxelFinalComputeInitData(1600, 900);
		VolumetricComputeInitData(1600, 900);

		VolumetricBlurInitData(1600, 900);
	}

//#define INIT_IF_VALID(x, struc, ...) if(!x) { x = struc::Create(__VA_ARGS__); }

	void VulkanVolumetricPass::FroxelPopulateInitData(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;
		uint32_t froxel_ZSlices = Renderer::GetRendererConfig().VoluemtricFroxelSlicesZ;

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
				m_Data->FogVolumesDataBuffer[i] = BufferDevice::Create(1024 * sizeof(RenderQueue::FogVolume), { BufferUsage::Storage });
		}

		m_Data->FroxelPopulateDescriptor.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			if (!m_Data->FroxelPopulateDescriptor[i])
				m_Data->FroxelPopulateDescriptor[i] = Material::Create(m_Data->FroxelPopulateShader, "FroxelVolumePopulateMaterial");

			auto descriptor = m_Data->FroxelPopulateDescriptor[i].As<VulkanMaterial>();
			auto temporalBlueNoiseLUT = Renderer::GetTemporalNoiseLut();

			descriptor->Set("u_TemporalBlueNoiseLUT", temporalBlueNoiseLUT);
			descriptor->Set("FogVolumeData", m_Data->FogVolumesDataBuffer[i]);
			descriptor->Set("u_ScatExtinctionFroxel", m_Data->ScatExtinctionFroxelTexture[i]);
			descriptor->Set("u_EmissionPhaseFroxel", m_Data->EmissionPhaseFroxelTexture[i]);

			descriptor->UpdateVulkanDescriptorIfNeeded();
		}
	}

	void VulkanVolumetricPass::FroxelLightInjectInitData(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;

		// Pipeline creation
		ComputePipeline::CreateInfo createInfoCP{};
		createInfoCP.Shader = m_Data->FroxelLightInjectShader;
		if (!m_Data->FroxelLightInjectPipeline)
			m_Data->FroxelLightInjectPipeline = ComputePipeline::Create(createInfoCP);

		m_Data->FroxelLightInjectDescriptor.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			if (!m_Data->FroxelLightInjectDescriptor[i])
				m_Data->FroxelLightInjectDescriptor[i] = Material::Create(m_Data->FroxelLightInjectShader, "FroxelLightInjectMaterial");

			auto descriptor = m_Data->FroxelLightInjectDescriptor[i].As<VulkanMaterial>();

			Ref<Image2D> shadowDepthTexture = m_RenderPassPipeline->GetRenderPassData<VulkanShadowPass>()->ShadowDepthRenderPass->GetColorAttachment(0, i);
			Ref<Texture2D> temporalBlueNoiseLUT = Renderer::GetTemporalNoiseLut();

			Ref<BufferDevice> pointLightBuffer = m_RenderPassPipeline->GetRenderPassData<VulkanCompositePass>()->PointLightBufferData[i].DeviceBuffer;
			Ref<BufferDevice> pointLightIndicesBuffer = m_RenderPassPipeline->GetRenderPassData<VulkanCompositePass>()->PointLightIndicesVolumetric[i];

			descriptor->Set("u_TemporalBlueNoiseLUT", temporalBlueNoiseLUT);
			descriptor->Set("u_ShadowDepthTexture", shadowDepthTexture);
			descriptor->Set("u_ScatExtinctionFroxel", m_Data->ScatExtinctionFroxelTexture[i]);
			descriptor->Set("u_EmissionPhaseFroxel", m_Data->EmissionPhaseFroxelTexture[i]);
			descriptor->Set("u_ScatExtinctionFroxel_Output", m_Data->ScatExtinctionFroxelTexture[i]); // The same volume texture, but outputting on the texel that we are reading,
																									  // so no need to sync
			descriptor->Set("LightData", pointLightBuffer);
			descriptor->Set("VisibleLightData", pointLightIndicesBuffer);

			descriptor->UpdateVulkanDescriptorIfNeeded();
		}
	}

	void VulkanVolumetricPass::FroxelTAAInitData(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;
		uint32_t froxel_ZSlices = Renderer::GetRendererConfig().VoluemtricFroxelSlicesZ;

		// Pipeline creation
		ComputePipeline::CreateInfo createInfoCP{};
		createInfoCP.Shader = m_Data->FroxelTAAShader;
		if (!m_Data->FroxelTAAPipeline)
			m_Data->FroxelTAAPipeline = ComputePipeline::Create(createInfoCP);

		m_Data->FroxelResolveTAATexture.resize(framesInFlight);
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

			m_Data->FroxelResolveTAATexture[i] = Texture3D::Create(imageSpec);
		}

		m_Data->FroxelTAADescriptor.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			if (!m_Data->FroxelTAADescriptor[i])
				m_Data->FroxelTAADescriptor[i] = Material::Create(m_Data->FroxelTAAShader, "FroxelTAAMaterial");

			auto descriptor = m_Data->FroxelTAADescriptor[i].As<VulkanMaterial>();

			int32_t lastFrame = i - 1;
			if (i == 0) lastFrame = framesInFlight - 1;

			descriptor->Set("u_PreviousVolumetricFroxel", m_Data->FroxelResolveTAATexture[lastFrame]);
			descriptor->Set("u_CurrentVolumetricFroxel", m_Data->ScatExtinctionFroxelTexture[i]);
			descriptor->Set("u_ResolveVolumetricFroxel", m_Data->FroxelResolveTAATexture[i]); // Using the same volume texture `u_CurrentVolumetricFroxel`,
																							  // because we don't more because we are only reading and writting at one texel per 

			descriptor->UpdateVulkanDescriptorIfNeeded();
		}
	}

	void VulkanVolumetricPass::FroxelFinalComputeInitData(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;

		// Pipeline creation
		ComputePipeline::CreateInfo createInfoCP{};
		createInfoCP.Shader = m_Data->FroxelFinalComputeShader;
		if (!m_Data->FroxelFinalComputePipeline)
			m_Data->FroxelFinalComputePipeline = ComputePipeline::Create(createInfoCP);

		m_Data->FroxelFinalComputeDescriptor.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			if (!m_Data->FroxelFinalComputeDescriptor[i])
				m_Data->FroxelFinalComputeDescriptor[i] = Material::Create(m_Data->FroxelFinalComputeShader, "FroxelFinalComputeMaterial");

			auto descriptor = m_Data->FroxelFinalComputeDescriptor[i].As<VulkanMaterial>();

			//descriptor->Set("u_LightExtinctionFroxel", m_Data->ScatExtinctionFroxelTexture[i]);
			descriptor->Set("u_LightExtinctionFroxel", m_Data->FroxelResolveTAATexture[i]);
			descriptor->Set("u_FinalLightFroxel", m_Data->EmissionPhaseFroxelTexture[i]); // We won't create a new volume texture,
																						  // instead use the one that we don't need anymore (after retrieving the data volume data)

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
			imageSpec.Sampler.SamplerFilter = ImageFilter::Linear;
			imageSpec.Sampler.SamplerWrap = ImageWrap::ClampToEdge;
			imageSpec.Format = ImageFormat::RGBA16F;
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

			auto spatialBlueNoiseLUT = Renderer::GetSpatialBlueNoiseLut();

			descriptor->Set("u_SpatialBlueNoiseLUT", spatialBlueNoiseLUT);
			descriptor->Set("u_FinalGatherFroxel", m_Data->EmissionPhaseFroxelTexture[i]);
			descriptor->Set("u_VolumetricTex", m_Data->VolumetricComputeTexture[i]);
			//descriptor->Set("u_CloudComputeTex", m_Data->CloudComputeTexture[i]);

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
		//m_Data->VolumetricBlurTexture_Upsample.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			// Compute the volumetrics at half the res because it is very expensive
			ImageSpecification imageSpec{};
			imageSpec.Width = width / 2.0f;
			imageSpec.Height = height / 2.0f;
			imageSpec.Sampler.SamplerFilter = ImageFilter::Linear;
			imageSpec.Sampler.SamplerWrap = ImageWrap::ClampToEdge;
			imageSpec.Format = ImageFormat::RGBA16F;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.UseMipChain = false;

			m_Data->VolumetricBlurTexture_DirX[i] = Image2D::Create(imageSpec);
			m_Data->VolumetricBlurTexture_DirY[i] = Image2D::Create(imageSpec);


			//imageSpec.Width = width;
			//imageSpec.Height = height;
			//m_Data->VolumetricBlurTexture_Upsample[i] = Image2D::Create(imageSpec);
		}



		m_Data->VolumetricBlurXDescriptor.resize(framesInFlight);
		m_Data->VolumetricBlurYDescriptor.resize(framesInFlight);
		//m_Data->VolumetricBlurUpsampleDescriptor.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			if (!m_Data->VolumetricBlurXDescriptor[i])
				m_Data->VolumetricBlurXDescriptor[i] = Material::Create(m_Data->VolumetricBlurShader, "VolumetricBlurXMaterial");

			if (!m_Data->VolumetricBlurYDescriptor[i])
				m_Data->VolumetricBlurYDescriptor[i] = Material::Create(m_Data->VolumetricBlurShader, "VolumetricBlurYMaterial");

			//if (!m_Data->VolumetricBlurUpsampleDescriptor[i])
			//	m_Data->VolumetricBlurUpsampleDescriptor[i] = Material::Create(m_Data->VolumetricBlurShader, "VolumetricBlurUpSampleMaterial");


			auto descriptorVolumetricBlurX = m_Data->VolumetricBlurXDescriptor[i].As<VulkanMaterial>();
			auto descriptorVolumetricBlurY = m_Data->VolumetricBlurYDescriptor[i].As<VulkanMaterial>();
			//auto descriptorVolumetricBlurUpSample = m_Data->VolumetricBlurUpsampleDescriptor[i].As<VulkanMaterial>();


			descriptorVolumetricBlurX->Set("u_SrcTex", m_Data->VolumetricComputeTexture[i]);
			descriptorVolumetricBlurX->Set("u_DstTex", m_Data->VolumetricBlurTexture_DirX[i]);
			descriptorVolumetricBlurX->UpdateVulkanDescriptorIfNeeded();


			descriptorVolumetricBlurY->Set("u_SrcTex", m_Data->VolumetricBlurTexture_DirX[i]);
			descriptorVolumetricBlurY->Set("u_DstTex", m_Data->VolumetricBlurTexture_DirY[i]);
			descriptorVolumetricBlurY->UpdateVulkanDescriptorIfNeeded();

			//descriptorVolumetricBlurUpSample->Set("u_SrcTex", m_Data->VolumetricBlurTexture_DirY[i]);
			//descriptorVolumetricBlurUpSample->Set("u_DstTex", m_Data->VolumetricBlurTexture_Upsample[i]);
			//descriptorVolumetricBlurUpSample->Set("u_DepthBuffer", Renderer::GetWhiteLUT()); // In the upsample pass, we don't need the depth buffer
			//descriptorVolumetricBlurUpSample->UpdateVulkanDescriptorIfNeeded();
		}
	}

	void VulkanVolumetricPass::CloudNoiseCompute(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;

		{
			ComputePipeline::CreateInfo createInfoCP{};
			createInfoCP.Shader = m_Data->PerlinNoiseShader;
			m_Data->PerlinNoisePipeline = ComputePipeline::Create(createInfoCP);

			ImageSpecification imageSpec{};
			imageSpec.Width = 128;
			imageSpec.Height = 128;
			imageSpec.Depth = 128;
			imageSpec.Sampler.SamplerFilter = ImageFilter::Linear;
			imageSpec.Sampler.SamplerWrap = ImageWrap::Repeat;
			imageSpec.Format = ImageFormat::RGBA8;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.UseMipChain = false;
			m_Data->CloudNoiseTexture = Texture3D::Create(imageSpec);

			m_Data->PerlinNoiseDescriptor = Material::Create(m_Data->PerlinNoiseShader, "CloudNoiseMaterial");
			m_Data->PerlinNoiseDescriptor->Set("u_CloudNoiseTex", m_Data->CloudNoiseTexture);
		}
		{
			ComputePipeline::CreateInfo createInfoCP{};
			createInfoCP.Shader = m_Data->WoorleyNoiseShader;
			m_Data->WoorleyNoisePipeline = ComputePipeline::Create(createInfoCP);

			ImageSpecification imageSpec{};
			imageSpec.Width = 32;
			imageSpec.Height = 32;
			imageSpec.Depth = 32;
			imageSpec.Sampler.SamplerFilter = ImageFilter::Linear;
			imageSpec.Sampler.SamplerWrap = ImageWrap::Repeat;
			imageSpec.Format = ImageFormat::RGBA8;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.UseMipChain = false;
			m_Data->ErroderNoiseTexture = Texture3D::Create(imageSpec);

			m_Data->WoorleyNoiseDescriptor = Material::Create(m_Data->WoorleyNoiseShader, "ErroderNoiseMaterial");
			m_Data->WoorleyNoiseDescriptor->Set("u_ErroderNoiseTex", m_Data->ErroderNoiseTexture);
		}



		// Recording a temporary commandbuffer
		VkCommandBuffer cmdBuf = VulkanContext::GetCurrentDevice()->AllocateCommandBuffer(RenderQueueType::Graphics, true);


		// Main noise texture
		Ref<VulkanComputePipeline> cloudNoisePipeline = m_Data->PerlinNoisePipeline.As<VulkanComputePipeline>();
		Ref<VulkanMaterial> cloudNoiseDescriptor = m_Data->PerlinNoiseDescriptor.As<VulkanComputePipeline>();
		Ref<VulkanTexture3D> cloudNoiseTexture = m_Data->CloudNoiseTexture.As<VulkanTexture3D>();

		cloudNoiseDescriptor->Bind(cmdBuf, m_Data->PerlinNoisePipeline);
		cloudNoisePipeline->Dispatch(cmdBuf, 128 / 4, 128 / 4, 128 / 4);

		cloudNoiseTexture->TransitionLayout(cmdBuf, cloudNoiseTexture->GetVulkanImageLayout(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

		cloudNoiseTexture->GenerateMipMaps(cmdBuf, cloudNoiseTexture->GetVulkanImageLayout());

		// Erroder
		Ref<VulkanComputePipeline> erroderNoisePipeline = m_Data->WoorleyNoisePipeline.As<VulkanComputePipeline>();
		Ref<VulkanMaterial> erroderNoiseDescriptor = m_Data->WoorleyNoiseDescriptor.As<VulkanComputePipeline>();
		Ref<VulkanTexture3D> erroderNoiseTexture = m_Data->ErroderNoiseTexture.As<VulkanTexture3D>();

		erroderNoiseDescriptor->Bind(cmdBuf, m_Data->WoorleyNoisePipeline);
		erroderNoisePipeline->Dispatch(cmdBuf, 32 / 4, 32 / 4, 32 / 4);

		erroderNoiseTexture->TransitionLayout(cmdBuf, erroderNoiseTexture->GetVulkanImageLayout(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
		erroderNoiseTexture->GenerateMipMaps(cmdBuf, erroderNoiseTexture->GetVulkanImageLayout());

		// Ending the temporary commandbuffer
		VulkanContext::GetCurrentDevice()->FlushCommandBuffer(cmdBuf);
	}

	void VulkanVolumetricPass::CloudComputeInitData(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;

		// Pipeline creation
		ComputePipeline::CreateInfo createInfoCP{};
		createInfoCP.Shader = m_Data->CloudComputeShader;
		if (!m_Data->CloudComputePipeline)
			m_Data->CloudComputePipeline = ComputePipeline::Create(createInfoCP);


		m_Data->CloudComputeTexture.resize(framesInFlight);
		m_Data->CloudVolumesDataBuffer.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			ImageSpecification imageSpec{};
			imageSpec.Width = width / 2.0f;
			imageSpec.Height = height / 2.0f;
			imageSpec.Sampler.SamplerFilter = ImageFilter::Linear;
			imageSpec.Sampler.SamplerWrap = ImageWrap::ClampToEdge;
			imageSpec.Format = ImageFormat::RGBA8;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.UseMipChain = false;

			m_Data->CloudComputeTexture[i] = Image2D::Create(imageSpec);

			if (!m_Data->CloudVolumesDataBuffer[i])
				m_Data->CloudVolumesDataBuffer[i] = BufferDevice::Create(1024 * sizeof(RenderQueue::CloudVolume), { BufferUsage::Storage });
		}



		m_Data->CloudComputeDescriptor.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			if (!m_Data->CloudComputeDescriptor[i])
				m_Data->CloudComputeDescriptor[i] = Material::Create(m_Data->CloudComputeShader, "CloudComputeMaterial");

			auto descriptor = m_Data->CloudComputeDescriptor[i].As<VulkanMaterial>();

			Ref<Image2D> depthTexture = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->GeometryRenderPass->GetDepthAttachment(i);
			Ref<Texture2D> blueNoise = Renderer::GetSpatialBlueNoiseLut();

			descriptor->Set("u_DepthBuffer", depthTexture);
			descriptor->Set("u_CloudTexture", m_Data->CloudComputeTexture[i]);
			descriptor->Set("u_CloudNoiseTex", m_Data->CloudNoiseTexture);
			descriptor->Set("u_ErroderTex", m_Data->ErroderNoiseTexture);
			descriptor->Set("u_SpatialBlueNoiseLUT", blueNoise);
			descriptor->Set("CloudVolumeData", m_Data->CloudVolumesDataBuffer[i]);
			descriptor->UpdateVulkanDescriptorIfNeeded();
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
			if (i == 0) lastFrame = framesInFlight - 1;

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
		if (m_Data->m_UseVolumetrics)
		{
			CloudComputeUpdate(renderQueue);

			FroxelPopulateUpdate(renderQueue);
			FroxelLightInjectUpdate(renderQueue);

			if (m_Data->m_UseTAA)
			{
				FroxelTAAUpdate(renderQueue);
			}

			FroxelFinalComputeUpdate(renderQueue);
			VolumetricComputeUpdate(renderQueue);
			VolumetricBlurUpdate(renderQueue);
		}
	}

	struct FroxelPopulatePushConstant
	{
		glm::mat4 InvViewProjMatrix;
		glm::vec3 CameraPosition;
		float FogVolumesCount;
		float Time = 0.0f;
	};
	static FroxelPopulatePushConstant s_FroxelPopulatePushConstant;

	void VulkanVolumetricPass::FroxelPopulateUpdate(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		auto vulkanPipeline = m_Data->FroxelPopulatePipeline.As<VulkanComputePipeline>();
		auto vulkanDescriptor = m_Data->FroxelPopulateDescriptor[currentFrameIndex].As<VulkanMaterial>();

		
		m_Data->FogVolumesDataBuffer[currentFrameIndex]->SetData(sizeof(RenderQueue::FogVolume) * renderQueue.m_FogVolumeData.size(), (void*)renderQueue.m_FogVolumeData.data());


		vulkanDescriptor->Bind(cmdBuf, m_Data->FroxelPopulatePipeline);
		
		// Push constans
		glm::mat4 cameraViewMatrix = renderQueue.m_Camera.GetViewMatrix();
		glm::mat4 cameraProjMatrix = renderQueue.m_Camera.GetProjectionMatrix();
		cameraProjMatrix[1][1] *= -1.0f;

		s_FroxelPopulatePushConstant.CameraPosition = renderQueue.CameraPosition;
		s_FroxelPopulatePushConstant.InvViewProjMatrix = glm::inverse(cameraProjMatrix * cameraViewMatrix);
		s_FroxelPopulatePushConstant.FogVolumesCount = renderQueue.m_FogVolumeData.size();

		s_FroxelPopulatePushConstant.Time++;
		if (s_FroxelPopulatePushConstant.Time > 128.0f) s_FroxelPopulatePushConstant.Time = 0.0f;

		vulkanPipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &s_FroxelPopulatePushConstant);


		// Dispatch
		float width = (renderQueue.ViewPortWidth / 8.0f);
		float height = (renderQueue.ViewPortHeight / 8.0f);

		uint32_t groupX = std::ceil(width / 8.0f);
		uint32_t groupY = std::ceil(height / 8.0f);
		uint32_t froxel_ZSlices = Renderer::GetRendererConfig().VoluemtricFroxelSlicesZ;
		vulkanPipeline->Dispatch(cmdBuf, groupX, groupY, froxel_ZSlices);

		// Volume texture barrier
		Ref<VulkanTexture3D> vulkanVolumeTexture = m_Data->EmissionPhaseFroxelTexture[currentFrameIndex].As<VulkanTexture3D>();
		vulkanVolumeTexture->TransitionLayout(cmdBuf, vulkanVolumeTexture->GetVulkanImageLayout(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	}

	struct VolumetricLightInjectPushConstant
	{
		glm::mat4 InvViewProjMatrix;
		glm::vec3 CameraPosition;
		float Time = 0.0f;
		glm::vec3 DirectionalLightDir;
		float LightCullingWorkgroupX;
		glm::vec2 ViewportSize;
		float PointLightCount;
	};
	static VolumetricLightInjectPushConstant s_VolumetricLightInjectPushConstant;

	void VulkanVolumetricPass::FroxelLightInjectUpdate(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		
		auto vulkanPipeline = m_Data->FroxelLightInjectPipeline.As<VulkanComputePipeline>();
		auto vulkanDescriptor = m_Data->FroxelLightInjectDescriptor[currentFrameIndex].As<VulkanMaterial>();

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

		vulkanDescriptor->Set("DirectionaLightData.MieScattering", renderQueue.m_LightData.DirectionalLight.Color);
		vulkanDescriptor->Set("DirectionaLightData.Density", renderQueue.m_LightData.DirectionalLight.VolumeDensity);
		vulkanDescriptor->Set("DirectionaLightData.Absorption", renderQueue.m_LightData.DirectionalLight.Absorption);
		vulkanDescriptor->Set("DirectionaLightData.Phase", renderQueue.m_LightData.DirectionalLight.Phase);
		vulkanDescriptor->Set("DirectionaLightData.Intensity", renderQueue.m_LightData.DirectionalLight.Intensity);

		// Binding the descriptor
		vulkanDescriptor->Bind(cmdBuf, m_Data->FroxelLightInjectPipeline);

		// Updating push constant
		s_VolumetricLightInjectPushConstant.InvViewProjMatrix = s_FroxelPopulatePushConstant.InvViewProjMatrix;
		s_VolumetricLightInjectPushConstant.CameraPosition = s_FroxelPopulatePushConstant.CameraPosition;
		s_VolumetricLightInjectPushConstant.DirectionalLightDir = renderQueue.m_LightData.DirectionalLight.Direction;
		s_VolumetricLightInjectPushConstant.Time = s_FroxelPopulatePushConstant.Time;
		s_VolumetricLightInjectPushConstant.PointLightCount = (uint32_t)renderQueue.m_LightData.PointLights.size();
		s_VolumetricLightInjectPushConstant.ViewportSize = { renderQueue.ViewPortWidth, renderQueue.ViewPortHeight };
		s_VolumetricLightInjectPushConstant.LightCullingWorkgroupX = std::ceil(renderQueue.ViewPortWidth / 16.0f);

		vulkanPipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &s_VolumetricLightInjectPushConstant);

		// Dispatch
		float width = (renderQueue.ViewPortWidth / 8.0f);
		float height = (renderQueue.ViewPortHeight / 8.0f);

		uint32_t groupX = std::ceil(width / 8.0f);
		uint32_t groupY = std::ceil(height / 8.0f);
		uint32_t froxel_ZSlices = Renderer::GetRendererConfig().VoluemtricFroxelSlicesZ;
		vulkanPipeline->Dispatch(cmdBuf, groupX, groupY, froxel_ZSlices);

		// Volume texture barrier
		Ref<VulkanTexture3D> vulkanVolumeTexture = m_Data->ScatExtinctionFroxelTexture[currentFrameIndex].As<VulkanTexture3D>();
		vulkanVolumeTexture->TransitionLayout(cmdBuf, vulkanVolumeTexture->GetVulkanImageLayout(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	}

	struct VolumetricTAAPushConstant
	{
		glm::mat4 InvViewProjMatrix{ 0.0f };
		glm::mat4 PreviousViewProjMatrix{ 0.0f };
		glm::mat4 PreviousInvViewProjMatrix{ 0.0f };

		glm::vec3 CameraPosition;
		float Padding0 = 0.0f;
		glm::vec3 PreviousCameraPosition;
	};
	static VolumetricTAAPushConstant s_VolumetricTAAPushConstant;

	void VulkanVolumetricPass::FroxelTAAUpdate(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		auto vulkanPipeline = m_Data->FroxelTAAPipeline.As<VulkanComputePipeline>();
		auto vulkanDescriptor = m_Data->FroxelTAADescriptor[currentFrameIndex].As<VulkanMaterial>();

		// Binding the descriptor
		vulkanDescriptor->Bind(cmdBuf, m_Data->FroxelTAAPipeline);

		// // Updating push constant
		// Previous frame stuff
		s_VolumetricTAAPushConstant.PreviousCameraPosition = s_VolumetricTAAPushConstant.CameraPosition;
		s_VolumetricTAAPushConstant.PreviousInvViewProjMatrix = s_VolumetricTAAPushConstant.InvViewProjMatrix;
		s_VolumetricTAAPushConstant.PreviousViewProjMatrix = glm::inverse(s_VolumetricTAAPushConstant.InvViewProjMatrix);

		// Current frame stuff
		s_VolumetricTAAPushConstant.InvViewProjMatrix = s_FroxelPopulatePushConstant.InvViewProjMatrix;
		s_VolumetricTAAPushConstant.CameraPosition = s_FroxelPopulatePushConstant.CameraPosition;

		vulkanPipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &s_VolumetricTAAPushConstant);

		// Dispatch
		float width = (renderQueue.ViewPortWidth / 8.0f);
		float height = (renderQueue.ViewPortHeight / 8.0f);

		uint32_t groupX = std::ceil(width / 8.0f);
		uint32_t groupY = std::ceil(height / 8.0f);
		vulkanPipeline->Dispatch(cmdBuf, groupX, groupY, 128);

		Ref<VulkanTexture3D> vulkanVolumeTexture = m_Data->FroxelResolveTAATexture[currentFrameIndex].As<VulkanTexture3D>();
		vulkanVolumeTexture->TransitionLayout(cmdBuf, vulkanVolumeTexture->GetVulkanImageLayout(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	}

	void VulkanVolumetricPass::FroxelFinalComputeUpdate(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		auto vulkanPipeline = m_Data->FroxelFinalComputePipeline.As<VulkanComputePipeline>();
		auto vulkanDescriptor = m_Data->FroxelFinalComputeDescriptor[currentFrameIndex].As<VulkanMaterial>();

		// Binding the descriptor
		vulkanDescriptor->Bind(cmdBuf, m_Data->FroxelFinalComputePipeline);

		// Updating push constant
		vulkanPipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &s_VolumetricLightInjectPushConstant); // The same push constant as in the `FroxelLightInject` pass

		// Dispatch
		float width = (renderQueue.ViewPortWidth / 8.0f);
		float height = (renderQueue.ViewPortHeight / 8.0f);

		uint32_t groupX = std::ceil(width / 8.0f);
		uint32_t groupY = std::ceil(height / 8.0f);
		vulkanPipeline->Dispatch(cmdBuf, groupX, groupY, 1);

		Ref<VulkanTexture3D> vulkanVolumeTexture = m_Data->EmissionPhaseFroxelTexture[currentFrameIndex].As<VulkanTexture3D>();
		vulkanVolumeTexture->TransitionLayout(cmdBuf, vulkanVolumeTexture->GetVulkanImageLayout(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	}

	struct VolumetricComputePushConstant
	{
		glm::mat4 InvViewProjMatrix;
		glm::vec3 CameraPosition;
		float Padding0 = 0.0f;
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
		s_VolumetricComputePushConstant.DirectionalLightDir = renderQueue.m_LightData.DirectionalLight.Direction;

		// Binding the descriptor and compute pipeline
		vulkanDescriptor->Bind(cmdBuf, m_Data->VolumetricComputePipeline);
		vulkanPipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &s_VolumetricComputePushConstant);

		// Dispatch
		float width = renderQueue.ViewPortWidth;
		float height = renderQueue.ViewPortHeight;
		uint32_t groupX = std::ceil((width  / 2.0f) / 32.0f);
		uint32_t groupY = std::ceil((height / 2.0f) / 32.0f);
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
		//auto vulkanBlurUpsampleDescriptor = m_Data->VolumetricBlurUpsampleDescriptor[currentFrameIndex].As<VulkanMaterial>();

		
		/// We firstly blur in the X direction
		s_VolumetricPushConstant.BlurDirection = { 1.0f, 0.0f };
		s_VolumetricPushConstant.DepthMipSample = 1;
		s_VolumetricPushConstant.Mode = 0; // 0 - MODE_BLUR

		vulkanBlurXDescriptor->Bind(cmdBuf, m_Data->VolumetricBlurPipeline);
		vulkanPipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &s_VolumetricPushConstant);

		
		// Dispatch
		float width = renderQueue.ViewPortWidth;
		float height = renderQueue.ViewPortHeight;
		uint32_t groupX = std::ceil((width  / 2.0f) / 16.0f);
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
		vulkanPipeline->Dispatch(cmdBuf, groupX, groupY, 1);

		// Image barrier for the volumetric direction X blur texture
		Ref<VulkanImage2D> volumetricBlurDirYTex = m_Data->VolumetricBlurTexture_DirY[currentFrameIndex].As<VulkanImage2D>();
		volumetricBlurDirYTex->TransitionLayout(cmdBuf, volumetricBlurDirYTex->GetVulkanImageLayout(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);


		/*
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
		*/
	}

	struct CloudComputePushConstant
	{
		glm::mat4 InvViewProjMatrix;
		glm::vec3 CameraPosition;

		float NearPlane;

		glm::vec3 DirectionaLightDir;

		float FarPlane;
		float CloudsCount = 0;
	};
	static CloudComputePushConstant s_CloudComputePushConstant;

	void VulkanVolumetricPass::CloudComputeUpdate(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		m_Data->CloudVolumesDataBuffer[currentFrameIndex]->SetData(sizeof(RenderQueue::CloudVolume) * renderQueue.m_CloudVolumeData.size(), (void*)renderQueue.m_CloudVolumeData.data());

		auto vulkanPipeline = m_Data->CloudComputePipeline.As<VulkanComputePipeline>();
		auto vulkanDescriptor = m_Data->CloudComputeDescriptor[currentFrameIndex].As<VulkanMaterial>();

		// Push constant information
		s_CloudComputePushConstant.InvViewProjMatrix = s_FroxelPopulatePushConstant.InvViewProjMatrix;
		s_CloudComputePushConstant.CameraPosition = renderQueue.CameraPosition;
		s_CloudComputePushConstant.NearPlane = renderQueue.m_Camera.GetNearClip();
		s_CloudComputePushConstant.FarPlane = renderQueue.m_Camera.GetFarClip();
		s_CloudComputePushConstant.CloudsCount = renderQueue.m_CloudVolumeData.size();
		s_CloudComputePushConstant.DirectionaLightDir = renderQueue.m_LightData.DirectionalLight.Direction;


		// Binding the descriptor and compute pipeline
		vulkanDescriptor->Bind(cmdBuf, m_Data->CloudComputePipeline);
		vulkanPipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &s_CloudComputePushConstant);

		// Dispatch
		float width = renderQueue.ViewPortWidth;
		float height = renderQueue.ViewPortHeight;
		uint32_t groupX = std::ceil((width  / 2.0f) / 16.0f);
		uint32_t groupY = std::ceil((height / 2.0f) / 16.0f);
		vulkanPipeline->Dispatch(cmdBuf, groupX, groupY, 1);
	}

	void VulkanVolumetricPass::OnRenderDebug()
	{
		if (ImGui::CollapsingHeader("Unified Volumetrics"))
		{
			ImGui::SliderInt("Enable", &m_Data->m_UseVolumetrics, 0, 1);
			ImGui::SliderInt("TAA", &m_Data->m_UseTAA, 0, 1);
		}
	}

	void VulkanVolumetricPass::OnResize(uint32_t width, uint32_t height)
	{
		CloudComputeInitData(width, height);

		FroxelPopulateInitData(width, height);
		FroxelLightInjectInitData(width, height);
		FroxelTAAInitData(width, height);
		FroxelFinalComputeInitData(width, height);
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