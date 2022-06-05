#include "frostpch.h"
#include "VulkanPostFXPass.h"

#include "Frost/Core/Application.h"
#include "Frost/Renderer/Renderer.h"
#include "Frost/Platform/Vulkan/VulkanRenderer.h"
#include "Frost/Platform/Vulkan/VulkanContext.h"
#include "Frost/Platform/Vulkan/VulkanMaterial.h"
#include "Frost/Platform/Vulkan/VulkanImage.h"
#include "Frost/Platform/Vulkan/VulkanPipelineCompute.h"
#include "Frost/Platform/Vulkan/VulkanSceneEnvironment.h"
#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanGeometryPass.h"
#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanCompositePass.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "imgui.h"

namespace Frost
{
#define TARGET_BLOOM     0
#define TARGET_COMPOSITE 1

	VulkanPostFXPass::VulkanPostFXPass()
		: m_Name("Post-ProcessingPass")
	{
	}

	VulkanPostFXPass::~VulkanPostFXPass()
	{
	}

	void VulkanPostFXPass::Init(SceneRenderPassPipeline* renderPassPipeline)
	{
		m_RenderPassPipeline = renderPassPipeline;
		m_Data = new InternalData();

		m_Data->SSRShader = Renderer::GetShaderLibrary()->Get("SSCT");
		m_Data->BlurShader = Renderer::GetShaderLibrary()->Get("GaussianBlur");
		m_Data->HZBShader = Renderer::GetShaderLibrary()->Get("HiZBufferBuilder");
		m_Data->VisibilityShader = Renderer::GetShaderLibrary()->Get("VisibilityBuffer");
		m_Data->AO_Shader = Renderer::GetShaderLibrary()->Get("GroundTruthAO_V2");
		m_Data->DenoiserShader = Renderer::GetShaderLibrary()->Get("SpatialDenoiser");
		m_Data->BloomShader = Renderer::GetShaderLibrary()->Get("Bloom");
		m_Data->ApplyAerialShader = Renderer::GetShaderLibrary()->Get("ApplyAerial");
		m_Data->ColorCorrectionShader = Renderer::GetShaderLibrary()->Get("ColorCorrection");

		CalculateMipLevels       (1600, 900);
		Bloom_InitData           (1600, 900);
		ApplyAerial_InitData     (1600, 900);
		ColorCorrection_InitData (1600, 900);
		HZB_InitData             (1600, 900);
		BlurColorBuffer_InitData (1600, 900);
		Visibility_InitData      (1600, 900);
		AO_InitData              (1600, 900);
		SpatialDenoiser_InitData (1600, 900);
		SSR_InitData             (1600, 900);
	}

	void VulkanPostFXPass::InitLate()
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;

		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			auto vulkanColorCorrectionDescriptor = m_Data->ColorCorrectionDescriptor[i].As<VulkanMaterial>();

