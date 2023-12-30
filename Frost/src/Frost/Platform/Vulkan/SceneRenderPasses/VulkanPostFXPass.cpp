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
#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanVolumetricPass.h"

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

		m_Data->SSRShader = Renderer::GetShaderLibrary()->Get("SSR");
		m_Data->BlurShader = Renderer::GetShaderLibrary()->Get("GaussianBlur");
		m_Data->HZBShader = Renderer::GetShaderLibrary()->Get("HiZBufferBuilder");
		m_Data->VisibilityShader = Renderer::GetShaderLibrary()->Get("VisibilityBuffer");
		//m_Data->AO_Shader = Renderer::GetShaderLibrary()->Get("AmbientOcclusion");
		m_Data->AO_Shader = Renderer::GetShaderLibrary()->Get("AO");
		m_Data->DenoiserShader = Renderer::GetShaderLibrary()->Get("AmbientOcclusionDenoiser");
		m_Data->AmbientOcclusionTAAShader = Renderer::GetShaderLibrary()->Get("AmbientOcclusionTAA");
		m_Data->BloomShader = Renderer::GetShaderLibrary()->Get("Bloom");
		m_Data->ApplyAerialShader = Renderer::GetShaderLibrary()->Get("ApplyAerial");
		m_Data->ColorCorrectionShader = Renderer::GetShaderLibrary()->Get("ColorCorrection");
		m_Data->BloomConvolutionShader = Renderer::GetShaderLibrary()->Get("BloomConvolution");
		m_Data->FXAAShader = Renderer::GetShaderLibrary()->Get("FXAA");
		m_Data->TAAShader = Renderer::GetShaderLibrary()->Get("TAA");
		m_Data->BloomConvolutionFilterShader = Renderer::GetShaderLibrary()->Get("BloomConvolutionFilter");

		CalculateMipLevels(1600, 900);
		BloomInitData(1600, 900);
		//BloomConvolutionInitData (1600, 900);
		//BloomConvolutionFilterInitData (1600, 900);
		//ApplyAerialInitData      (1600, 900);
		ColorCorrectionInitData(1600, 900);
		HZBInitData(1600, 900);
		SSRFilterInitData(1600, 900);
		//VisibilityInitData     (1600, 900);
		AmbientOcclusionInitData(1600 / 2.0, 900 / 2.0);
		SpatialDenoiserInitData(1600, 900);
		AmbientOcclusionTAAInitData(1600, 900);
		SSRInitData(1600 / 1.0, 900 / 1.0);
		TAAInitData(1600, 900);
		FXAAInitData(1600, 900);

		Renderer::SubmitImageToOutputImageMap("FinalImage", [this]() -> Ref<Image2D>
		{
			uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
			return this->m_Data->FinalTexture[currentFrameIndex];
			//return this->m_Data->FXAAImage[currentFrameIndex];
		});
	}

	void VulkanPostFXPass::InitLate()
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;

		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			auto vulkanColorCorrectionDescriptor = m_Data->ColorCorrectionDescriptor[i].As<VulkanMaterial>();

			vulkanColorCorrectionDescriptor->Set("u_SSRTexture", m_Data->SSRTexture[i]);
			vulkanColorCorrectionDescriptor->Set("u_AOTexture", m_Data->AmbientOcclusionTAAImage[i]);
			vulkanColorCorrectionDescriptor->UpdateVulkanDescriptorIfNeeded();
		}
	}

	void VulkanPostFXPass::SSRFilterInitData(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		uint32_t mipLevels = m_Data->ScreenMipLevel;

		{
			ComputePipeline::CreateInfo computePipelineCreateInfo{};
			computePipelineCreateInfo.Shader = m_Data->BlurShader;

			if (!m_Data->BlurPipeline)
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

				if (!descriptor)
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

	void VulkanPostFXPass::SSRInitData(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		{
			ComputePipeline::CreateInfo computePipelineCreateInfo{};
			computePipelineCreateInfo.Shader = m_Data->SSRShader;

			if (!m_Data->SSRPipeline)
				m_Data->SSRPipeline = ComputePipeline::Create(computePipelineCreateInfo);
		}

		m_Data->SSRTexture.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			ImageSpecification imageSpec{};
			imageSpec.Width = uint32_t(width);
			imageSpec.Height = uint32_t(height);
			imageSpec.Format = ImageFormat::RGBA8;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.Sampler.SamplerFilter = ImageFilter::Nearest;
			m_Data->SSRTexture[i] = Image2D::Create(imageSpec);
		}



		m_Data->SSRDescriptor.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			if (!m_Data->SSRDescriptor[i])
				m_Data->SSRDescriptor[i] = Material::Create(m_Data->SSRShader, "Post-ProceesingFX");

			auto vulkanMaterial = m_Data->SSRDescriptor[i].As<VulkanMaterial>();
			auto vulkanDepthPyramid = m_Data->DepthPyramid[i].As<VulkanImage2D>();
			VkDescriptorSet descriptorSet = vulkanMaterial->GetVulkanDescriptorSet(0);

			auto viewPosTexture = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->GeometryRenderPass->GetColorAttachment(2, i);
			auto normalTexture = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->GeometryRenderPass->GetColorAttachment(0, i);

			// Textures
			vulkanMaterial->Set("u_ColorFrameTex", m_Data->ColorCorrectionTexture[i]);
			vulkanMaterial->Set("u_ViewPosTex", viewPosTexture);
			//vulkanMaterial->Set("u_HiZBuffer", m_Data->DepthPyramid[i]);
			vulkanMaterial->Set("u_NormalTex", normalTexture);
			vulkanMaterial->Set("o_FrameTex", m_Data->SSRTexture[i]);
			vulkanMaterial->Set("u_PrefilteredColorBuffer", m_Data->BlurredColorBuffer[i]);
			vulkanMaterial->Set("u_SpatialBlueNoiseLUT", Renderer::GetSpatialBlueNoiseLut());
			vulkanMaterial->Set("u_HiZBuffer", m_Data->DepthPyramid[i]);

#if 0
			// HZB with custum linear sampler (exclusive for SSR)
			VkDescriptorImageInfo imageDescriptorInfo{};
			imageDescriptorInfo.imageView = vulkanDepthPyramid->GetVulkanImageView();
			imageDescriptorInfo.imageLayout = vulkanDepthPyramid->GetVulkanImageLayout();
			imageDescriptorInfo.sampler = m_Data->DepthPyramid[i];

			VkWriteDescriptorSet writeDescriptorSet{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			writeDescriptorSet.dstBinding = 2; // layout(binding = 2) uniform sampler2D u_HiZBuffer;
			writeDescriptorSet.dstArrayElement = 0;
			writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writeDescriptorSet.pImageInfo = &imageDescriptorInfo;
			writeDescriptorSet.descriptorCount = 1;
			writeDescriptorSet.dstSet = descriptorSet;

			vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, 0);
#endif

			// Uniform buffer data
			//vulkanMaterial->Set("UniformBuffer.ScreenSize", glm::vec4(width + (width % 2), height + (height % 2), 0.0f, 0.0f));
			width = int32_t(width * 0.75f) + (int32_t(width * 0.75f) % 2);
			height = int32_t(height * 0.75f) + (int32_t(height * 0.75f) % 2);
			vulkanMaterial->Set("UniformBuffer.ScreenSize", glm::vec4(width, height, 0.0f, 0.0f));

			vulkanMaterial->UpdateVulkanDescriptorIfNeeded();
		}
	}

	void VulkanPostFXPass::HZBInitData(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		VkPhysicalDevice physicalDevice = VulkanContext::GetCurrentDevice()->GetPhysicalDevice();
		uint32_t mipLevels = m_Data->ScreenMipLevel;

		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(physicalDevice, &properties);

		{
			// Pipeline creation
			ComputePipeline::CreateInfo computePipelineCreateInfo{};
			computePipelineCreateInfo.Shader = m_Data->HZBShader;
			if (!m_Data->HZBPipeline)
				m_Data->HZBPipeline = ComputePipeline::Create(computePipelineCreateInfo);
		}

		//if (width % 2 != 0)
		//	width = width - 1;
		//if (height % 2 == 0)
		//	height = height - 1;

		// Calculating the previous frame index
		m_Data->DepthPyramid.resize(framesInFlight);
		m_Data->HZBNearestSampler.resize(framesInFlight);
		m_Data->HZBMinReductionSampler.resize(framesInFlight);
		m_Data->HZBMaxReductionSampler.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{

			ImageSpecification imageSpec{};
			imageSpec.Width = width;
			imageSpec.Height = height;
			//imageSpec.Sampler.ReductionMode_Optional = ReductionMode::Max; // TODO: Temporary
			//imageSpec.Sampler.ReductionMode_Optional = ReductionMode::Min;
			imageSpec.Sampler.SamplerFilter = ImageFilter::Linear;
			imageSpec.Sampler.SamplerWrap = ImageWrap::ClampToEdge;

			imageSpec.Format = ImageFormat::RG32F;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.UseMipChain = true;

			m_Data->DepthPyramid[i] = Image2D::Create(imageSpec);


			if (!m_Data->HZBNearestSampler[i])
			{
				VkSamplerCreateInfo nearestSamplerCreateInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
				nearestSamplerCreateInfo.magFilter = VK_FILTER_NEAREST;
				nearestSamplerCreateInfo.minFilter = VK_FILTER_NEAREST;
				nearestSamplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
				nearestSamplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
				nearestSamplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
				nearestSamplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
				nearestSamplerCreateInfo.anisotropyEnable = VK_TRUE;
				nearestSamplerCreateInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
				nearestSamplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
				nearestSamplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
				nearestSamplerCreateInfo.compareEnable = VK_FALSE;
				nearestSamplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
				nearestSamplerCreateInfo.mipLodBias = 0.0f;
				nearestSamplerCreateInfo.minLod = 0.0f;
				nearestSamplerCreateInfo.maxLod = static_cast<float>(mipLevels);
				vkCreateSampler(device, &nearestSamplerCreateInfo, nullptr, &m_Data->HZBNearestSampler[i]);
			}

			if (!m_Data->HZBMinReductionSampler[i])
			{
				VkSamplerCreateInfo minReductionSamplerCreateInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
				minReductionSamplerCreateInfo.magFilter = VK_FILTER_LINEAR;
				minReductionSamplerCreateInfo.minFilter = VK_FILTER_LINEAR;
				minReductionSamplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
				minReductionSamplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
				minReductionSamplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
				minReductionSamplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
				minReductionSamplerCreateInfo.anisotropyEnable = VK_TRUE;
				minReductionSamplerCreateInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
				minReductionSamplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
				minReductionSamplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
				minReductionSamplerCreateInfo.compareEnable = VK_FALSE;
				minReductionSamplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
				minReductionSamplerCreateInfo.mipLodBias = 0.0f;
				minReductionSamplerCreateInfo.minLod = 0.0f;
				minReductionSamplerCreateInfo.maxLod = static_cast<float>(mipLevels);

				// Using Min Mode Reduction
				VkSamplerReductionModeCreateInfoEXT createInfoReduction{ VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT };
				createInfoReduction.reductionMode = VK_SAMPLER_REDUCTION_MODE_MIN_EXT;
				minReductionSamplerCreateInfo.pNext = &createInfoReduction;

				vkCreateSampler(device, &minReductionSamplerCreateInfo, nullptr, &m_Data->HZBMinReductionSampler[i]);
			}

			if (!m_Data->HZBMaxReductionSampler[i])
			{
				VkSamplerCreateInfo maxReductionSamplerCreateInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
				maxReductionSamplerCreateInfo.magFilter = VK_FILTER_LINEAR;
				maxReductionSamplerCreateInfo.minFilter = VK_FILTER_LINEAR;
				maxReductionSamplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
				maxReductionSamplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
				maxReductionSamplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
				maxReductionSamplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
				maxReductionSamplerCreateInfo.anisotropyEnable = VK_TRUE;
				maxReductionSamplerCreateInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
				maxReductionSamplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
				maxReductionSamplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
				maxReductionSamplerCreateInfo.compareEnable = VK_FALSE;
				maxReductionSamplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
				maxReductionSamplerCreateInfo.mipLodBias = 0.0f;
				maxReductionSamplerCreateInfo.minLod = 0.0f;
				maxReductionSamplerCreateInfo.maxLod = static_cast<float>(mipLevels);

				// Using Max Mode Reduction
				VkSamplerReductionModeCreateInfoEXT createInfoReduction{ VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT };
				createInfoReduction.reductionMode = VK_SAMPLER_REDUCTION_MODE_MAX_EXT;
				maxReductionSamplerCreateInfo.pNext = &createInfoReduction;

				vkCreateSampler(device, &maxReductionSamplerCreateInfo, nullptr, &m_Data->HZBMaxReductionSampler[i]);
			}
		}

		m_Data->HZBDescriptor.resize(framesInFlight);
		for (uint32_t frame = 0; frame < framesInFlight; frame++)
		{
			m_Data->HZBDescriptor[frame].resize(mipLevels);
			for (uint32_t mip = 0; mip < mipLevels; mip++)
			{
				auto& material = m_Data->HZBDescriptor[frame][mip];

				if (!material)
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
					writeDescriptorSet.dstBinding = 2;
					writeDescriptorSet.dstArrayElement = 0;
					writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
					writeDescriptorSet.pImageInfo = &imageDescriptorInfo;
					writeDescriptorSet.descriptorCount = 1;
					writeDescriptorSet.dstSet = descriptorSet;

					vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);

					// `i_Depth`
					auto renderPass = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->GeometryRenderPass;
					auto geometryDepthAttachment = renderPass->GetDepthAttachment(frame);

					vulkanDescriptor->Set("i_Depth_MinReduction", geometryDepthAttachment);
					vulkanDescriptor->Set("i_Depth_MaxReduction", geometryDepthAttachment);
					vulkanDescriptor->UpdateVulkanDescriptorIfNeeded();

					continue;
				}


				// Setting up `i_Depth_MinReduction`
				{
					VkDescriptorImageInfo imageDescriptorInfo{};
					imageDescriptorInfo.imageView = vulkanDepthPyramid->GetVulkanImageViewMip(mip - 1);
					imageDescriptorInfo.imageLayout = vulkanDepthPyramid->GetVulkanImageLayout();
					imageDescriptorInfo.sampler = m_Data->HZBMinReductionSampler[frame];

					VkWriteDescriptorSet writeDescriptorSet{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
					writeDescriptorSet.dstBinding = 0;
					writeDescriptorSet.dstArrayElement = 0;
					writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
					writeDescriptorSet.pImageInfo = &imageDescriptorInfo;
					writeDescriptorSet.descriptorCount = 1;
					writeDescriptorSet.dstSet = descriptorSet;

					vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
				}

				// Setting up `i_Depth_MaxReduction`
				{
					VkDescriptorImageInfo imageDescriptorInfo{};
					imageDescriptorInfo.imageView = vulkanDepthPyramid->GetVulkanImageViewMip(mip - 1);
					imageDescriptorInfo.imageLayout = vulkanDepthPyramid->GetVulkanImageLayout();
					imageDescriptorInfo.sampler = m_Data->HZBMaxReductionSampler[frame];

					VkWriteDescriptorSet writeDescriptorSet{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
					writeDescriptorSet.dstBinding = 1;
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

	void VulkanPostFXPass::VisibilityInitData(uint32_t width, uint32_t height)
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

			imageSpec.Format = ImageFormat::R32F;
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

	void VulkanPostFXPass::AmbientOcclusionInitData(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		// Pipeline creation
		ComputePipeline::CreateInfo computePipelineCreateInfo{};
		computePipelineCreateInfo.Shader = m_Data->AO_Shader;
		if (!m_Data->AO_Pipeline)
			m_Data->AO_Pipeline = ComputePipeline::Create(computePipelineCreateInfo);


		m_Data->AO_Image.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			ImageSpecification imageSpec{};
			imageSpec.Width = width;
			imageSpec.Height = height;
			imageSpec.Sampler.SamplerFilter = ImageFilter::Nearest;
			imageSpec.Sampler.SamplerWrap = ImageWrap::ClampToEdge;

			imageSpec.Format = ImageFormat::R32F;
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
			auto viewPosTexture = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->GeometryRenderPass->GetColorAttachment(2, i);
			auto normalTexture = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->GeometryRenderPass->GetColorAttachment(0, i);

			descriptor->Set("u_ViewPositionTex", viewPosTexture);
			descriptor->Set("u_DepthPyramid", m_Data->DepthPyramid[i]);
			descriptor->Set("u_NormalsTex", normalTexture);
			descriptor->Set("u_NoiseTex", noiseTexture);

			descriptor->Set("o_AOTexture", m_Data->AO_Image[i]);

			vulkanDescriptor->UpdateVulkanDescriptorIfNeeded();

		}
	}

	void VulkanPostFXPass::SpatialDenoiserInitData(uint32_t width, uint32_t height)
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
		m_Data->DenoiserUpsampledImage.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			ImageSpecification imageSpec{};
			imageSpec.Width = width / 2.0;
			imageSpec.Height = height / 2.0;
			imageSpec.Sampler.SamplerFilter = ImageFilter::Nearest;
			imageSpec.Sampler.SamplerWrap = ImageWrap::ClampToEdge;

			imageSpec.Format = ImageFormat::R32F;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.UseMipChain = false;

			m_Data->DenoiserImage[i] = Image2D::Create(imageSpec);


			imageSpec.Width = width;
			imageSpec.Height = height;
			imageSpec.Sampler.SamplerFilter = ImageFilter::Linear;
			m_Data->DenoiserUpsampledImage[i] = Image2D::Create(imageSpec);
		}

		m_Data->DenoiserDescriptor.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			auto& descriptor = m_Data->DenoiserDescriptor[i];

			if (!descriptor)
				descriptor = Material::Create(m_Data->DenoiserShader, "AmbientOcclusionDenoiser_ForBlur");

			auto& vulkanDescriptor = descriptor.As<VulkanMaterial>();

			vulkanDescriptor->Set("u_AOTexture", m_Data->AO_Image[i]);
			vulkanDescriptor->Set("u_DepthTexture", m_Data->AO_Image[i]);        // Does not matter
			vulkanDescriptor->Set("u_FilteredHalfTexture", m_Data->AO_Image[i]); // Does not matter
			vulkanDescriptor->Set("u_NormalTexture", m_Data->AO_Image[i]);       // Does not matter
			vulkanDescriptor->Set("o_Texture", m_Data->DenoiserImage[i]);

			vulkanDescriptor->UpdateVulkanDescriptorIfNeeded();
		}


		m_Data->DenoiserUpsampledDescriptor.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			auto& descriptor = m_Data->DenoiserUpsampledDescriptor[i];

			if (!descriptor)
				descriptor = Material::Create(m_Data->DenoiserShader, "AmbientOcclusionDenoiser_ForUpsampling");

			auto& vulkanDescriptor = descriptor.As<VulkanMaterial>();
			auto& vulkanDepthTex = m_Data->DepthPyramid[i];
			auto& vulkanNormalTexture = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->GeometryRenderPass->GetColorAttachment(0, i);

			vulkanDescriptor->Set("u_AOTexture", m_Data->AO_Image[i]);
			vulkanDescriptor->Set("u_DepthTexture", m_Data->DepthPyramid[i]);
			vulkanDescriptor->Set("u_FilteredHalfTexture", m_Data->DenoiserImage[i]);
			vulkanDescriptor->Set("u_NormalTexture", m_Data->DenoiserImage[i]);
			vulkanDescriptor->Set("o_Texture", m_Data->DenoiserUpsampledImage[i]);

			vulkanDescriptor->UpdateVulkanDescriptorIfNeeded();
		}
	}

	void VulkanPostFXPass::AmbientOcclusionTAAInitData(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		{
			// Pipeline creation
			ComputePipeline::CreateInfo computePipelineCreateInfo{};
			computePipelineCreateInfo.Shader = m_Data->AmbientOcclusionTAAShader;
			if (!m_Data->AmbientOcclusionTAAPipeline)
				m_Data->AmbientOcclusionTAAPipeline = ComputePipeline::Create(computePipelineCreateInfo);
		}

		m_Data->AmbientOcclusionTAAImage.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			ImageSpecification imageSpec{};
			imageSpec.Width = width;
			imageSpec.Height = height;
			imageSpec.Sampler.SamplerFilter = ImageFilter::Linear;
			imageSpec.Sampler.SamplerWrap = ImageWrap::ClampToEdge;

			imageSpec.Format = ImageFormat::R32F;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.UseMipChain = false;

			m_Data->AmbientOcclusionTAAImage[i] = Image2D::Create(imageSpec);
		}

		m_Data->AmbientOcclusionTAADescriptor.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			auto& descriptor = m_Data->AmbientOcclusionTAADescriptor[i];
			if (!descriptor)
				descriptor = Material::Create(m_Data->AmbientOcclusionTAAShader, "AmbientOcclusionTAA");

			auto& vulkanDescriptor = descriptor.As<VulkanMaterial>();

			int32_t lastFrame = i - 1;
			if (i == 0) lastFrame = framesInFlight - 1;

			vulkanDescriptor->Set("u_AOTexture", m_Data->DenoiserUpsampledImage[i]);
			vulkanDescriptor->Set("u_AccumulationBeforeTexture", m_Data->AmbientOcclusionTAAImage[lastFrame]);
			vulkanDescriptor->Set("o_AccumulationFinalTexture", m_Data->AmbientOcclusionTAAImage[i]);
			vulkanDescriptor->Set("u_CurrentDepthBuffer", m_Data->DepthPyramid[i]);
			vulkanDescriptor->Set("u_PreviousDepthBuffer", m_Data->DepthPyramid[lastFrame]);

			vulkanDescriptor->UpdateVulkanDescriptorIfNeeded();
		}
	}

	void VulkanPostFXPass::BloomInitData(uint32_t width, uint32_t height)
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

#if 1
	void VulkanPostFXPass::BloomConvolutionInitData(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		// Pipeline creation
		ComputePipeline::CreateInfo computePipelineCreateInfo{};
		computePipelineCreateInfo.Shader = m_Data->BloomConvolutionShader;
		if (!m_Data->BloomConvPipeline)
			m_Data->BloomConvPipeline = ComputePipeline::Create(computePipelineCreateInfo);

		m_Data->BloomConv_PingTexture.resize(framesInFlight);
		m_Data->BloomConv_PongTexture.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			ImageSpecification imageSpec{};
			imageSpec.Width = width;
			imageSpec.Height = height;
			imageSpec.Sampler.SamplerFilter = ImageFilter::Linear;
			imageSpec.Sampler.SamplerWrap = ImageWrap::ClampToEdge;

			imageSpec.Format = ImageFormat::RGBA32F;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.UseMipChain = false;

			m_Data->BloomConv_PingTexture[i] = Image2D::Create(imageSpec);
			m_Data->BloomConv_PongTexture[i] = Image2D::Create(imageSpec);
		}

		if (!m_Data->BloomConvDescriptor)
			m_Data->BloomConvDescriptor = Material::Create(m_Data->BloomConvolutionShader, "BloomConvolution");
	}
#endif

	void VulkanPostFXPass::BloomConvolutionFilterInitData(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		// Pipeline creation
		ComputePipeline::CreateInfo computePipelineCreateInfo{};
		computePipelineCreateInfo.Shader = m_Data->BloomConvolutionFilterShader;
		if (!m_Data->BloomConvFilterPipeline)
			m_Data->BloomConvFilterPipeline = ComputePipeline::Create(computePipelineCreateInfo);

		m_Data->BloomConvFilterTexture.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			ImageSpecification imageSpec{};
			imageSpec.Width = width;
			imageSpec.Height = height;
			imageSpec.Sampler.SamplerFilter = ImageFilter::Linear;
			imageSpec.Sampler.SamplerWrap = ImageWrap::ClampToEdge;

			imageSpec.Format = ImageFormat::RGBA32F;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.UseMipChain = false;

			m_Data->BloomConvFilterTexture[i] = Image2D::Create(imageSpec);
		}

		m_Data->BloomConvFilterDescriptor.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			if (!m_Data->BloomConvFilterDescriptor[i])
				m_Data->BloomConvFilterDescriptor[i] = Material::Create(m_Data->BloomConvolutionFilterShader, "BloomConvolutionFilter");

			auto& descriptor = m_Data->BloomConvFilterDescriptor[i];
			auto& vulkanDescriptor = descriptor.As<VulkanMaterial>();

			Ref<Image2D> thresholdBloomTex = m_RenderPassPipeline->GetRenderPassData<VulkanCompositePass>()->RenderPass->GetColorAttachment(0, i);

			vulkanDescriptor->Set("u_Texture", thresholdBloomTex);
			vulkanDescriptor->Set("u_ThresholdImage", m_Data->BloomConvFilterTexture[i]);

			vulkanDescriptor->UpdateVulkanDescriptorIfNeeded();
		}

	}

	void VulkanPostFXPass::ApplyAerialInitData(uint32_t width, uint32_t height)
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
			Ref<Image2D> depthBuffer = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->GeometryRenderPass->GetDepthAttachment(i);
			Ref<Image2D> viewPos = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->GeometryRenderPass->GetColorAttachment(2, i);

			vulkanApplyAerialDescriptor->Set("u_AerialLUT", aerialLUT);
			vulkanApplyAerialDescriptor->Set("u_ViewPos", viewPos);
			vulkanApplyAerialDescriptor->Set("u_DepthBuffer", depthBuffer);
			vulkanApplyAerialDescriptor->Set("o_Image", m_Data->ApplyAerialImage[i]);

			vulkanApplyAerialDescriptor->UpdateVulkanDescriptorIfNeeded();
		}
	}

	void VulkanPostFXPass::ColorCorrectionInitData(uint32_t width, uint32_t height)
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
			imageSpec.UseMipChain = false;
			m_Data->FinalTexture[i] = Image2D::Create(imageSpec);
		}

		m_Data->ColorCorrectionDescriptor.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			if (!m_Data->ColorCorrectionDescriptor[i])
				m_Data->ColorCorrectionDescriptor[i] = Material::Create(m_Data->ColorCorrectionShader, "ColorCorrection");

			auto vulkanColorCorrectionDescriptor = m_Data->ColorCorrectionDescriptor[i].As<VulkanMaterial>();

			Ref<Image2D> colorBuffer = m_RenderPassPipeline->GetRenderPassData<VulkanCompositePass>()->RenderPass->GetColorAttachment(0, i); // From the pbr shader
			//Ref<Image2D> volumetricTexture = m_RenderPassPipeline->GetRenderPassData<VulkanVolumetricPass>()->VolumetricBlurTexture_Upsample[i];
			Ref<Image2D> volumetricTexture = m_RenderPassPipeline->GetRenderPassData<VulkanVolumetricPass>()->VolumetricBlurTexture_DirY[i];
			//Ref<Image2D> volumetricTexture = m_RenderPassPipeline->GetRenderPassData<VulkanVolumetricPass>()->VolumetricComputeTexture[i];
			//Ref<Image2D> cloudsTexture = m_RenderPassPipeline->GetRenderPassData<VulkanVolumetricPass>()->CloudComputeBlurTexture_DirY[i];
			Ref<Image2D> cloudsTexture = nullptr;

			vulkanColorCorrectionDescriptor->Set("u_ColorFrameTexture", colorBuffer);
			vulkanColorCorrectionDescriptor->Set("u_BloomTexture", m_Data->Bloom_UpsampledTexture[i]);
			vulkanColorCorrectionDescriptor->Set("u_VolumetricTexture", volumetricTexture);
			vulkanColorCorrectionDescriptor->Set("u_CloudComputeTex", cloudsTexture);
			//vulkanColorCorrectionDescriptor->Set("u_SSRTexture", m_Data->SSRTexture[i]);
			vulkanColorCorrectionDescriptor->Set("o_Texture_ForSSR", m_Data->ColorCorrectionTexture[i]);
			vulkanColorCorrectionDescriptor->Set("o_Texture_Final", m_Data->FinalTexture[i]);
			vulkanColorCorrectionDescriptor->Set("u_SpatialBlueNoiseLUT", Renderer::GetSpatialBlueNoiseLut());

			vulkanColorCorrectionDescriptor->UpdateVulkanDescriptorIfNeeded();
		}
	}

	void VulkanPostFXPass::FXAAInitData(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;
		uint32_t mipLevels = m_Data->ScreenMipLevel;

		{
			ComputePipeline::CreateInfo computePipelineCreateInfo{};
			computePipelineCreateInfo.Shader = m_Data->FXAAShader;

			if (!m_Data->FXAAPipeline)
				m_Data->FXAAPipeline = ComputePipeline::Create(computePipelineCreateInfo);
		}

		m_Data->FXAAImage.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			ImageSpecification imageSpec{};
			imageSpec.Format = ImageFormat::RGBA8;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.Sampler.SamplerFilter = ImageFilter::Linear;
			imageSpec.Sampler.SamplerWrap = ImageWrap::Repeat;
			imageSpec.Width = width;
			imageSpec.Height = height;

			m_Data->FXAAImage[i] = Image2D::Create(imageSpec);
		}

		m_Data->FXAADescriptor.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			if (!m_Data->FXAADescriptor[i])
				m_Data->FXAADescriptor[i] = Material::Create(m_Data->FXAAShader, "FXAA");

			auto vulkanFXAADescriptor = m_Data->FXAADescriptor[i].As<VulkanMaterial>();
			vulkanFXAADescriptor->Set("i_SrcImage", m_Data->TAATextureAcummulation[i]);
			vulkanFXAADescriptor->Set("o_DstImage", m_Data->FXAAImage[i]);
			vulkanFXAADescriptor->UpdateVulkanDescriptorIfNeeded();
		}

	}

	void VulkanPostFXPass::TAAInitData(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;
		uint32_t mipLevels = m_Data->ScreenMipLevel;

		{
			ComputePipeline::CreateInfo computePipelineCreateInfo{};
			computePipelineCreateInfo.Shader = m_Data->TAAShader;

			if (!m_Data->TAAPipeline)
				m_Data->TAAPipeline = ComputePipeline::Create(computePipelineCreateInfo);
		}

		m_Data->TAATextureAcummulation.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			ImageSpecification imageSpec{};
			imageSpec.Format = ImageFormat::RGBA8;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.Sampler.SamplerFilter = ImageFilter::Linear;
			imageSpec.Sampler.SamplerWrap = ImageWrap::Repeat;
			imageSpec.Width = width;
			imageSpec.Height = height;

			m_Data->TAATextureAcummulation[i] = Image2D::Create(imageSpec);
		}

		m_Data->TAADescriptor.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			if (!m_Data->TAADescriptor[i])
				m_Data->TAADescriptor[i] = Material::Create(m_Data->TAAShader, "TAA");

			Ref<Image2D> velocityBuffer = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->GeometryRenderPass->GetColorAttachment(4, i);
			Ref<Image2D> previousAccumulationBuffer = i == 0 ? m_Data->TAATextureAcummulation[framesInFlight - 1] : m_Data->TAATextureAcummulation[i - 1];
			Ref<Image2D> previousDepthBuffer = i == 0 ? m_Data->DepthPyramid[framesInFlight - 1] : m_Data->DepthPyramid[i - 1];

			auto vulkanTAADescriptor = m_Data->TAADescriptor[i].As<VulkanMaterial>();
			vulkanTAADescriptor->Set("u_CurrentColorBuffer", m_Data->FinalTexture[i]);
			vulkanTAADescriptor->Set("u_CurrentDepthBuffer", m_Data->DepthPyramid[i]);
			vulkanTAADescriptor->Set("u_VelocityBuffer", velocityBuffer);
			vulkanTAADescriptor->Set("u_AccumulationColorBuffer", previousAccumulationBuffer);
			vulkanTAADescriptor->Set("u_PreviouDepthBuffer", previousDepthBuffer);
			vulkanTAADescriptor->Set("o_FinalTexture", m_Data->TAATextureAcummulation[i]);
			vulkanTAADescriptor->UpdateVulkanDescriptorIfNeeded();
		}
	}

	void VulkanPostFXPass::OnUpdate(const RenderQueue& renderQueue)
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		auto finalImage = m_RenderPassPipeline->GetRenderPassData<VulkanCompositePass>()->RenderPass->GetColorAttachment(0, currentFrameIndex).As<VulkanImage2D>();
		finalImage->TransitionLayout(cmdBuf, finalImage->GetVulkanImageLayout());

		UpdateRenderingSettings();

		VulkanRenderer::BeginTimeStampPass("Bloom Pass");
		if (m_CompositeSetings.UseBloom)
		{
			BloomUpdate(renderQueue);
		}
		VulkanRenderer::EndTimeStampPass("Bloom Pass");

		VulkanRenderer::BeginTimeStampPass("Color Correction (TARGET_BLOOM)");
		ColorCorrectionUpdate(renderQueue, TARGET_BLOOM);
		VulkanRenderer::EndTimeStampPass("Color Correction (TARGET_BLOOM)");


		VulkanRenderer::BeginTimeStampPass("HZB Builder");
		HZBUpdate(renderQueue);
		VulkanRenderer::EndTimeStampPass("HZB Builder");


		if (m_CompositeSetings.UseSSR)
		{
			VulkanRenderer::BeginTimeStampPass("SSCTR Filter");
			SSRFilterUpdate(renderQueue);
			VulkanRenderer::EndTimeStampPass("SSCTR Filter");

			VulkanRenderer::BeginTimeStampPass("SSCTR Pass");
			SSRUpdate(renderQueue);
			VulkanRenderer::EndTimeStampPass("SSCTR Pass");
		}


		VulkanRenderer::BeginTimeStampPass("AO Pass");
		if (m_CompositeSetings.UseAO)
		{
			AmbientOcclusionUpdate(renderQueue);
			SpatialDenoiserUpdate(renderQueue);
			AmbientOcclusionTAAUpdate(renderQueue);
		}
		VulkanRenderer::EndTimeStampPass("AO Pass");


		VulkanRenderer::BeginTimeStampPass("Color Correction (TARGET_COMPOSITE)");
		ColorCorrectionUpdate(renderQueue, TARGET_COMPOSITE);
		VulkanRenderer::EndTimeStampPass("Color Correction (TARGET_COMPOSITE)");
		
		VulkanRenderer::BeginTimeStampPass("TAA");
		TAAUpdate(renderQueue);
		VulkanRenderer::EndTimeStampPass("TAA");

		VulkanRenderer::BeginTimeStampPass("FXAA");
		FXAAUpdate(renderQueue);
		VulkanRenderer::EndTimeStampPass("FXAA");

	}

	void VulkanPostFXPass::SSRFilterUpdate(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		RendererSettings& rendererSettings = Renderer::GetRendererSettings();

		auto vulkan_BlurPipeline = m_Data->BlurPipeline.As<VulkanComputePipeline>();
		auto vulkan_BlurColorTexture = m_Data->BlurredColorBuffer[currentFrameIndex].As<VulkanImage2D>();

		glm::vec4 currentRes = glm::vec4(renderQueue.ViewPortWidth, renderQueue.ViewPortHeight, 0.0f, 0.0f);
		uint32_t screenMipLevels = rendererSettings.SSR.UseConeTracing ? m_Data->ScreenMipLevel : 1;
		for (uint32_t mipLevel = 0; mipLevel < screenMipLevels; mipLevel++)
		{
			auto vulkan_BlurDescriptor = m_Data->BlurShaderDescriptor[currentFrameIndex][mipLevel].As<VulkanMaterial>();

			if (mipLevel > 0)
			{
				currentRes.x /= 2;
				currentRes.y /= 2;
			}
			currentRes.z = static_cast<float>(mipLevel);

			// Only the first 2 mip should be blurred, the rest should only be downsampled
			currentRes.w = 3;
			if (mipLevel >= 2)
				currentRes.w = 4;

			vulkan_BlurDescriptor->Bind(cmdBuf, m_Data->BlurPipeline);
			vulkan_BlurPipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &currentRes);

			uint32_t groupX = std::ceil(currentRes.x / 32.0f);
			uint32_t groupY = std::ceil(currentRes.y / 32.0f);
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

	void VulkanPostFXPass::SSRUpdate(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		RendererSettings& rendererSettings = Renderer::GetRendererSettings();

		Ref<VulkanMaterial> ssrMaterial = m_Data->SSRDescriptor[currentFrameIndex].As<VulkanMaterial>();
		Ref<VulkanComputePipeline> ssrPipeline = m_Data->SSRPipeline.As<VulkanComputePipeline>();

		glm::mat4 projectionMatrix = renderQueue.CameraProjectionMatrix;
		projectionMatrix[1][1] *= -1;
		glm::mat4 invProjectionMatrix = glm::inverse(projectionMatrix);

		glm::mat4 invViewMatrix = glm::inverse(renderQueue.CameraViewMatrix);



		ssrMaterial->Set("UniformBuffer.ProjectionMatrix", projectionMatrix);
		ssrMaterial->Set("UniformBuffer.InvProjectionMatrix", invProjectionMatrix);

		ssrMaterial->Set("UniformBuffer.ViewMatrix", renderQueue.CameraViewMatrix);
		ssrMaterial->Set("UniformBuffer.InvViewMatrix", invViewMatrix);

		ssrMaterial->Set("UniformBuffer.UseConeTracing", rendererSettings.SSR.UseConeTracing);
		ssrMaterial->Set("UniformBuffer.RayStepCount", rendererSettings.SSR.RayStepCount);
		ssrMaterial->Set("UniformBuffer.RayStepSize", rendererSettings.SSR.RayStepSize);
		ssrMaterial->Set("UniformBuffer.UseHizTracing", rendererSettings.SSR.UseHizTracing);


		ssrMaterial->Bind(cmdBuf, m_Data->SSRPipeline);

		uint32_t groupX = std::ceil((renderQueue.ViewPortWidth / 1.0f) / 32.0f);
		uint32_t groupY = std::ceil((renderQueue.ViewPortHeight / 1.0f) / 32.0f);
		ssrPipeline->Dispatch(cmdBuf, groupX, groupY, 1);
	}

	void VulkanPostFXPass::HZBUpdate(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		// `i_Depth`
		auto renderPass = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->GeometryRenderPass;
		auto geometryDepthAttachment = renderPass->GetDepthAttachment(currentFrameIndex).As<VulkanImage2D>();


		VkImageLayout srcDepthImageLayout = geometryDepthAttachment->GetVulkanImageLayout();
		geometryDepthAttachment->TransitionLayout(cmdBuf, srcDepthImageLayout, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

		glm::vec2 currentHZB_Dimensions = { renderQueue.ViewPortWidth, renderQueue.ViewPortHeight };
		glm::vec3 pushConstant{};

		auto vulkanHZB_Pipeline = m_Data->HZBPipeline.As<VulkanComputePipeline>();

		for (uint32_t mipLevel = 0; mipLevel < m_Data->ScreenMipLevel; mipLevel++)
		{
			auto vulkanHZB_Descriptor = m_Data->HZBDescriptor[currentFrameIndex][mipLevel].As<VulkanMaterial>();
			auto vulkanDepthPyramid = m_Data->DepthPyramid[currentFrameIndex].As<VulkanImage2D>();

			if (mipLevel != 0)
				currentHZB_Dimensions /= glm::vec2(2.0f);


			vulkanHZB_Descriptor->Bind(cmdBuf, m_Data->HZBPipeline);

			pushConstant = { currentHZB_Dimensions.x, currentHZB_Dimensions.y, float(mipLevel) };
			vulkanHZB_Pipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &pushConstant);

			uint32_t groupX = std::ceil(currentHZB_Dimensions.x / 32.0f);
			uint32_t groupY = std::ceil(currentHZB_Dimensions.y / 32.0f);
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

	void VulkanPostFXPass::VisibilityUpdate(const RenderQueue& renderQueue)
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

		visBufferPushConstant.NearPlane = renderQueue.m_Camera->GetNearClip();
		visBufferPushConstant.FarPlane = renderQueue.m_Camera->GetFarClip();

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

		// vec3:                                       x        ||     y,z   
		glm::vec3 AO_Data = glm::vec3(0.0f); // ProjectionScale || ScreenSize
		int32_t AO_Mode = 1; // 0 = HBAO || 1 = GTAO

		int FrameIndex = 0;
		float CameraFOV = 0.0f;

	} static s_AO_pushConstant;

	void VulkanPostFXPass::AmbientOcclusionUpdate(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		RendererSettings& rendererSettings = Renderer::GetRendererSettings();

		auto vulkan_AO_Pipeline = m_Data->AO_Pipeline.As<VulkanComputePipeline>();
		auto vulkan_AO_Descriptor = m_Data->AO_Descriptor[currentFrameIndex].As<VulkanMaterial>();

		glm::mat4 projMatrix = renderQueue.m_Camera->GetProjectionMatrix();
		projMatrix[1][1] *= -1;

		float width = renderQueue.ViewPortWidth;
		float height = renderQueue.ViewPortHeight;
		float fov = glm::radians(renderQueue.m_Camera->GetCameraFOV());
		float farplane = renderQueue.m_Camera->GetFarClip();
		float nearplane = renderQueue.m_Camera->GetNearClip();


		float projectionScale = (height / (2.0f * tanf(fov * 0.5f)));
		s_AO_pushConstant.AO_Mode = rendererSettings.AmbientOcclusion.AOMode;
		s_AO_pushConstant.AO_Data = { projectionScale, width, height };
		s_AO_pushConstant.ViewMatrix = renderQueue.CameraViewMatrix;
		s_AO_pushConstant.InvProjMatrix = glm::inverse(projMatrix);

		s_AO_pushConstant.FrameIndex++;
		if (s_AO_pushConstant.FrameIndex == ~0)
			s_AO_pushConstant.FrameIndex = 0;
		//s_AO_pushConstant.FrameIndex = Renderer::GetFrameCount();
		if (s_AO_pushConstant.FrameIndex > 6)
			s_AO_pushConstant.FrameIndex = 0;

		s_AO_pushConstant.CameraFOV = renderQueue.m_Camera->GetCameraFOV();


		vulkan_AO_Descriptor->Bind(cmdBuf, m_Data->AO_Pipeline);

		vulkan_AO_Pipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &s_AO_pushConstant);

		uint32_t groupX = static_cast<uint32_t>(std::ceil(std::ceil(width / 2.0f) / 8.0f));
		uint32_t groupY = static_cast<uint32_t>(std::ceil(std::ceil(height / 2.0f) / 8.0f));
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

	void VulkanPostFXPass::SpatialDenoiserUpdate(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		auto vulkan_denoiser_Pipeline = m_Data->DenoiserPipeline.As<VulkanComputePipeline>();
		auto vulkan_denoiser_Descriptor = m_Data->DenoiserDescriptor[currentFrameIndex].As<VulkanMaterial>();

		vulkan_denoiser_Descriptor->Bind(cmdBuf, m_Data->DenoiserPipeline);

		float width = renderQueue.ViewPortWidth;
		float height = renderQueue.ViewPortHeight;

		/// /////////////////////////////////////////
		/// Bluring the noise ///////////////////////
		/// /////////////////////////////////////////
		uint32_t IsUpsamplePass = 0;
		vulkan_denoiser_Pipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &IsUpsamplePass);
		uint32_t groupX = std::ceil((width / 2.0) / 32.0f);
		uint32_t groupY = std::ceil((height / 2.0) / 32.0f);
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


		/// /////////////////////////////////////////
		/// Upsampling the blurred texture //////////
		/// /////////////////////////////////////////

		auto vulkan_denoiser_upsampled_Descriptor = m_Data->DenoiserUpsampledDescriptor[currentFrameIndex].As<VulkanMaterial>();
		vulkan_denoiser_upsampled_Descriptor->Bind(cmdBuf, m_Data->DenoiserPipeline);

		IsUpsamplePass = 1;
		vulkan_denoiser_Pipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &IsUpsamplePass);
		groupX = std::ceil((width) / 32.0f);
		groupY = std::ceil((height) / 32.0f);
		vulkan_denoiser_Pipeline->Dispatch(cmdBuf, groupX, groupY, 1);

		Ref<VulkanImage2D> vulkanDenoisedUpsampledAOTexture = m_Data->DenoiserUpsampledImage[currentFrameIndex].As<VulkanImage2D>();

		Utils::InsertImageMemoryBarrier(cmdBuf, vulkanDenoisedUpsampledAOTexture->GetVulkanImage(),
			VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
			vulkanDenoisedUpsampledAOTexture->GetVulkanImageLayout(), vulkanDenoisedUpsampledAOTexture->GetVulkanImageLayout(),
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			imageSubrange
		);
	}


	struct AmbientOcclusionTAAData
	{
		//mat4 InvViewProjMatrix;
		//mat4 PreviousViewProjMatrix;
		glm::mat4 CurrentViewProjMatrix{ 1.0f };
		glm::mat4 CurrentInvViewProjMatrix { 1.0f };

		glm::mat4 PreviousViewProjMatrix{ 1.0f };
		glm::mat4 PreviousInvViewProjMatrix{ 1.0f };

	};
	static AmbientOcclusionTAAData s_AmbientOcclusionTAAData;

	void VulkanPostFXPass::AmbientOcclusionTAAUpdate(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		auto vulkan_AO_TAA_Pipeline = m_Data->AmbientOcclusionTAAPipeline.As<VulkanComputePipeline>();
		auto vulkan_AO_TAA_Descriptor = m_Data->AmbientOcclusionTAADescriptor[currentFrameIndex].As<VulkanMaterial>();

		// Configure the data
		s_AmbientOcclusionTAAData.PreviousViewProjMatrix = s_AmbientOcclusionTAAData.CurrentViewProjMatrix;
		s_AmbientOcclusionTAAData.PreviousInvViewProjMatrix = s_AmbientOcclusionTAAData.CurrentInvViewProjMatrix;

		s_AmbientOcclusionTAAData.CurrentViewProjMatrix = renderQueue.m_Camera->GetViewProjectionVK();
		s_AmbientOcclusionTAAData.CurrentInvViewProjMatrix = glm::inverse(s_AmbientOcclusionTAAData.CurrentViewProjMatrix);


		vulkan_AO_TAA_Descriptor->Set("UniformBuffer.CurrentInvViewProjMatrix", s_AmbientOcclusionTAAData.CurrentInvViewProjMatrix);
		vulkan_AO_TAA_Descriptor->Set("UniformBuffer.PreviousViewProjMatrix", s_AmbientOcclusionTAAData.PreviousViewProjMatrix);
		vulkan_AO_TAA_Descriptor->Set("UniformBuffer.PreviousInvViewProjMatrix", s_AmbientOcclusionTAAData.PreviousInvViewProjMatrix);
		vulkan_AO_TAA_Descriptor->Set("UniformBuffer.CameraNearClip", renderQueue.m_Camera->GetNearClip());
		vulkan_AO_TAA_Descriptor->Set("UniformBuffer.CameraFarClip", renderQueue.m_Camera->GetFarClip());
		vulkan_AO_TAA_Descriptor->Bind(cmdBuf, m_Data->AmbientOcclusionTAAPipeline);

		float width = renderQueue.ViewPortWidth;
		float height = renderQueue.ViewPortHeight;
		//vulkan_AO_TAA_Pipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &s_AmbientOcclusionTAAPushConstant);
		uint32_t groupX = std::ceil(width / 32.0f);
		uint32_t groupY = std::ceil(height / 32.0f);
		vulkan_AO_TAA_Pipeline->Dispatch(cmdBuf, groupX, groupY, 1);


		Ref<VulkanImage2D> vulkanAmbientOcclusionTAATexture = m_Data->AmbientOcclusionTAAImage[currentFrameIndex].As<VulkanImage2D>();

		VkImageSubresourceRange imageSubrange{};
		imageSubrange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageSubrange.baseArrayLayer = 0;
		imageSubrange.baseMipLevel = 0;
		imageSubrange.layerCount = 1;
		imageSubrange.levelCount = 1;

		Utils::InsertImageMemoryBarrier(cmdBuf, vulkanAmbientOcclusionTAATexture->GetVulkanImage(),
			VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
			vulkanAmbientOcclusionTAATexture->GetVulkanImageLayout(), vulkanAmbientOcclusionTAATexture->GetVulkanImageLayout(),
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			imageSubrange
		);
	}

	void VulkanPostFXPass::ColorCorrectionUpdate(const RenderQueue& renderQueue, uint32_t target)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		// cc = color correction
		auto vulkan_cc_Pipeline = m_Data->ColorCorrectionPipeline.As<VulkanComputePipeline>();
		auto vulkan_cc_Descriptor = m_Data->ColorCorrectionDescriptor[currentFrameIndex].As<VulkanMaterial>();

		vulkan_cc_Descriptor->Bind(cmdBuf, m_Data->ColorCorrectionPipeline);

		m_CompositeSetings.Gamma = 2.2f;
		m_CompositeSetings.Exposure = renderQueue.m_Camera->GetExposure();
		m_CompositeSetings.Stage = static_cast<float>(target);
		vulkan_cc_Pipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &m_CompositeSetings);

		float width = renderQueue.ViewPortWidth;
		float height = renderQueue.ViewPortHeight;

		uint32_t groupX = std::ceil(width / 32.0f);
		uint32_t groupY = std::ceil(height / 32.0f);
		vulkan_cc_Pipeline->Dispatch(cmdBuf, groupX, groupY, 1);
	}

	void VulkanPostFXPass::ApplyAerialUpdate(const RenderQueue& renderQueue)
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
		vulkanApplyAerialDescriptor->Set("CameraBlock.InvViewProjMatrix", glm::inverse(renderQueue.m_Camera->GetViewProjection()));
		vulkanApplyAerialDescriptor->Set("CameraBlock.CamPosition", glm::vec4(renderQueue.CameraPosition, 0.0f));
		vulkanApplyAerialDescriptor->Set("CameraBlock.NearFarPlane", glm::vec4(renderQueue.m_Camera->GetNearClip(), renderQueue.m_Camera->GetFarClip(), 0.0f, 0.0f));

		float width = renderQueue.ViewPortWidth;
		float height = renderQueue.ViewPortHeight;

		uint32_t groupX = std::ceil(width / 32.0f);
		uint32_t groupY = std::ceil(height / 32.0f);
		vulkanApplyAerialPipeline->Dispatch(cmdBuf, groupX, groupY, 1);
	}


	void VulkanPostFXPass::BloomConvolutionUpdate(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		//RendererSettings& rendererSettings = Renderer::GetRendererSettings();

		Ref<VulkanMaterial> vulkanBloomConvDescriptor = m_Data->BloomConvDescriptor.As<VulkanMaterial>();
		Ref<VulkanComputePipeline> vulkanBloomConvPipeline = m_Data->BloomConvPipeline.As<VulkanComputePipeline>();

		Ref<VulkanImage2D> vulkanPingFFTTex = m_Data->BloomConv_PingTexture[currentFrameIndex].As<VulkanImage2D>();
		Ref<VulkanImage2D> vulkanPongFFTTex = m_Data->BloomConv_PongTexture[currentFrameIndex].As<VulkanImage2D>();

		//Ref<VulkanImage2D> vulkanThresholdBloomTex = m_Data->Bloom_DownsampledTexture[currentFrameIndex].As<VulkanImage2D>();
		//Ref<VulkanImage2D> vulkanThresholdBloomTex = m_RenderPassPipeline->GetRenderPassData<VulkanCompositePass>()->RenderPass->GetColorAttachment(0, currentFrameIndex).As<VulkanImage2D>();



		VkDescriptorSetLayout descriptorSetLayout = vulkanBloomConvDescriptor->GetVulkanDescriptorLayout()[0];



		struct BloomComputePushConstants
		{
			glm::vec2 Resolution;
			float SubtransformSize;
			float Horizontal;
			float IsNotInverse;
			float Normalization;
		} bloomConvPushConstant;

		float workGroupSize = 32.0f;
		uint32_t width = renderQueue.ViewPortWidth;
		uint32_t height = renderQueue.ViewPortHeight;

		float xIterations = std::round(std::log2(width));
		float yIterations = std::round(std::log2(height));
		float iterations = xIterations + yIterations;

		bool pingPong = false;

		for (int i = 0; i < iterations; i++)
		{
			VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = &descriptorSetLayout;
			VkDescriptorSet descriptorSet = VulkanRenderer::AllocateDescriptorSet(allocInfo);

			if (i == 0)
			{
				// Input image
				auto inputImageInfo = m_Data->BloomConvFilterTexture[currentFrameIndex].As<VulkanImage2D>()->GetVulkanDescriptorInfo(DescriptorImageType::Sampled);

				VkWriteDescriptorSet wds{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				wds.dstBinding = 0;
				wds.dstArrayElement = 0;
				wds.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				wds.pImageInfo = &inputImageInfo;
				wds.descriptorCount = 1;
				wds.dstSet = descriptorSet;
				vkUpdateDescriptorSets(device, 1, &wds, 0, nullptr);

				// Output image
				auto outputImageInfo = vulkanPingFFTTex->GetVulkanDescriptorInfo(DescriptorImageType::Storage);

				//wds{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				wds.dstBinding = 1;
				wds.dstArrayElement = 0;
				wds.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				wds.pImageInfo = &outputImageInfo;
				wds.descriptorCount = 1;
				wds.dstSet = descriptorSet;
				vkUpdateDescriptorSets(device, 1, &wds, 0, nullptr);
			}
			else
			{
				// 0 - sampler, 1 - image
				Ref<VulkanImage2D> FFTTexSampler, FFTTexImage;

				if (pingPong)
				{
					FFTTexSampler = vulkanPingFFTTex;
					FFTTexImage = vulkanPongFFTTex;
				}
				else
				{
					FFTTexImage = vulkanPingFFTTex;
					FFTTexSampler = vulkanPongFFTTex;
				}


				{
					// Input texture
					auto inputImageInfo = FFTTexSampler->GetVulkanDescriptorInfo(DescriptorImageType::Sampled);

					VkWriteDescriptorSet wds{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
					wds.dstBinding = 0;
					wds.dstArrayElement = 0;
					wds.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
					wds.pImageInfo = &inputImageInfo;
					wds.descriptorCount = 1;
					wds.dstSet = descriptorSet;
					vkUpdateDescriptorSets(device, 1, &wds, 0, nullptr);


					// Output texture
					auto outputImageInfo = FFTTexImage->GetVulkanDescriptorInfo(DescriptorImageType::Storage);

					//wds{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
					wds.dstBinding = 1;
					wds.dstArrayElement = 0;
					wds.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
					wds.pImageInfo = &outputImageInfo;
					wds.descriptorCount = 1;
					wds.dstSet = descriptorSet;
					vkUpdateDescriptorSets(device, 1, &wds, 0, nullptr);
				}





			}


			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, vulkanBloomConvPipeline->GetVulkanPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);

			bloomConvPushConstant.Horizontal = i < xIterations;
			bloomConvPushConstant.Resolution = { (float)width, (float)height };
			bloomConvPushConstant.IsNotInverse = 1.0f;
			//bloomConvPushConstant.Normalization = 0.0f; //TODO: Im not sure ??
			bloomConvPushConstant.SubtransformSize = std::pow(2, (bloomConvPushConstant.Horizontal ? i : (i - xIterations)) + 1);

			if (i == 0)
				bloomConvPushConstant.Normalization = 1.0f;
			else
				bloomConvPushConstant.Normalization = 0.0f;

			vulkanBloomConvPipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &bloomConvPushConstant);


			uint32_t workGroupsX = std::ceil(width / workGroupSize);
			uint32_t workGroupsY = std::ceil(height / workGroupSize);
			vulkanBloomConvPipeline->Dispatch(cmdBuf, workGroupsX, workGroupsY, 1);

			if (pingPong)
				vulkanPongFFTTex->TransitionLayout(cmdBuf, vulkanPongFFTTex->GetVulkanImageLayout(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
			else
				vulkanPingFFTTex->TransitionLayout(cmdBuf, vulkanPingFFTTex->GetVulkanImageLayout(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
			pingPong = !pingPong;
		}





		///
		/// INVERSE
		/// 
		/// 

		for (int i = 0; i < iterations; i++)
		{
			VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = &descriptorSetLayout;
			VkDescriptorSet descriptorSet = VulkanRenderer::AllocateDescriptorSet(allocInfo);



			{
				// 0 - sampler, 1 - image
				Ref<VulkanImage2D> FFTTexSampler, FFTTexImage;

				if (pingPong)
				{
					FFTTexSampler = vulkanPingFFTTex;
					FFTTexImage = vulkanPongFFTTex;
				}
				else
				{
					FFTTexImage = vulkanPingFFTTex;
					FFTTexSampler = vulkanPongFFTTex;
				}


				{
					// Input texture
					auto inputImageInfo = FFTTexSampler->GetVulkanDescriptorInfo(DescriptorImageType::Sampled);

					VkWriteDescriptorSet wds{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
					wds.dstBinding = 0;
					wds.dstArrayElement = 0;
					wds.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
					wds.pImageInfo = &inputImageInfo;
					wds.descriptorCount = 1;
					wds.dstSet = descriptorSet;
					vkUpdateDescriptorSets(device, 1, &wds, 0, nullptr);


					// Output texture
					auto outputImageInfo = FFTTexImage->GetVulkanDescriptorInfo(DescriptorImageType::Storage);

					//wds{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
					wds.dstBinding = 1;
					wds.dstArrayElement = 0;
					wds.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
					wds.pImageInfo = &outputImageInfo;
					wds.descriptorCount = 1;
					wds.dstSet = descriptorSet;
					vkUpdateDescriptorSets(device, 1, &wds, 0, nullptr);
				}





			}


			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, vulkanBloomConvPipeline->GetVulkanPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);

			bloomConvPushConstant.Horizontal = i < xIterations;
			bloomConvPushConstant.Resolution = { (float)width, (float)height };
			bloomConvPushConstant.IsNotInverse = 0.0f;

			if (i == 0)
				bloomConvPushConstant.Normalization = 1.0f;
			else
				bloomConvPushConstant.Normalization = 0.0f;

			bloomConvPushConstant.SubtransformSize = std::pow(2, (bloomConvPushConstant.Horizontal ? i : (i - xIterations)) + 1);

			vulkanBloomConvPipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &bloomConvPushConstant);


			uint32_t workGroupsX = std::ceil(width / workGroupSize);
			uint32_t workGroupsY = std::ceil(height / workGroupSize);
			vulkanBloomConvPipeline->Dispatch(cmdBuf, workGroupsX, workGroupsY, 1);

			if (pingPong)
				vulkanPongFFTTex->TransitionLayout(cmdBuf, vulkanPongFFTTex->GetVulkanImageLayout(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
			else
				vulkanPingFFTTex->TransitionLayout(cmdBuf, vulkanPingFFTTex->GetVulkanImageLayout(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
			pingPong = !pingPong;
		}
	}

	void VulkanPostFXPass::BloomConvolutionFilterUpdate(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		auto vulkanBloomConvFilterPipeline = m_Data->BloomConvFilterPipeline.As<VulkanComputePipeline>();
		auto vulkanBloomConvFilterDescriptor = m_Data->BloomConvFilterDescriptor[currentFrameIndex].As<VulkanMaterial>();

		vulkanBloomConvFilterDescriptor->Bind(cmdBuf, m_Data->BloomConvFilterPipeline);

		float width = renderQueue.ViewPortWidth;
		float height = renderQueue.ViewPortHeight;

		uint32_t groupX = std::ceil(width / 32.0f);
		uint32_t groupY = std::ceil(height / 32.0f);
		vulkanBloomConvFilterPipeline->Dispatch(cmdBuf, groupX, groupY, 1);

		Ref<VulkanImage2D> vulkanBloomConvFilterImage = m_Data->BloomConvFilterTexture[currentFrameIndex].As<VulkanImage2D>();
		vulkanBloomConvFilterImage->TransitionLayout(
			cmdBuf,
			vulkanBloomConvFilterImage->GetVulkanImageLayout(),
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
		);

	}

#if 0
	void VulkanPostFXPass::BloomConvolutionUpdate(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		RendererSettings& rendererSettings = Renderer::GetRendererSettings();

		Ref<VulkanMaterial> vulkanBloomConvDescriptor = m_Data->BloomConvDescriptor.As<VulkanMaterial>();
		Ref<VulkanComputePipeline> vulkanBloomConvPipeline = m_Data->BloomConvPipeline.As<VulkanComputePipeline>();


		Ref<VulkanImage2D> vulkanPingFFTTex = m_Data->BloomConv_PingTexture[currentFrameIndex].As<VulkanImage2D>();
		Ref<VulkanImage2D> vulkanPongFFTTex = m_Data->BloomConv_PongTexture[currentFrameIndex].As<VulkanImage2D>();

		//Ref<VulkanImage2D> vulkanThresholdBloomTex = m_Data->Bloom_DownsampledTexture[currentFrameIndex].As<VulkanImage2D>();
		Ref<VulkanImage2D> vulkanThresholdBloomTex = m_RenderPassPipeline->GetRenderPassData<VulkanCompositePass>()->RenderPass->GetColorAttachment(0, currentFrameIndex).As<VulkanImage2D>();



		VkDescriptorSetLayout descriptorSetLayout = vulkanBloomConvDescriptor->GetVulkanDescriptorLayout()[0];



		struct BloomComputePushConstants
		{
			glm::vec2 Resolution;
			float SubtransformSize;
			float Horizontal;
			float IsNotInverse;
			float Normalization;
		} bloomConvPushConstant;

		float workGroupSize = 32.0f;
		uint32_t width = renderQueue.ViewPortWidth;
		uint32_t height = renderQueue.ViewPortHeight;
		//float log2Result = std::log(2);

		float xIterations = std::round(std::log2(width));
		float yIterations = std::round(std::log2(height));
		float iterations = xIterations + yIterations;

		bool pingPong = false;

		for (int i = 0; i < iterations; i++)
		{

			VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = &descriptorSetLayout;
			VkDescriptorSet descriptorSet = VulkanRenderer::AllocateDescriptorSet(allocInfo);

			if (i == 0)
			{
				// Input texture
				{
					auto inputImageInfo = vulkanThresholdBloomTex->GetVulkanDescriptorInfo(DescriptorImageType::Sampled);

					VkWriteDescriptorSet wds{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
					wds.dstBinding = 0;
					wds.dstArrayElement = 0;
					wds.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
					wds.pImageInfo = &inputImageInfo;
					wds.descriptorCount = 1;
					wds.dstSet = descriptorSet;

					vkUpdateDescriptorSets(device, 1, &wds, 0, nullptr);

				}
				{
					auto outputImageInfo = vulkanPingFFTTex->GetVulkanDescriptorInfo(DescriptorImageType::Storage);

					VkWriteDescriptorSet wds{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
					wds.dstBinding = 1;
					wds.dstArrayElement = 0;
					wds.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
					wds.pImageInfo = &outputImageInfo;
					wds.descriptorCount = 1;
					wds.dstSet = descriptorSet;

					vkUpdateDescriptorSets(device, 1, &wds, 0, nullptr);
				}
			}
			else
			{
				// 0 - sampler, 1 - image
				Ref<VulkanImage2D> FFTTex0, FFTTex1;

				if (pingPong)
				{
					FFTTex0 = vulkanPingFFTTex;
					FFTTex1 = vulkanPongFFTTex;
				}
				else
				{
					FFTTex1 = vulkanPingFFTTex;
					FFTTex0 = vulkanPongFFTTex;
				}


				// Input texture
				{
					// Get the prefiltered (blurred) color buffer
					// I could've used the normal buffer, from the renderpass, but I'm lazy to write more code
					auto inputImageInfo = FFTTex0->GetVulkanDescriptorInfo(DescriptorImageType::Sampled);

					VkWriteDescriptorSet wds{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
					wds.dstBinding = 0;
					wds.dstArrayElement = 0;
					wds.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
					wds.pImageInfo = &inputImageInfo;
					wds.descriptorCount = 1;
					wds.dstSet = descriptorSet;

					vkUpdateDescriptorSets(device, 1, &wds, 0, nullptr);

				}
				{
					auto outputImageInfo = FFTTex1->GetVulkanDescriptorInfo(DescriptorImageType::Storage);

					VkWriteDescriptorSet wds{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
					wds.dstBinding = 1;
					wds.dstArrayElement = 0;
					wds.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
					wds.pImageInfo = &outputImageInfo;
					wds.descriptorCount = 1;
					wds.dstSet = descriptorSet;

					vkUpdateDescriptorSets(device, 1, &wds, 0, nullptr);
				}


			}

			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, vulkanBloomConvPipeline->GetVulkanPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);

			bloomConvPushConstant.Horizontal = i < xIterations;
			bloomConvPushConstant.Resolution = { (float)width, (float)height };
			bloomConvPushConstant.IsNotInverse = 1.0f;
			bloomConvPushConstant.Normalization = 0.0f; //TODO: Im not sure ??
			bloomConvPushConstant.SubtransformSize = std::pow(2, (bloomConvPushConstant.Horizontal ? i : (i - xIterations)) + 1);;

			vulkanBloomConvPipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &bloomConvPushConstant);



			uint32_t workGroupsX = std::ceil(width / workGroupSize);
			uint32_t workGroupsY = std::ceil(height / workGroupSize);
			vulkanBloomConvPipeline->Dispatch(cmdBuf, workGroupsX, workGroupsY, 1);

			if (pingPong)
			{
				vulkanPongFFTTex->TransitionLayout(cmdBuf, vulkanPongFFTTex->GetVulkanImageLayout(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
			}
			else
			{
				vulkanPingFFTTex->TransitionLayout(cmdBuf, vulkanPingFFTTex->GetVulkanImageLayout(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
			}
			pingPong = !pingPong;
		}

#if 0

		for (int i = 0; i < 1; i++)
		{

			VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = &descriptorSetLayout;
			VkDescriptorSet descriptorSet = VulkanRenderer::AllocateDescriptorSet(allocInfo);

			// 0 - sampler, 1 - image
			Ref<VulkanImage2D> FFTTex0, FFTTex1;

			if (pingPong)
			{
				FFTTex0 = vulkanPingFFTTex;
				FFTTex1 = vulkanPongFFTTex;
			}
			else
			{
				FFTTex1 = vulkanPingFFTTex;
				FFTTex0 = vulkanPongFFTTex;
			}


			// Input texture
			{
				// Get the prefiltered (blurred) color buffer
				// I could've used the normal buffer, from the renderpass, but I'm lazy to write more code
				auto inputImageInfo = FFTTex0->GetVulkanDescriptorInfo(DescriptorImageType::Sampled);

				VkWriteDescriptorSet wds{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				wds.dstBinding = 0;
				wds.dstArrayElement = 0;
				wds.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				wds.pImageInfo = &inputImageInfo;
				wds.descriptorCount = 1;
				wds.dstSet = descriptorSet;

				vkUpdateDescriptorSets(device, 1, &wds, 0, nullptr);

			}
			{
				auto outputImageInfo = FFTTex1->GetVulkanDescriptorInfo(DescriptorImageType::Storage);

				VkWriteDescriptorSet wds{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				wds.dstBinding = 1;
				wds.dstArrayElement = 0;
				wds.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				wds.pImageInfo = &outputImageInfo;
				wds.descriptorCount = 1;
				wds.dstSet = descriptorSet;

				vkUpdateDescriptorSets(device, 1, &wds, 0, nullptr);
			}


			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, vulkanBloomConvPipeline->GetVulkanPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);

			bloomConvPushConstant.Horizontal = i < xIterations;
			bloomConvPushConstant.Resolution = { (float)width, (float)height };
			bloomConvPushConstant.IsNotInverse = 0.0f;

			if (i == 0)
			{
				bloomConvPushConstant.Normalization = 1.0f;
			}
			else
			{
				bloomConvPushConstant.Normalization = 0.0f;
			}


			bloomConvPushConstant.SubtransformSize = std::pow(2, (i % int32_t(iterations / 2)) + 1);
			//bloomConvPushConstant.SubtransformSize = std::pow(2, (bloomConvPushConstant.Horizontal ? i : (i - xIterations)) + 1);;

			vulkanBloomConvPipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &bloomConvPushConstant);



			uint32_t workGroupsX = std::ceil(width / workGroupSize);
			uint32_t workGroupsY = std::ceil(height / workGroupSize);
			vulkanBloomConvPipeline->Dispatch(cmdBuf, workGroupsX, workGroupsY, 1);

			if (pingPong)
			{
				vulkanPongFFTTex->TransitionLayout(cmdBuf, vulkanPongFFTTex->GetVulkanImageLayout(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
			}
			else
			{
				vulkanPingFFTTex->TransitionLayout(cmdBuf, vulkanPingFFTTex->GetVulkanImageLayout(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
			}
			pingPong = !pingPong;
		}
#endif

	}
#endif

	void VulkanPostFXPass::BloomUpdate(const RenderQueue& renderQueue)
	{
		if (m_Data->ScreenMipLevel <= 1) return;

		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		RendererSettings& rendererSettings = Renderer::GetRendererSettings();

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


		bloomPushConstant.Params = { rendererSettings.Bloom.Threshold,
									 rendererSettings.Bloom.Threshold - rendererSettings.Bloom.Knee,
									 rendererSettings.Bloom.Knee * 2.0f,
									 0.25f / rendererSettings.Bloom.Knee
		};


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

			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, vulkanBloomPipeline->GetVulkanPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);

			vulkanBloomPipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &bloomPushConstant);

			uint32_t width = renderQueue.ViewPortWidth;
			uint32_t height = renderQueue.ViewPortHeight;

			uint32_t workGroupsX = std::ceil(width / workGroupSize);
			uint32_t workGroupsY = std::ceil(height / workGroupSize);
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

		//BloomConvolutionFilterUpdate(renderQueue);
		//BloomConvolutionUpdate(renderQueue);


#if 1
		/// Part 2: Downsample the prefiltered thresholded texture
		{
			uint32_t totalMips = int32_t(m_Data->ScreenMipLevel - 2) > 1 ?
				(m_Data->ScreenMipLevel - 2) :
				1;

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

				uint32_t workGroupsX = (uint32_t)std::ceil((float)mipWidth / workGroupSize);
				uint32_t workGroupsY = (uint32_t)std::ceil((float)mipHeight / workGroupSize);


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
			uint32_t mip = int32_t(m_Data->ScreenMipLevel - 4) > 1 ?
				(m_Data->ScreenMipLevel - 4) :
				1;

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

			uint32_t workGroupsX = (uint32_t)std::ceil((float)mipWidth / workGroupSize);
			uint32_t workGroupsY = (uint32_t)std::ceil((float)mipHeight / workGroupSize);
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
			uint32_t mipInit = int32_t(m_Data->ScreenMipLevel - 4) > 1 ?
				(m_Data->ScreenMipLevel - 4) :
				1;

			for (uint32_t mip = mipInit; mip >= 1; mip--)
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

				uint32_t workGroupsX = (uint32_t)std::ceil((float)mipWidth / workGroupSize);
				uint32_t workGroupsY = (uint32_t)std::ceil((float)mipHeight / workGroupSize);


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
#endif
	}

	void VulkanPostFXPass::FXAAUpdate(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		RendererSettings& rendererSettings = Renderer::GetRendererSettings();

		auto vulkan_FXAA_Pipeline = m_Data->FXAAPipeline.As<VulkanComputePipeline>();
		auto vulkan_FXAA_Descriptor = m_Data->FXAADescriptor[currentFrameIndex].As<VulkanMaterial>();

		vulkan_FXAA_Descriptor->Bind(cmdBuf, m_Data->FXAAPipeline);

		uint32_t groupX = static_cast<uint32_t>(std::ceil((renderQueue.ViewPortWidth / 1.0f) / 32.0f));
		uint32_t groupY = static_cast<uint32_t>(std::ceil((renderQueue.ViewPortHeight / 1.0f) / 32.0f));
		vulkan_FXAA_Pipeline->Dispatch(cmdBuf, groupX, groupY, 1);


		Ref<VulkanImage2D> vulkanFXAATexture = m_Data->FXAAImage[currentFrameIndex].As<VulkanImage2D>();

		VkImageSubresourceRange imageSubrange{};
		imageSubrange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageSubrange.baseArrayLayer = 0;
		imageSubrange.baseMipLevel = 0;
		imageSubrange.layerCount = 1;
		imageSubrange.levelCount = 1;

		Utils::InsertImageMemoryBarrier(cmdBuf, vulkanFXAATexture->GetVulkanImage(),
			VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
			vulkanFXAATexture->GetVulkanImageLayout(), vulkanFXAATexture->GetVulkanImageLayout(),
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			imageSubrange
		);
	}

	void VulkanPostFXPass::TAAUpdate(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		RendererSettings& rendererSettings = Renderer::GetRendererSettings();

		auto vulkan_TAA_Pipeline = m_Data->TAAPipeline.As<VulkanComputePipeline>();
		auto vulkan_TAA_Descriptor = m_Data->TAADescriptor[currentFrameIndex].As<VulkanMaterial>();

		vulkan_TAA_Descriptor->Bind(cmdBuf, m_Data->TAAPipeline);

		glm::vec2 pushConstant = { 0.05f, renderQueue.m_Camera->GetNearClip() };
		vulkan_TAA_Pipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &pushConstant);

		uint32_t groupX = static_cast<uint32_t>(std::ceil((renderQueue.ViewPortWidth / 1.0f) / 32.0f));
		uint32_t groupY = static_cast<uint32_t>(std::ceil((renderQueue.ViewPortHeight / 1.0f) / 32.0f));
		vulkan_TAA_Pipeline->Dispatch(cmdBuf, groupX, groupY, 1);


		Ref<VulkanImage2D> vulkanTAATexture = m_Data->TAATextureAcummulation[currentFrameIndex].As<VulkanImage2D>();

		VkImageSubresourceRange imageSubrange{};
		imageSubrange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageSubrange.baseArrayLayer = 0;
		imageSubrange.baseMipLevel = 0;
		imageSubrange.layerCount = 1;
		imageSubrange.levelCount = 1;

		Utils::InsertImageMemoryBarrier(cmdBuf, vulkanTAATexture->GetVulkanImage(),
			VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
			vulkanTAATexture->GetVulkanImageLayout(), vulkanTAATexture->GetVulkanImageLayout(),
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			imageSubrange
		);
	}

	void VulkanPostFXPass::OnResize(uint32_t width, uint32_t height)
	{
		CalculateMipLevels(width, height);
		BloomInitData(width, height);
		//BloomConvolutionInitData (width, height);
		//BloomConvolutionFilterInitData(width, height);
		//ApplyAerialInitData      (width, height);
		ColorCorrectionInitData(width, height);
		SSRFilterInitData(width, height);
		HZBInitData(width, height);
		//VisibilityInitData       (width, height);
		AmbientOcclusionInitData(width / 2.0, height / 2.0);
		SpatialDenoiserInitData(width, height);
		AmbientOcclusionTAAInitData(width, height);
		SSRInitData(width / 1.0, height / 1.0);
		TAAInitData(width, height);
		FXAAInitData(width, height);

		Renderer::SubmitImageToOutputImageMap("FinalImage", [this]() -> Ref<Image2D>
		{
			uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
			return this->m_Data->FinalTexture[currentFrameIndex];
		});

		Renderer::SubmitImageToOutputImageMap("SSR", [this]() -> Ref<Image2D>
		{
			uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
			return this->m_Data->SSRTexture[currentFrameIndex];
		});

		Renderer::SubmitImageToOutputImageMap("AO", [this]() -> Ref<Image2D>
		{
			uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
			return this->m_Data->AmbientOcclusionTAAImage[currentFrameIndex];
		});

		Renderer::SubmitImageToOutputImageMap("TAA", [this]() -> Ref<Image2D>
		{
			uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
			return this->m_Data->TAATextureAcummulation[currentFrameIndex];
		});

		Renderer::SubmitImageToOutputImageMap("FXAA", [this]() -> Ref<Image2D>
		{
			uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
			return this->m_Data->FXAAImage[currentFrameIndex];
		});
	}

	void VulkanPostFXPass::OnResizeLate(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;

		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			auto vulkanColorCorrectionDescriptor = m_Data->ColorCorrectionDescriptor[i].As<VulkanMaterial>();

			vulkanColorCorrectionDescriptor->Set("u_SSRTexture", m_Data->SSRTexture[i]);
			vulkanColorCorrectionDescriptor->Set("u_AOTexture", m_Data->AmbientOcclusionTAAImage[i]);
			vulkanColorCorrectionDescriptor->UpdateVulkanDescriptorIfNeeded();
		}
	}

	void VulkanPostFXPass::OnRenderDebug()
	{
		RendererSettings& rendererSettings = Renderer::GetRendererSettings();
		if (ImGui::CollapsingHeader("Screen Space Reflections"))
		{
			ImGui::SliderInt("Enable", &rendererSettings.SSR.Enabled, 0, 1);
			ImGui::Separator();
			ImGui::SliderInt("Use Hi-Z Tracing", &rendererSettings.SSR.UseHizTracing, 0, 1);
			ImGui::SliderInt("Use Cone Tracing", &rendererSettings.SSR.UseConeTracing, 0, 1);
			ImGui::SliderInt("RayStep Count", &rendererSettings.SSR.RayStepCount, 5, 10);
			ImGui::DragFloat("RayStep Size", &rendererSettings.SSR.RayStepSize, 0.001f, 0.04f, 0.1f);
		}
		if (ImGui::CollapsingHeader("Ambient Occlusion"))
		{
			ImGui::SliderInt("Enable", &rendererSettings.AmbientOcclusion.Enabled, 0, 1);
			ImGui::Separator();
			ImGui::Text("AO Mode (0 - HBAO; 1 - GTAO)");
			ImGui::SliderInt(" ", &rendererSettings.AmbientOcclusion.AOMode, 0, 1);

			ImGui::Text("AO Texture");
			uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
			Application::Get().GetImGuiLayer()->RenderTexture(m_Data->DenoiserImage[currentFrameIndex], 300, 200);
		}
		if (ImGui::CollapsingHeader("Bloom"))
		{
			ImGui::SliderInt("Enable", &rendererSettings.Bloom.Enabled, 0, 1);
			ImGui::Separator();
			ImGui::DragFloat("Threshold", &rendererSettings.Bloom.Threshold, 0.1f, 0.0f, 1000.0f);
			ImGui::SliderFloat("Knee", &rendererSettings.Bloom.Knee, 0.1f, 1.0f);
		}
		//if (ImGui::CollapsingHeader("Sky"))
		//{
		//	AtmosphereParams& atmosParams = Renderer::GetSceneEnvironment()->GetAtmoshpereParams();
		//	//ImGui::DragFloat3("Rayleigh Scattering", &atmosParams.RayleighScattering.x, 0.01f, 0.0f, 100.0f);
		//	//ImGui::DragFloat4("Rayleigh Abscoprtion", &atmosParams.RayleighAbsorption.x, 0.01f, 0.0f, 100.0f);
		//	//ImGui::DragFloat3("Mie Scattering", &atmosParams.MieScattering.x, 0.01f, 0.0f, 100.0f);
		//	//ImGui::DragFloat3("Ozone", &atmosParams.OzoneAbsorption.x, 0.01f, 0.0f, 100.0f);
		//}
	}

	void VulkanPostFXPass::CalculateMipLevels(uint32_t width, uint32_t height)
	{
		m_Data->ScreenMipLevel = (uint32_t)std::floor(std::log2(std::max(width, height))) + 1;
	}

	void VulkanPostFXPass::UpdateRenderingSettings()
	{
		RendererSettings& rendererSettings = Renderer::GetRendererSettings();
		m_CompositeSetings.UseAO = rendererSettings.AmbientOcclusion.Enabled;
		m_CompositeSetings.UseBloom = rendererSettings.Bloom.Enabled;
		m_CompositeSetings.UseSSR = rendererSettings.SSR.Enabled;
	}

	void VulkanPostFXPass::ShutDown()
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			vkDestroySampler(device, m_Data->HZBNearestSampler[i], nullptr);
			vkDestroySampler(device, m_Data->HZBMinReductionSampler[i], nullptr);
			vkDestroySampler(device, m_Data->HZBMaxReductionSampler[i], nullptr);
		}

		delete m_Data;
	}
}