			vulkanColorCorrectionDescriptor->Set("u_SSRTexture", m_Data->SSRTexture[i]);
			vulkanColorCorrectionDescriptor->Set("u_AOTexture", m_Data->DenoiserImage[i]);
			vulkanColorCorrectionDescriptor->UpdateVulkanDescriptorIfNeeded();
		}
	}

	void VulkanPostFXPass::BlurColorBuffer_InitData(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		uint32_t mipLevels = m_Data->ScreenMipLevel;

		{
			ComputePipeline::CreateInfo computePipelineCreateInfo{};
			computePipelineCreateInfo.Shader = m_Data->BlurShader;

			if(!m_Data->BlurPipeline)
				m_Data->BlurPipeline = ComputePipeline::Create(computePipelineCreateInfo);
		}

		m_Data->BlurredColorBuffer.resize(framesInFlight);
		for (auto& blurredColorBuffer : m_Data->BlurredColorBuffer)
		{
			ImageSpecification imageSpec{};
			imageSpec.Format = ImageFormat::RGBA8;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.Sampler.SamplerFilter = ImageFilter::Linear;
			imageSpec.Sampler.SamplerWrap = ImageWrap::Repeat;
			imageSpec.Width = width;
			imageSpec.Height = height;
			blurredColorBuffer = Image2D::Create(imageSpec);
		}


		m_Data->BlurShaderDescriptor.resize(framesInFlight);
		for (uint32_t j = 0; j < framesInFlight; j++)
		{
			m_Data->BlurShaderDescriptor[j].resize(mipLevels);
			for (uint32_t mipLevel = 0; mipLevel < mipLevels; mipLevel++)
			{
				auto& descriptor = m_Data->BlurShaderDescriptor[j][mipLevel];

				if(!descriptor)
					descriptor = Material::Create(m_Data->BlurShader, "GaussianBlur");

				auto& vulkanDescriptor = descriptor.As<VulkanMaterial>();
				VkDescriptorSet descriptorSet = vulkanDescriptor->GetVulkanDescriptorSet(0);


				/// Setting up `i_SrcImage`
				if (mipLevel == 0)
				{
					// The first mip is being blurred from the color buffer (not from other blurred mips)
					//Ref<Image2D> colorBuffer = m_RenderPassPipeline->GetRenderPassData<VulkanCompositePass>()->RenderPass->GetColorAttachment(0, j);
					Ref<Image2D> colorBuffer = m_Data->ColorCorrectionTexture[j];
					Ref<VulkanImage2D> vulkanColorBuffer = colorBuffer.As<VulkanImage2D>();


					VkDescriptorImageInfo imageDescriptorInfo{};
					imageDescriptorInfo.imageView = vulkanColorBuffer->GetVulkanImageView();
					imageDescriptorInfo.imageLayout = vulkanColorBuffer->GetVulkanImageLayout();
					imageDescriptorInfo.sampler = vulkanColorBuffer->GetVulkanSampler();

					VkWriteDescriptorSet writeDescriptorSet{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
					writeDescriptorSet.dstBinding = 0;
					writeDescriptorSet.dstArrayElement = 0;
					writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
					writeDescriptorSet.pImageInfo = &imageDescriptorInfo;
					writeDescriptorSet.descriptorCount = 1;
					writeDescriptorSet.dstSet = descriptorSet;

					vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
				}
				else
				{
					Ref<VulkanImage2D> vulkanBlurredColorBuffer = m_Data->BlurredColorBuffer[j].As<VulkanImage2D>();

					VkDescriptorImageInfo imageDescriptorInfo{};
					imageDescriptorInfo.imageView = vulkanBlurredColorBuffer->GetVulkanImageViewMip(mipLevel - 1);
					imageDescriptorInfo.imageLayout = vulkanBlurredColorBuffer->GetVulkanImageLayout();
					imageDescriptorInfo.sampler = vulkanBlurredColorBuffer->GetVulkanSampler();

					VkWriteDescriptorSet writeDescriptorSet{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
					writeDescriptorSet.dstBinding = 0;
					writeDescriptorSet.dstArrayElement = 0;
					writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
					writeDescriptorSet.pImageInfo = &imageDescriptorInfo;
					writeDescriptorSet.descriptorCount = 1;
					writeDescriptorSet.dstSet = descriptorSet;

					vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
				}


				/// Setting up `o_DstImage`
				{

					Ref<VulkanImage2D> vulkanBlurredColorBuffer = m_Data->BlurredColorBuffer[j].As<VulkanImage2D>();

					VkDescriptorImageInfo imageDescriptorInfo{};
					imageDescriptorInfo.imageView = vulkanBlurredColorBuffer->GetVulkanImageViewMip(mipLevel);
					imageDescriptorInfo.imageLayout = vulkanBlurredColorBuffer->GetVulkanImageLayout();
					imageDescriptorInfo.sampler = vulkanBlurredColorBuffer->GetVulkanSampler();

					VkWriteDescriptorSet writeDescriptorSet{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
					writeDescriptorSet.dstBinding = 1;
					writeDescriptorSet.dstArrayElement = 0;
					writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
					writeDescriptorSet.pImageInfo = &imageDescriptorInfo;
					writeDescriptorSet.descriptorCount = 1;
					writeDescriptorSet.dstSet = descriptorSet;

					vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
				}

			}
		}
	}

	void VulkanPostFXPass::SSR_InitData(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;


		{
			ComputePipeline::CreateInfo computePipelineCreateInfo{};
			computePipelineCreateInfo.Shader = m_Data->SSRShader;

			if(!m_Data->SSRPipeline)
				m_Data->SSRPipeline = ComputePipeline::Create(computePipelineCreateInfo);
		}

		m_Data->SSRTexture.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			ImageSpecification imageSpec{};
			imageSpec.Format = ImageFormat::RGBA8;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.Sampler.SamplerFilter = ImageFilter::Linear;
			imageSpec.Width = width;
			imageSpec.Height = height;
			m_Data->SSRTexture[i] = Image2D::Create(imageSpec);
		}

		

		m_Data->SSRDescriptor.resize(framesInFlight);
		for(uint32_t i = 0; i < framesInFlight; i++)
		{
			if(!m_Data->SSRDescriptor[i])
				m_Data->SSRDescriptor[i] = Material::Create(m_Data->SSRShader, "Post-ProceesingFX");

			auto& vulkanMaterial = m_Data->SSRDescriptor[i].As<VulkanMaterial>();

			auto viewPosTexture = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->RenderPass->GetColorAttachment(3, i);
			auto normalTexture = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->RenderPass->GetColorAttachment(1, i);
			//auto finalImage = m_RenderPassPipeline->GetRenderPassData<VulkanCompositePass>()->RenderPass->GetColorAttachment(0, i);

			// Textures
			vulkanMaterial->Set("u_ColorFrameTex", m_Data->ColorCorrectionTexture[i]);
			vulkanMaterial->Set("u_ViewPosTex", viewPosTexture);
			vulkanMaterial->Set("u_HiZBuffer", m_Data->DepthPyramid[i]);
			vulkanMaterial->Set("u_NormalTex", normalTexture);
			vulkanMaterial->Set("o_FrameTex", m_Data->SSRTexture[i]);
			vulkanMaterial->Set("u_PrefilteredColorBuffer", m_Data->BlurredColorBuffer[i]);
			vulkanMaterial->Set("u_VisibilityBuffer", m_Data->VisibilityImage[i]);
			vulkanMaterial->Set("u_AOBuffer", m_Data->DenoiserImage[i]);
			vulkanMaterial->Set("u_BloomTexture", m_Data->Bloom_UpsampledTexture[i]);

			// Uniform buffer data
			vulkanMaterial->Set("UniformBuffer.ScreenSize", glm::vec4(width, height, 0.0f, 0.0f));

			vulkanMaterial->UpdateVulkanDescriptorIfNeeded();
		}
	}

	void VulkanPostFXPass::HZB_InitData(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		uint32_t mipLevels = m_Data->ScreenMipLevel;

		{
			// Pipeline creation
			ComputePipeline::CreateInfo computePipelineCreateInfo{};
			computePipelineCreateInfo.Shader = m_Data->HZBShader;
			if(!m_Data->HZBPipeline)
				m_Data->HZBPipeline = ComputePipeline::Create(computePipelineCreateInfo);
		}

		// Calculating the previous frame index
		m_Data->DepthPyramid.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{

			ImageSpecification imageSpec{};
			imageSpec.Width = width;
			imageSpec.Height = height;
			//imageSpec.Sampler.ReductionMode_Optional = ReductionMode::Min;
			imageSpec.Sampler.SamplerFilter = ImageFilter::Nearest;
			imageSpec.Sampler.SamplerWrap = ImageWrap::ClampToEdge;

			imageSpec.Format = ImageFormat::R32;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.UseMipChain = true;

			m_Data->DepthPyramid[i] = Image2D::Create(imageSpec);
		}

		m_Data->HZBDescriptor.resize(framesInFlight);
		for (uint32_t frame = 0; frame < framesInFlight; frame++)
		{
			m_Data->HZBDescriptor[frame].resize(mipLevels);
			for (uint32_t mip = 0; mip < mipLevels; mip++)
			{
				auto& material = m_Data->HZBDescriptor[frame][mip];

				if(!material)
					material = Material::Create(m_Data->HZBShader, "Hi-Z_Buffer_Builder");

				auto& vulkanDescriptor = material.As<VulkanMaterial>();
				auto& vulkanDepthPyramid = m_Data->DepthPyramid[frame].As<VulkanImage2D>();

				VkDescriptorSet descriptorSet = vulkanDescriptor->GetVulkanDescriptorSet(0);

				// If we use the first descriptor set, we only will set the `o_Depth` texture (because the input will be last's frame depth buffer)
				if (mip == 0)
				{
					// `o_Depth`
					VkDescriptorImageInfo imageDescriptorInfo{};
					imageDescriptorInfo.imageView = vulkanDepthPyramid->GetVulkanImageViewMip(0);
					imageDescriptorInfo.imageLayout = vulkanDepthPyramid->GetVulkanImageLayout();
					imageDescriptorInfo.sampler = vulkanDepthPyramid->GetVulkanSampler();

					VkWriteDescriptorSet writeDescriptorSet{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
					writeDescriptorSet.dstBinding = 1;
					writeDescriptorSet.dstArrayElement = 0;
					writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
					writeDescriptorSet.pImageInfo = &imageDescriptorInfo;
					writeDescriptorSet.descriptorCount = 1;
					writeDescriptorSet.dstSet = descriptorSet;

					vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);

					// `i_Depth`
					auto renderPass = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->RenderPass;
					auto geometryDepthAttachment = renderPass->GetDepthAttachment(frame);

					vulkanDescriptor->Set("i_Depth", geometryDepthAttachment);
					vulkanDescriptor->UpdateVulkanDescriptorIfNeeded();

					continue;
				}


				// Setting up `i_Depth`
				{
					VkDescriptorImageInfo imageDescriptorInfo{};
					imageDescriptorInfo.imageView = vulkanDepthPyramid->GetVulkanImageViewMip(mip - 1);
					imageDescriptorInfo.imageLayout = vulkanDepthPyramid->GetVulkanImageLayout();
					imageDescriptorInfo.sampler = vulkanDepthPyramid->GetVulkanSampler();

					VkWriteDescriptorSet writeDescriptorSet{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
					writeDescriptorSet.dstBinding = 0;
					writeDescriptorSet.dstArrayElement = 0;
					writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
					writeDescriptorSet.pImageInfo = &imageDescriptorInfo;
					writeDescriptorSet.descriptorCount = 1;
					writeDescriptorSet.dstSet = descriptorSet;

					vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
				}

				// Setting up `o_Depth`
				{
					VkDescriptorImageInfo imageDescriptorInfo{};
					imageDescriptorInfo.imageView = vulkanDepthPyramid->GetVulkanImageViewMip(mip);
					imageDescriptorInfo.imageLayout = vulkanDepthPyramid->GetVulkanImageLayout();
					imageDescriptorInfo.sampler = vulkanDepthPyramid->GetVulkanSampler();

					VkWriteDescriptorSet writeDescriptorSet{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
					writeDescriptorSet.dstBinding = 1;
					writeDescriptorSet.dstArrayElement = 0;
					writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
					writeDescriptorSet.pImageInfo = &imageDescriptorInfo;
					writeDescriptorSet.descriptorCount = 1;
					writeDescriptorSet.dstSet = descriptorSet;

					vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
				}

			}
		}

	}

	void VulkanPostFXPass::Visibility_InitData(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		uint32_t mipLevels = m_Data->ScreenMipLevel;

		{
			// Pipeline creation
			ComputePipeline::CreateInfo computePipelineCreateInfo{};
			computePipelineCreateInfo.Shader = m_Data->VisibilityShader;
			if (!m_Data->VisibilityPipeline)
				m_Data->VisibilityPipeline = ComputePipeline::Create(computePipelineCreateInfo);
		}

		// Calculating the previous frame index
		m_Data->VisibilityImage.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{

			ImageSpecification imageSpec{};
			imageSpec.Width = width;
			imageSpec.Height = height;
			//imageSpec.Sampler.ReductionMode_Optional = ReductionMode::Min;
			imageSpec.Sampler.SamplerFilter = ImageFilter::Linear;
			imageSpec.Sampler.SamplerWrap = ImageWrap::Repeat;

			imageSpec.Format = ImageFormat::R32;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.UseMipChain = true;

			m_Data->VisibilityImage[i] = Image2D::Create(imageSpec);
		}

		m_Data->VisibilityDescriptor.resize(framesInFlight);
		for (uint32_t frame = 0; frame < framesInFlight; frame++)
		{
			m_Data->VisibilityDescriptor[frame].resize(mipLevels);
			for (uint32_t mip = 0; mip < mipLevels; mip++)
			{
				auto& material = m_Data->VisibilityDescriptor[frame][mip];

				if (!material)
					material = Material::Create(m_Data->VisibilityShader, "Hi-Z_Buffer_Builder");

				auto& vulkanDescriptor = material.As<VulkanMaterial>();
				auto& vulkanVisBuffer = m_Data->VisibilityImage[frame].As<VulkanImage2D>();
				auto& vulkanDepthPyramid = m_Data->DepthPyramid[frame].As<VulkanImage2D>();

				VkDescriptorSet descriptorSet = vulkanDescriptor->GetVulkanDescriptorSet(0);

				// The first mip of the visibility buffer should be white (which is what we do in the compute shader)
				if (mip == 0)
				{
					// Depth pyramid
					VkDescriptorImageInfo imageDescriptorInfo{};
					imageDescriptorInfo.imageLayout = vulkanVisBuffer->GetVulkanImageLayout();
					imageDescriptorInfo.imageView = vulkanVisBuffer->GetVulkanImageViewMip(mip);
					imageDescriptorInfo.sampler = vulkanVisBuffer->GetVulkanSampler();

					VkWriteDescriptorSet writeDescriptorSet{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
					writeDescriptorSet.dstBinding = 2;
					writeDescriptorSet.dstArrayElement = 0;
					writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
					writeDescriptorSet.pImageInfo = &imageDescriptorInfo;
					writeDescriptorSet.descriptorCount = 1;
					writeDescriptorSet.dstSet = descriptorSet;

					vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);


					vulkanDescriptor->Set("i_DepthTex", m_Data->DepthPyramid[frame]);
					vulkanDescriptor->Set("i_LastVisBuffer", m_Data->DepthPyramid[frame]);
					vulkanDescriptor->UpdateVulkanDescriptorIfNeeded();

					continue;
				}

				// Setting up 'i_DepthTex'
				{
					VkDescriptorImageInfo imageDescriptorInfo{};
					imageDescriptorInfo.imageLayout = vulkanDepthPyramid->GetVulkanImageLayout();
					imageDescriptorInfo.imageView = vulkanDepthPyramid->GetVulkanImageView();
					imageDescriptorInfo.sampler = vulkanDepthPyramid->GetVulkanSampler();

					VkWriteDescriptorSet writeDescriptorSet{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
					writeDescriptorSet.dstBinding = 0;
					writeDescriptorSet.dstArrayElement = 0;
					writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
					writeDescriptorSet.pImageInfo = &imageDescriptorInfo;
					writeDescriptorSet.descriptorCount = 1;
					writeDescriptorSet.dstSet = descriptorSet;

					vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
				}

				// Setting up 'i_LastVisBuffer'
				{
					VkDescriptorImageInfo imageDescriptorInfo{};
					imageDescriptorInfo.imageLayout = vulkanVisBuffer->GetVulkanImageLayout();
					imageDescriptorInfo.imageView = vulkanVisBuffer->GetVulkanImageViewMip(mip - 1);
					imageDescriptorInfo.sampler = vulkanVisBuffer->GetVulkanSampler();

					VkWriteDescriptorSet writeDescriptorSet{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
					writeDescriptorSet.dstBinding = 1;
					writeDescriptorSet.dstArrayElement = 0;
					writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
					writeDescriptorSet.pImageInfo = &imageDescriptorInfo;
					writeDescriptorSet.descriptorCount = 1;
					writeDescriptorSet.dstSet = descriptorSet;

					vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
				}

				// Setting up 'o_VisBuffer'
				{
					VkDescriptorImageInfo imageDescriptorInfo{};
					imageDescriptorInfo.imageLayout = vulkanVisBuffer->GetVulkanImageLayout();
					imageDescriptorInfo.imageView = vulkanVisBuffer->GetVulkanImageViewMip(mip);
					imageDescriptorInfo.sampler = vulkanVisBuffer->GetVulkanSampler();

					VkWriteDescriptorSet writeDescriptorSet{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
					writeDescriptorSet.dstBinding = 2;
					writeDescriptorSet.dstArrayElement = 0;
					writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
					writeDescriptorSet.pImageInfo = &imageDescriptorInfo;
					writeDescriptorSet.descriptorCount = 1;
					writeDescriptorSet.dstSet = descriptorSet;

					vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
				}

			}
		}
	}

	void VulkanPostFXPass::AO_InitData(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		{
			// Pipeline creation
			ComputePipeline::CreateInfo computePipelineCreateInfo{};
			computePipelineCreateInfo.Shader = m_Data->AO_Shader;
			if (!m_Data->AO_Pipeline)
				m_Data->AO_Pipeline = ComputePipeline::Create(computePipelineCreateInfo);
		}


		m_Data->AO_Image.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			ImageSpecification imageSpec{};
			imageSpec.Width = width;
			imageSpec.Height = height;
			imageSpec.Sampler.SamplerFilter = ImageFilter::Nearest;
			imageSpec.Sampler.SamplerWrap = ImageWrap::ClampToEdge;

			imageSpec.Format = ImageFormat::R32;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.UseMipChain = false;

			m_Data->AO_Image[i] = Image2D::Create(imageSpec);
		}

		m_Data->AO_Descriptor.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			auto& descriptor = m_Data->AO_Descriptor[i];

			if (!descriptor)
				descriptor = Material::Create(m_Data->AO_Shader, "GTAO");

			auto& vulkanDescriptor = descriptor.As<VulkanMaterial>();

			auto noiseTexture = Renderer::GetNoiseLut();
			auto viewPosTexture = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->RenderPass->GetColorAttachment(3, i);
			auto normalTexture = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->RenderPass->GetColorAttachment(1, i);

			descriptor->Set("u_ViewPositionTex", viewPosTexture);
			descriptor->Set("u_DepthPyramid", m_Data->DepthPyramid[i]);
			descriptor->Set("u_NormalsTex", normalTexture);
			descriptor->Set("u_NoiseTex", noiseTexture);
			
			descriptor->Set("o_AOTexture", m_Data->AO_Image[i]);

			vulkanDescriptor->UpdateVulkanDescriptorIfNeeded();

		}
	}

	void VulkanPostFXPass::SpatialDenoiser_InitData(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		{
			// Pipeline creation
			ComputePipeline::CreateInfo computePipelineCreateInfo{};
			computePipelineCreateInfo.Shader = m_Data->DenoiserShader;
			if (!m_Data->DenoiserPipeline)
				m_Data->DenoiserPipeline = ComputePipeline::Create(computePipelineCreateInfo);
		}

		m_Data->DenoiserImage.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			ImageSpecification imageSpec{};
			imageSpec.Width = width;
			imageSpec.Height = height;
			imageSpec.Sampler.SamplerFilter = ImageFilter::Nearest;
			imageSpec.Sampler.SamplerWrap = ImageWrap::ClampToEdge;

			imageSpec.Format = ImageFormat::R32;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.UseMipChain = false;

			m_Data->DenoiserImage[i] = Image2D::Create(imageSpec);
		}

		m_Data->DenoiserDescriptor.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			auto& descriptor = m_Data->DenoiserDescriptor[i];

			if (!descriptor)
				descriptor = Material::Create(m_Data->DenoiserShader, "SpatialDenosier");

			auto& vulkanDescriptor = descriptor.As<VulkanMaterial>();

			vulkanDescriptor->Set("i_Texture", m_Data->AO_Image[i]);
			vulkanDescriptor->Set("o_Texture", m_Data->DenoiserImage[i]);

			vulkanDescriptor->UpdateVulkanDescriptorIfNeeded();
		}

	}

	void VulkanPostFXPass::Bloom_InitData(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();


		{
			// Pipeline creation
			ComputePipeline::CreateInfo computePipelineCreateInfo{};
			computePipelineCreateInfo.Shader = m_Data->BloomShader;
			if (!m_Data->BloomPipeline)
				m_Data->BloomPipeline = ComputePipeline::Create(computePipelineCreateInfo);
		}

		m_Data->Bloom_DownsampledTexture.resize(framesInFlight);
		m_Data->Bloom_UpsampledTexture.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			ImageSpecification imageSpec{};
			imageSpec.Width = width;
			imageSpec.Height = height;
			imageSpec.Sampler.SamplerFilter = ImageFilter::Linear;
			imageSpec.Sampler.SamplerWrap = ImageWrap::ClampToEdge;

			imageSpec.Format = ImageFormat::RGBA16F;
			imageSpec.Usage = ImageUsage::Storage;

			m_Data->Bloom_DownsampledTexture[i] = Image2D::Create(imageSpec);
			m_Data->Bloom_UpsampledTexture[i] = Image2D::Create(imageSpec);
		}

		m_Data->BloomDescriptor.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			if (!m_Data->BloomDescriptor[i])
				m_Data->BloomDescriptor[i] = Material::Create(m_Data->BloomShader, "Bloom");
		}
	}

	void VulkanPostFXPass::ApplyAerial_InitData(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		// Pipeline creation
		ComputePipeline::CreateInfo computePipelineCreateInfo{};
		computePipelineCreateInfo.Shader = m_Data->ApplyAerialShader;
		if (!m_Data->ApplyAerialPipeline)
			m_Data->ApplyAerialPipeline = ComputePipeline::Create(computePipelineCreateInfo);


		m_Data->ApplyAerialImage.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			ImageSpecification imageSpec{};
			imageSpec.Width = width;
			imageSpec.Height = height;
			imageSpec.Sampler.SamplerFilter = ImageFilter::Linear;
			imageSpec.Sampler.SamplerWrap = ImageWrap::ClampToEdge;

			imageSpec.Format = ImageFormat::RGBA8;
			imageSpec.Usage = ImageUsage::Storage;

			m_Data->ApplyAerialImage[i] = Image2D::Create(imageSpec);
		}

		m_Data->ApplyAerialDescriptor.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			if (!m_Data->ApplyAerialDescriptor[i])
				m_Data->ApplyAerialDescriptor[i] = Material::Create(m_Data->ApplyAerialShader, "ApplyAerialPerspective");

			Ref<VulkanSceneEnvironment> vulkanSceneEnv = Renderer::GetSceneEnvironment().As<VulkanSceneEnvironment>();
			Ref<VulkanMaterial> vulkanApplyAerialDescriptor = m_Data->ApplyAerialDescriptor[i].As<VulkanMaterial>();

			Ref<Texture3D> aerialLUT = vulkanSceneEnv->GetAerialPerspectiveLUT();
			Ref<Image2D> depthBuffer = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->RenderPass->GetDepthAttachment(i);
			Ref<Image2D> viewPos = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->RenderPass->GetColorAttachment(3, i);
			
			vulkanApplyAerialDescriptor->Set("u_AerialLUT", aerialLUT);
			vulkanApplyAerialDescriptor->Set("u_ViewPos", viewPos);
			vulkanApplyAerialDescriptor->Set("u_DepthBuffer", depthBuffer);
			vulkanApplyAerialDescriptor->Set("o_Image", m_Data->ApplyAerialImage[i]);

			vulkanApplyAerialDescriptor->UpdateVulkanDescriptorIfNeeded();
		}
	}

	void VulkanPostFXPass::ColorCorrection_InitData(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;
		uint32_t mipLevels = m_Data->ScreenMipLevel;

		{
			ComputePipeline::CreateInfo computePipelineCreateInfo{};
			computePipelineCreateInfo.Shader = m_Data->ColorCorrectionShader;

			if (!m_Data->ColorCorrectionPipeline)
				m_Data->ColorCorrectionPipeline = ComputePipeline::Create(computePipelineCreateInfo);
		}

		m_Data->ColorCorrectionTexture.resize(framesInFlight);
		m_Data->FinalTexture.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			ImageSpecification imageSpec{};
			imageSpec.Format = ImageFormat::RGBA8;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.Sampler.SamplerFilter = ImageFilter::Linear;
			imageSpec.Sampler.SamplerWrap = ImageWrap::Repeat;
			imageSpec.Width = width;
			imageSpec.Height = height;

			m_Data->ColorCorrectionTexture[i] = Image2D::Create(imageSpec);

			imageSpec.Sampler.SamplerFilter = ImageFilter::Nearest;
			m_Data->FinalTexture[i] = Image2D::Create(imageSpec);
		}

		m_Data->ColorCorrectionDescriptor.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			if (!m_Data->ColorCorrectionDescriptor[i])
				m_Data->ColorCorrectionDescriptor[i] = Material::Create(m_Data->ColorCorrectionShader, "ColorCorrection");

			auto vulkanColorCorrectionDescriptor = m_Data->ColorCorrectionDescriptor[i].As<VulkanMaterial>();

			auto colorBuffer = m_RenderPassPipeline->GetRenderPassData<VulkanCompositePass>()->RenderPass->GetColorAttachment(0, i); // From the pbr shader

			vulkanColorCorrectionDescriptor->Set("u_ColorFrameTexture", colorBuffer);
			vulkanColorCorrectionDescriptor->Set("u_BloomTexture", m_Data->Bloom_UpsampledTexture[i]);
			//vulkanColorCorrectionDescriptor->Set("u_SSRTexture", m_Data->SSRTexture[i]);
			vulkanColorCorrectionDescriptor->Set("o_Texture_ForSSR", m_Data->ColorCorrectionTexture[i]);
			vulkanColorCorrectionDescriptor->Set("o_Texture_Final", m_Data->FinalTexture[i]);
			vulkanColorCorrectionDescriptor->Set("o_AerialImage", m_Data->ApplyAerialImage[i]);

			vulkanColorCorrectionDescriptor->UpdateVulkanDescriptorIfNeeded();
		}
	}


	void VulkanPostFXPass::OnUpdate(const RenderQueue& renderQueue)
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		auto finalImage = m_RenderPassPipeline->GetRenderPassData<VulkanCompositePass>()->RenderPass->GetColorAttachment(0, currentFrameIndex).As<VulkanImage2D>();
		finalImage->TransitionLayout(cmdBuf, finalImage->GetVulkanImageLayout());

		Bloom_Update(renderQueue);
		ColorCorrection_Update(renderQueue, TARGET_BLOOM);
		HZB_Update(renderQueue);
		BlurColorBuffer_Update(renderQueue);
		Visibility_Update(renderQueue);
		AO_Update(renderQueue);
		SpatialDenoiser_Update(renderQueue);
		SSR_Update(renderQueue);
		ApplyAerial_Update(renderQueue);
		ColorCorrection_Update(renderQueue, TARGET_COMPOSITE);
	}

	void VulkanPostFXPass::OnRenderDebug()
	{
		if(ImGui::CollapsingHeader("Ambient Occlusion"))
		{
			ImGui::Text("AO Mode (0 - HBAO; 1 - GTAO)");
			ImGui::SliderInt(" ", &m_AOSettings.AOMode, 0, 1);

			ImGui::Text("AO Texture");
			uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
			Application::Get().GetImGuiLayer()->RenderTexture(m_Data->DenoiserImage[currentFrameIndex], 300, 200);
		}
		if (ImGui::CollapsingHeader("Bloom"))
		{
			ImGui::DragFloat("Threshold", &m_BloomSettings.Threshold, 0.1f, 0.0f, 1000.0f);
			ImGui::SliderFloat("Knee", &m_BloomSettings.Knee, 0.1f, 1.0f);
			ImGui::DragFloat("Intensity", &m_BloomSettings.Intensity, 0.1f, 0.0f, 1000.0f);
		}
	}

	void VulkanPostFXPass::BlurColorBuffer_Update(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		auto vulkan_BlurPipeline = m_Data->BlurPipeline.As<VulkanComputePipeline>();
		auto vulkan_BlurColorTexture = m_Data->BlurredColorBuffer[currentFrameIndex].As<VulkanImage2D>();

		glm::vec4 currentRes = glm::vec4(renderQueue.ViewPortWidth, renderQueue.ViewPortHeight, 0.0f, 0.0f);
		for (uint32_t mipLevel = 0; mipLevel < m_Data->ScreenMipLevel; mipLevel++)
		{
			auto vulkan_BlurDescriptor = m_Data->BlurShaderDescriptor[currentFrameIndex][mipLevel].As<VulkanMaterial>();
			
			if (mipLevel > 0)
			{
				currentRes.x /= 2;
				currentRes.y /= 2;
			}
			currentRes.z = static_cast<float>(mipLevel);
			currentRes.w = mipLevel == 0 ? 0.0f : 1.0f;

			vulkan_BlurDescriptor->Bind(cmdBuf, m_Data->BlurPipeline);
			vulkan_BlurPipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &currentRes);

			uint32_t groupX = std::floor(currentRes.x / 32.0f) + 1;
			uint32_t groupY = std::floor(currentRes.y / 32.0f) + 1;
			vulkan_BlurPipeline->Dispatch(cmdBuf, groupX, groupY, 1);


			// Set a barrier
			VkImageSubresourceRange imageSubrange{};
			imageSubrange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageSubrange.baseArrayLayer = 0;
			imageSubrange.baseMipLevel = mipLevel;
			imageSubrange.layerCount = 1;
			imageSubrange.levelCount = 1;

			Utils::InsertImageMemoryBarrier(cmdBuf, vulkan_BlurColorTexture->GetVulkanImage(),
				VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
				vulkan_BlurColorTexture->GetVulkanImageLayout(), vulkan_BlurColorTexture->GetVulkanImageLayout(),
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				imageSubrange
			);


			
		}
	}

	void VulkanPostFXPass::SSR_Update(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		Ref<VulkanMaterial> ssrMaterial = m_Data->SSRDescriptor[currentFrameIndex].As<VulkanMaterial>();
		Ref<VulkanComputePipeline> ssrPipeline = m_Data->SSRPipeline.As<VulkanComputePipeline>();

		glm::mat4 projectionMatrix = renderQueue.CameraProjectionMatrix;
		projectionMatrix[1][1] *= -1;
		glm::mat4 invProjectionMatrix = glm::inverse(projectionMatrix);


		glm::mat4 viewMatrix = renderQueue.CameraViewMatrix;
		glm::mat4 invViewMatrix = glm::inverse(viewMatrix);



		ssrMaterial->Set("UniformBuffer.ProjectionMatrix", projectionMatrix);
		ssrMaterial->Set("UniformBuffer.InvProjectionMatrix", invProjectionMatrix);

		ssrMaterial->Set("UniformBuffer.ViewMatrix", viewMatrix);
		ssrMaterial->Set("UniformBuffer.InvViewMatrix", invViewMatrix);


		ssrMaterial->Bind(cmdBuf, m_Data->SSRPipeline);

		uint32_t groupX = (renderQueue.ViewPortWidth  / 32.0f) + 1;
		uint32_t groupY = (renderQueue.ViewPortHeight / 32.0f) + 1;
		ssrPipeline->Dispatch(cmdBuf, groupX, groupY, 1);
	}

	void VulkanPostFXPass::HZB_Update(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		// `i_Depth`
		auto renderPass = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->RenderPass;
		auto geometryDepthAttachment = renderPass->GetDepthAttachment(currentFrameIndex).As<VulkanImage2D>();


		VkImageLayout srcDepthImageLayout = geometryDepthAttachment->GetVulkanImageLayout();
		geometryDepthAttachment->TransitionLayout(cmdBuf, srcDepthImageLayout, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

		glm::vec2 currentHZB_Dimensions = { renderQueue.ViewPortWidth, renderQueue.ViewPortHeight };

		auto vulkanHZB_Pipeline = m_Data->HZBPipeline.As<VulkanComputePipeline>();

		for (uint32_t mipLevel = 0; mipLevel < m_Data->ScreenMipLevel; mipLevel++)
		{
			auto vulkanHZB_Descriptor = m_Data->HZBDescriptor[currentFrameIndex][mipLevel].As<VulkanMaterial>();
			auto vulkanDepthPyramid = m_Data->DepthPyramid[currentFrameIndex].As<VulkanImage2D>();

			if (mipLevel != 0)
				currentHZB_Dimensions /= glm::vec2(2.0f);


			vulkanHZB_Descriptor->Bind(cmdBuf, m_Data->HZBPipeline);

			vulkanHZB_Pipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &currentHZB_Dimensions);

			uint32_t groupX = std::floor(currentHZB_Dimensions.x / 32.0f) + 1;
			uint32_t groupY = std::floor(currentHZB_Dimensions.y / 32.0f) + 1;
			vulkanHZB_Pipeline->Dispatch(cmdBuf, groupX, groupY, 1);


			// Barrier after compute shaders
			VkImageSubresourceRange imageSubrange{};
			imageSubrange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageSubrange.baseArrayLayer = 0;
			imageSubrange.baseMipLevel = mipLevel;
			imageSubrange.layerCount = 1;
			imageSubrange.levelCount = 1;

			Utils::InsertImageMemoryBarrier(cmdBuf, vulkanDepthPyramid->GetVulkanImage(),
				VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
				vulkanDepthPyramid->GetVulkanImageLayout(), vulkanDepthPyramid->GetVulkanImageLayout(),
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				imageSubrange
			);

		}
	}

	void VulkanPostFXPass::Visibility_Update(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		glm::vec2 currentVisBuf_Dimensions = { renderQueue.ViewPortWidth, renderQueue.ViewPortHeight };

		auto vulkanVis_Pipeline = m_Data->VisibilityPipeline.As<VulkanComputePipeline>();

		struct VisBuffer_PushConstant
		{
			uint32_t CurrentMipLevel;
			float NearPlane;
			float FarPlane;
		} visBufferPushConstant;

		visBufferPushConstant.NearPlane = renderQueue.m_Camera.GetNearClip();
		visBufferPushConstant.FarPlane = renderQueue.m_Camera.GetFarClip();

		for (uint32_t mipLevel = 0; mipLevel < m_Data->ScreenMipLevel; mipLevel++)
		{
			auto vulkanVis_Descriptor = m_Data->VisibilityDescriptor[currentFrameIndex][mipLevel].As<VulkanMaterial>();
			auto vulkanVis_Image = m_Data->VisibilityImage[currentFrameIndex].As<VulkanImage2D>();

			if (mipLevel != 0)
				currentVisBuf_Dimensions /= glm::vec2(2.0f);

			vulkanVis_Descriptor->Bind(cmdBuf, m_Data->VisibilityPipeline);

			visBufferPushConstant.CurrentMipLevel = mipLevel;
			vulkanVis_Pipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &visBufferPushConstant);

			uint32_t groupX = std::floor(currentVisBuf_Dimensions.x / 32.0f) + 1;
			uint32_t groupY = std::floor(currentVisBuf_Dimensions.y / 32.0f) + 1;
			vulkanVis_Pipeline->Dispatch(cmdBuf, groupX, groupY, 1);


			// Barrier after compute shaders
			VkImageSubresourceRange imageSubrange{};
			imageSubrange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageSubrange.baseArrayLayer = 0;
			imageSubrange.baseMipLevel = mipLevel;
			imageSubrange.layerCount = 1;
			imageSubrange.levelCount = 1;

			Utils::InsertImageMemoryBarrier(cmdBuf, vulkanVis_Image->GetVulkanImage(),
				VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
				vulkanVis_Image->GetVulkanImageLayout(), vulkanVis_Image->GetVulkanImageLayout(),
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				imageSubrange
			);
		}
	}

	struct AO_PushConstant
	{
		glm::mat4 ViewMatrix;
		glm::mat4 InvProjMatrix;

		// vec4:                                       x        ||     y,z   
		glm::vec3 AO_Data = glm::vec3(0.0f); // ProjectionScale || ScreenSize
		int32_t AO_Mode = 1; // 0 = HBAO || 1 = GTAO

	} static s_AO_pushConstant;

	void VulkanPostFXPass::AO_Update(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		auto vulkan_AO_Pipeline = m_Data->AO_Pipeline.As<VulkanComputePipeline>();
		auto vulkan_AO_Descriptor = m_Data->AO_Descriptor[currentFrameIndex].As<VulkanMaterial>();

		glm::mat4 projMatrix = renderQueue.m_Camera.GetProjectionMatrix();
		projMatrix[1][1] *= -1;

		float width = renderQueue.ViewPortWidth;
		float height = renderQueue.ViewPortHeight;
		float fov = glm::radians(renderQueue.m_Camera.GetCameraFOV());
		float farplane = renderQueue.m_Camera.GetFarClip();
		float nearplane = renderQueue.m_Camera.GetNearClip();


		float projectionScale = (height / (2.0f * tanf(fov * 0.5f)));
		s_AO_pushConstant.AO_Mode = m_AOSettings.AOMode;
		s_AO_pushConstant.AO_Data = { projectionScale, width, height };
		s_AO_pushConstant.ViewMatrix = renderQueue.CameraViewMatrix;
		s_AO_pushConstant.InvProjMatrix = glm::inverse(projMatrix);

		vulkan_AO_Descriptor->Bind(cmdBuf, m_Data->AO_Pipeline);

		vulkan_AO_Pipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &s_AO_pushConstant);

		uint32_t groupX = static_cast<uint32_t>(std::floor(width / 32.0f)) + 1;
		uint32_t groupY = static_cast<uint32_t>(std::floor(height / 32.0f)) + 1;
		vulkan_AO_Pipeline->Dispatch(cmdBuf, groupX, groupY, 1);


		Ref<VulkanImage2D> vulkanAOTexture = m_Data->AO_Image[currentFrameIndex].As<VulkanImage2D>();

		VkImageSubresourceRange imageSubrange{};
		imageSubrange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageSubrange.baseArrayLayer = 0;
		imageSubrange.baseMipLevel = 0;
		imageSubrange.layerCount = 1;
		imageSubrange.levelCount = 1;

		Utils::InsertImageMemoryBarrier(cmdBuf, vulkanAOTexture->GetVulkanImage(),
			VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
			vulkanAOTexture->GetVulkanImageLayout(), vulkanAOTexture->GetVulkanImageLayout(),
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			imageSubrange
		);
	}

	void VulkanPostFXPass::SpatialDenoiser_Update(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		auto vulkan_denoiser_Pipeline = m_Data->DenoiserPipeline.As<VulkanComputePipeline>();
		auto vulkan_denoiser_Descriptor = m_Data->DenoiserDescriptor[currentFrameIndex].As<VulkanMaterial>();

		vulkan_denoiser_Descriptor->Bind(cmdBuf, m_Data->DenoiserPipeline);

		float width = renderQueue.ViewPortWidth;
		float height = renderQueue.ViewPortHeight;
		
		uint32_t groupX = std::floor(width / 32.0f) + 1;
		uint32_t groupY = std::floor(height / 32.0f) + 1;
		vulkan_denoiser_Pipeline->Dispatch(cmdBuf, groupX, groupY, 1);



		Ref<VulkanImage2D> vulkanDenoisedAOTexture = m_Data->DenoiserImage[currentFrameIndex].As<VulkanImage2D>();

		VkImageSubresourceRange imageSubrange{};
		imageSubrange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageSubrange.baseArrayLayer = 0;
		imageSubrange.baseMipLevel = 0;
		imageSubrange.layerCount = 1;
		imageSubrange.levelCount = 1;

		Utils::InsertImageMemoryBarrier(cmdBuf, vulkanDenoisedAOTexture->GetVulkanImage(),
			VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
			vulkanDenoisedAOTexture->GetVulkanImageLayout(), vulkanDenoisedAOTexture->GetVulkanImageLayout(),
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			imageSubrange
		);
	}

	void VulkanPostFXPass::ColorCorrection_Update(const RenderQueue& renderQueue, uint32_t target)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		// cc = color correction
		auto vulkan_cc_Pipeline = m_Data->ColorCorrectionPipeline.As<VulkanComputePipeline>();
		auto vulkan_cc_Descriptor = m_Data->ColorCorrectionDescriptor[currentFrameIndex].As<VulkanMaterial>();

		vulkan_cc_Descriptor->Bind(cmdBuf, m_Data->ColorCorrectionPipeline);

		glm::vec3 pushConstant = { 2.2f, renderQueue.m_Camera.GetExposure(), static_cast<float>(target) };
		vulkan_cc_Pipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &pushConstant);

		float width = renderQueue.ViewPortWidth;
		float height = renderQueue.ViewPortHeight;

		uint32_t groupX = std::floor(width / 32.0f) + 1;
		uint32_t groupY = std::floor(height / 32.0f) + 1;
		vulkan_cc_Pipeline->Dispatch(cmdBuf, groupX, groupY, 1);
	}

	void VulkanPostFXPass::ApplyAerial_Update(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		auto vulkanApplyAerialPipeline = m_Data->ApplyAerialPipeline.As<VulkanComputePipeline>();
		auto vulkanApplyAerialDescriptor = m_Data->ApplyAerialDescriptor[currentFrameIndex].As<VulkanMaterial>();

		vulkanApplyAerialDescriptor->Bind(cmdBuf, m_Data->ApplyAerialPipeline);

		vulkanApplyAerialDescriptor->Set("CameraBlock.ViewMatrix", renderQueue.CameraViewMatrix);
		vulkanApplyAerialDescriptor->Set("CameraBlock.ProjMatrix", renderQueue.CameraProjectionMatrix);
		vulkanApplyAerialDescriptor->Set("CameraBlock.InvViewProjMatrix", glm::inverse(renderQueue.m_Camera.GetViewProjection()));
		vulkanApplyAerialDescriptor->Set("CameraBlock.CamPosition", glm::vec4(renderQueue.CameraPosition, 0.0f));
		vulkanApplyAerialDescriptor->Set("CameraBlock.NearFarPlane", glm::vec4(renderQueue.m_Camera.GetNearClip(), renderQueue.m_Camera.GetFarClip(), 0.0f, 0.0f));

		float width = renderQueue.ViewPortWidth;
		float height = renderQueue.ViewPortHeight;

		uint32_t groupX = std::ceil(width / 32.0f);
		uint32_t groupY = std::ceil(height / 32.0f);
		vulkanApplyAerialPipeline->Dispatch(cmdBuf, groupX, groupY, 1);
	}

	void VulkanPostFXPass::Bloom_Update(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		auto vulkanColorBuffer = m_RenderPassPipeline->GetRenderPassData<VulkanCompositePass>()->RenderPass->GetColorAttachment(0, currentFrameIndex).As<VulkanImage2D>();

		float workGroupSize = 16.0f;

		Ref<VulkanImage2D> vulkanDownscaleBloomTex = m_Data->Bloom_DownsampledTexture[currentFrameIndex].As<VulkanImage2D>();
		Ref<VulkanImage2D> vulkanUpscaleBloomTex = m_Data->Bloom_UpsampledTexture[currentFrameIndex].As<VulkanImage2D>();

		Ref<VulkanMaterial> vulkanBloomDescriptor = m_Data->BloomDescriptor[currentFrameIndex].As<VulkanMaterial>();
		Ref<VulkanComputePipeline> vulkanBloomPipeline = m_Data->BloomPipeline.As<VulkanComputePipeline>();

		VkDescriptorSetLayout descriptorSetLayout = vulkanBloomDescriptor->GetVulkanDescriptorLayout()[0];
		
		struct BloomComputePushConstants
		{
			glm::vec4 Params; // threshold, 
			float LOD = 0.0f;
			int Mode = 0; // 0 = prefilter, 1 = downsample, 2 = firstUpsample, 3 = upsample
		} bloomPushConstant;

#if 0
		
		bloomComputePushConstants.Params = { m_BloomSettings.Threshold, m_BloomSettings.Threshold - m_BloomSettings.Knee, m_BloomSettings.Knee * 2.0f, 0.25f / m_BloomSettings.Knee };
#endif



		bloomPushConstant.Params = { m_BloomSettings.Threshold, m_BloomSettings.Threshold - m_BloomSettings.Knee, m_BloomSettings.Knee * 2.0f, 0.25f / m_BloomSettings.Knee };

		
		/// Part 1: Prefilter and pick up the color values over the threeshold
		{
			//VkDescriptorSet descriptorSet = vulkanBloomDescriptor->GetVulkanDescriptorSet(0);
			VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = &descriptorSetLayout;
			VkDescriptorSet descriptorSet = VulkanRenderer::AllocateDescriptorSet(allocInfo);

			// Input texture
			{
				// Get the prefiltered (blurred) color buffer
				// I could've used the normal buffer, from the renderpass, but I'm lazy to write more code
				auto inputImageInfo = vulkanColorBuffer->GetVulkanDescriptorInfo(DescriptorImageType::Sampled);

				VkWriteDescriptorSet wds{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				wds.dstBinding = 1;
				wds.dstArrayElement = 0;
				wds.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				wds.pImageInfo = &inputImageInfo;
				wds.descriptorCount = 1;
				wds.dstSet = descriptorSet;

				vkUpdateDescriptorSets(device, 1, &wds, 0, nullptr);

				// This is for the 'u_BloomTexture' which we dont care at the moment
				wds.dstBinding = 2;
				vkUpdateDescriptorSets(device, 1, &wds, 0, nullptr);

			}

			// Output texture
			{
				auto outputImageInfo = vulkanDownscaleBloomTex->GetVulkanDescriptorInfo(DescriptorImageType::Storage);

				VkWriteDescriptorSet wds{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				wds.dstBinding = 0;
				wds.dstArrayElement = 0;
				wds.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				wds.pImageInfo = &outputImageInfo;
				wds.descriptorCount = 1;
				wds.dstSet = descriptorSet;

				vkUpdateDescriptorSets(device, 1, &wds, 0, nullptr);
			}

			//vulkanBloomDescriptor->Bind(cmdBuf, m_Data->BloomPipeline);
			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, vulkanBloomPipeline->GetVulkanPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);

			vulkanBloomPipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &bloomPushConstant);

			uint32_t width = renderQueue.ViewPortWidth;
			uint32_t height = renderQueue.ViewPortHeight;

			uint32_t workGroupsX = std::floor(width / workGroupSize) + 1;
			uint32_t workGroupsY = std::floor(height / workGroupSize) + 1;
			vulkanBloomPipeline->Dispatch(cmdBuf, workGroupsX, workGroupsY, 1);

			VkImageSubresourceRange imageSubrange{};
			imageSubrange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageSubrange.baseArrayLayer = 0;
			imageSubrange.baseMipLevel = 0;
			imageSubrange.layerCount = 1;
			imageSubrange.levelCount = 1;

			// Set a barrier for the prefiltered thresholded texture
			Utils::InsertImageMemoryBarrier(cmdBuf, vulkanDownscaleBloomTex->GetVulkanImage(),
				VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
				vulkanDownscaleBloomTex->GetVulkanImageLayout(), vulkanDownscaleBloomTex->GetVulkanImageLayout(),
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				imageSubrange
			);
		}

		/// Part 2: Downsample the prefiltered thresholded texture
		{
			uint32_t totalMips = m_Data->ScreenMipLevel - 2;
			for (uint32_t mip = 1; mip < totalMips; mip++)
			{
				auto [mipWidth, mipHeight] = vulkanDownscaleBloomTex->GetMipSize(mip);

				VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
				allocInfo.descriptorSetCount = 1;
				allocInfo.pSetLayouts = &descriptorSetLayout;
				VkDescriptorSet descriptorSet = VulkanRenderer::AllocateDescriptorSet(allocInfo);

				
				// Input texture
				{
					VkDescriptorImageInfo inputImageInfo{};
					inputImageInfo.imageLayout = vulkanDownscaleBloomTex->GetVulkanImageLayout();
					inputImageInfo.imageView = vulkanDownscaleBloomTex->GetVulkanImageView();
					inputImageInfo.sampler = vulkanDownscaleBloomTex->GetVulkanSampler();

					VkWriteDescriptorSet wds{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
					wds.dstBinding = 1;
					wds.dstArrayElement = 0;
					wds.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
					wds.pImageInfo = &inputImageInfo;
					wds.descriptorCount = 1;
					wds.dstSet = descriptorSet;

					vkUpdateDescriptorSets(device, 1, &wds, 0, nullptr);

					// This is for the 'u_BloomTexture' which we dont care at the moment
					wds.dstBinding = 2;
					vkUpdateDescriptorSets(device, 1, &wds, 0, nullptr);
				}

				// Output texture
				{
					VkDescriptorImageInfo outputImageInfo{};
					outputImageInfo.imageLayout = vulkanDownscaleBloomTex->GetVulkanImageLayout();
					outputImageInfo.imageView = vulkanDownscaleBloomTex->GetVulkanImageViewMip(mip);
					outputImageInfo.sampler = vulkanDownscaleBloomTex->GetVulkanSampler();

					VkWriteDescriptorSet wds{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
					wds.dstBinding = 0;
					wds.dstArrayElement = 0;
					wds.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
					wds.pImageInfo = &outputImageInfo;
					wds.descriptorCount = 1;
					wds.dstSet = descriptorSet;

					vkUpdateDescriptorSets(device, 1, &wds, 0, nullptr);
				}

				bloomPushConstant.LOD = mip - 1.0f; // We sample the previous mip (for downsampling)
				bloomPushConstant.Mode = 1; // DOWNSAMPLE

				vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, vulkanBloomPipeline->GetVulkanPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);

				uint32_t workGroupsX = (uint32_t)std::floor((float)mipWidth / workGroupSize) + 1;
				uint32_t workGroupsY = (uint32_t)std::floor((float)mipHeight / workGroupSize) + 1;


				vulkanBloomPipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &bloomPushConstant);
				vulkanBloomPipeline->Dispatch(cmdBuf, workGroupsX, workGroupsY, 1);


				VkImageSubresourceRange imageSubrange{};
				imageSubrange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				imageSubrange.baseArrayLayer = 0;
				imageSubrange.baseMipLevel = mip;
				imageSubrange.layerCount = 1;
				imageSubrange.levelCount = 1;

				
				// Set a barrier for the prefiltered thresholded texture
				Utils::InsertImageMemoryBarrier(cmdBuf, vulkanDownscaleBloomTex->GetVulkanImage(),
					VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
					vulkanDownscaleBloomTex->GetVulkanImageLayout(), vulkanDownscaleBloomTex->GetVulkanImageLayout(),
					VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
					imageSubrange
				);

			}
		}

		/// Part 3: Upsample the last mip
		{
			VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = &descriptorSetLayout;
			VkDescriptorSet descriptorSet = VulkanRenderer::AllocateDescriptorSet(allocInfo);

			// Previous part we were using "mipLevel - 2" (11-2=9), but the for loop was "<" not "<=",
			// which means that it reaches the maximum of "mipLevel - 3" (11-3=8).
			// Now we want to upsample so we are using for the output the "mipLevel - 4" (11-4=7) and
			// as input we are using the "mip+1" which is "(mipLevel - 4) + 1" (11-4=7, 7+1=8)
			uint32_t mip = m_Data->ScreenMipLevel - 4;

			// Input texture
			{
				VkDescriptorImageInfo inputImageInfo{};
				inputImageInfo.imageLayout = vulkanDownscaleBloomTex->GetVulkanImageLayout();
				inputImageInfo.imageView = vulkanDownscaleBloomTex->GetVulkanImageView();
				inputImageInfo.sampler = vulkanDownscaleBloomTex->GetVulkanSampler();
				
				VkWriteDescriptorSet wds{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				wds.dstBinding = 1;
				wds.dstArrayElement = 0;
				wds.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				wds.pImageInfo = &inputImageInfo;
				wds.descriptorCount = 1;
				wds.dstSet = descriptorSet;

				vkUpdateDescriptorSets(device, 1, &wds, 0, nullptr);

				// This is for the 'u_BloomTexture' which we dont care at the moment
				wds.dstBinding = 2;
				vkUpdateDescriptorSets(device, 1, &wds, 0, nullptr);

			}

			// Output texture
			{
				VkDescriptorImageInfo outputImageInfo{};
				outputImageInfo.imageLayout = vulkanUpscaleBloomTex->GetVulkanImageLayout();
				outputImageInfo.imageView = vulkanUpscaleBloomTex->GetVulkanImageViewMip(mip);
				outputImageInfo.sampler = vulkanUpscaleBloomTex->GetVulkanSampler();

				VkWriteDescriptorSet wds{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				wds.dstBinding = 0;
				wds.dstArrayElement = 0;
				wds.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				wds.pImageInfo = &outputImageInfo;
				wds.descriptorCount = 1;
				wds.dstSet = descriptorSet;

				vkUpdateDescriptorSets(device, 1, &wds, 0, nullptr);
			}


			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, vulkanBloomPipeline->GetVulkanPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);

			bloomPushConstant.Mode = 2; // UPSAMPLE_FIRST
			bloomPushConstant.LOD = mip + 1;
			vulkanBloomPipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &bloomPushConstant);

			auto [mipWidth, mipHeight] = vulkanUpscaleBloomTex->GetMipSize(mip);

			uint32_t workGroupsX = (uint32_t)std::floor((float)mipWidth / workGroupSize) + 1;
			uint32_t workGroupsY = (uint32_t)std::floor((float)mipHeight / workGroupSize) + 1;
			vulkanBloomPipeline->Dispatch(cmdBuf, workGroupsX, workGroupsY, 1);

			VkImageSubresourceRange imageSubrange{};
			imageSubrange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageSubrange.baseArrayLayer = 0;
			imageSubrange.baseMipLevel = mip;
			imageSubrange.layerCount = 1;
			imageSubrange.levelCount = 1;

			// Set a barrier for the prefiltered thresholded texture
			Utils::InsertImageMemoryBarrier(cmdBuf, vulkanUpscaleBloomTex->GetVulkanImage(),
				VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
				vulkanUpscaleBloomTex->GetVulkanImageLayout(), vulkanUpscaleBloomTex->GetVulkanImageLayout(),
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				imageSubrange
			);
		}

		/// Part 4: Usample and combine
		{
			uint32_t totalMips = m_Data->ScreenMipLevel;
			for (uint32_t mip = totalMips - 4; mip >= 1; mip--)
			{
				auto [mipWidth, mipHeight] = vulkanUpscaleBloomTex->GetMipSize(mip - 1);

				VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
				allocInfo.descriptorSetCount = 1;
				allocInfo.pSetLayouts = &descriptorSetLayout;
				VkDescriptorSet descriptorSet = VulkanRenderer::AllocateDescriptorSet(allocInfo);


				// Input texture
				{
					{
						// 'u_Texture' - the WHOLE downsampled texture
						VkDescriptorImageInfo inputImageInfo{};
						inputImageInfo.imageLayout = vulkanDownscaleBloomTex->GetVulkanImageLayout();
						inputImageInfo.imageView = vulkanDownscaleBloomTex->GetVulkanImageView();
						inputImageInfo.sampler = vulkanDownscaleBloomTex->GetVulkanSampler();

						VkWriteDescriptorSet wds{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
						wds.dstBinding = 1;
						wds.dstArrayElement = 0;
						wds.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
						wds.pImageInfo = &inputImageInfo;
						wds.descriptorCount = 1;
						wds.dstSet = descriptorSet;

						vkUpdateDescriptorSets(device, 1, &wds, 0, nullptr);
					}

					{
						// 'u_BloomTexture' - the previous mip of the upsampled texture
						VkDescriptorImageInfo inputImageInfo{};
						inputImageInfo.imageLayout = vulkanUpscaleBloomTex->GetVulkanImageLayout();
						inputImageInfo.imageView = vulkanUpscaleBloomTex->GetVulkanImageView();
						inputImageInfo.sampler = vulkanUpscaleBloomTex->GetVulkanSampler();

						VkWriteDescriptorSet wds{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
						wds.dstBinding = 2;
						wds.dstArrayElement = 0;
						wds.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
						wds.pImageInfo = &inputImageInfo;
						wds.descriptorCount = 1;
						wds.dstSet = descriptorSet;

						vkUpdateDescriptorSets(device, 1, &wds, 0, nullptr);
					}
				}

				// Output texture
				{
					VkDescriptorImageInfo outputImageInfo{};
					outputImageInfo.imageLayout = vulkanUpscaleBloomTex->GetVulkanImageLayout();
					outputImageInfo.imageView = vulkanUpscaleBloomTex->GetVulkanImageViewMip(mip - 1);
					outputImageInfo.sampler = vulkanUpscaleBloomTex->GetVulkanSampler();

					VkWriteDescriptorSet wds{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
					wds.dstBinding = 0;
					wds.dstArrayElement = 0;
					wds.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
					wds.pImageInfo = &outputImageInfo;
					wds.descriptorCount = 1;
					wds.dstSet = descriptorSet;

					vkUpdateDescriptorSets(device, 1, &wds, 0, nullptr);
				}

				bloomPushConstant.LOD = mip - 1; // We sample the next mip (descending order) (for upsampling)
				bloomPushConstant.Mode = 3; // UPSAMPLE

				vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, vulkanBloomPipeline->GetVulkanPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);

				uint32_t workGroupsX = (uint32_t)std::floor((float)mipWidth / workGroupSize) + 1;
				uint32_t workGroupsY = (uint32_t)std::floor((float)mipHeight / workGroupSize) + 1;


				vulkanBloomPipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &bloomPushConstant);
				vulkanBloomPipeline->Dispatch(cmdBuf, workGroupsX, workGroupsY, 1);


				VkImageSubresourceRange imageSubrange{};
				imageSubrange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				imageSubrange.baseArrayLayer = 0;
				imageSubrange.baseMipLevel = mip - 1;
				imageSubrange.layerCount = 1;
				imageSubrange.levelCount = 1;


				// Set a barrier for the prefiltered thresholded texture
				Utils::InsertImageMemoryBarrier(cmdBuf, vulkanDownscaleBloomTex->GetVulkanImage(),
					VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
					vulkanDownscaleBloomTex->GetVulkanImageLayout(), vulkanDownscaleBloomTex->GetVulkanImageLayout(),
					VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
					imageSubrange
				);

			}
		}
	}

	void VulkanPostFXPass::OnResize(uint32_t width, uint32_t height)
	{
		CalculateMipLevels       (width, height);
		Bloom_InitData           (width, height);
		ApplyAerial_InitData     (width, height);
		ColorCorrection_InitData (width, height);
		BlurColorBuffer_InitData (width, height);
		HZB_InitData             (width, height);
		Visibility_InitData      (width, height);
		AO_InitData              (width, height);
		SpatialDenoiser_InitData (width, height);
		SSR_InitData             (width, height);
	}

	void VulkanPostFXPass::OnResizeLate(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;

		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			auto vulkanColorCorrectionDescriptor = m_Data->ColorCorrectionDescriptor[i].As<VulkanMaterial>();

			vulkanColorCorrectionDescriptor->Set("u_SSRTexture", m_Data->SSRTexture[i]);
			vulkanColorCorrectionDescriptor->Set("u_AOTexture", m_Data->DenoiserImage[i]);
			vulkanColorCorrectionDescriptor->UpdateVulkanDescriptorIfNeeded();
		}
	}

	void VulkanPostFXPass::CalculateMipLevels(uint32_t width, uint32_t height)
	{
		m_Data->ScreenMipLevel = (uint32_t)std::floor(std::log2(std::max(width, height))) + 1;
	}

	void VulkanPostFXPass::ShutDown()
	{
		delete m_Data;
	}
}