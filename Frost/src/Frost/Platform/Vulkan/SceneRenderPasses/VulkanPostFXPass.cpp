#include "frostpch.h"
#include "VulkanPostFXPass.h"

#include "Frost/Renderer/Renderer.h"
#include "Frost/Platform/Vulkan/VulkanRenderer.h"
#include "Frost/Platform/Vulkan/VulkanContext.h"
#include "Frost/Platform/Vulkan/VulkanMaterial.h"
#include "Frost/Platform/Vulkan/VulkanImage.h"
#include "Frost/Platform/Vulkan/VulkanPipelineCompute.h"
#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanGeometryPass.h"
#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanCompositePass.h"

namespace Frost
{
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


		BlurColorBuffer_InitData(1600, 900);
		SSR_InitData(1600, 900);
	}

	void VulkanPostFXPass::BlurColorBuffer_InitData(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;
		uint32_t mipLevels = (uint32_t)std::floor(std::log2(std::max(width, height))) + 1;
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		m_Data->ScreenMipLevel = mipLevels;

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
			imageSpec.Format = ImageFormat::RGBA16F;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.Sampler.SamplerFilter = ImageFilter::Linear;
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
					Ref<Image2D> colorBuffer = m_RenderPassPipeline->GetRenderPassData<VulkanCompositePass>()->RenderPass->GetColorAttachment(0, j);
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

		m_Data->FinalImage.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			ImageSpecification imageSpec{};
			imageSpec.Format = ImageFormat::RGBA16F;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.Sampler.SamplerFilter = ImageFilter::Nearest;
			imageSpec.Width = width;
			imageSpec.Height = height;
			m_Data->FinalImage[i] = Image2D::Create(imageSpec);
		}

		

		m_Data->SSRDescriptor.resize(framesInFlight);
		for(uint32_t i = 0; i < framesInFlight; i++)
		{
			if(!m_Data->SSRDescriptor[i])
				m_Data->SSRDescriptor[i] = Material::Create(m_Data->SSRShader, "Post-ProceesingFX");

			auto& vulkanMaterial = m_Data->SSRDescriptor[i].As<VulkanMaterial>();

			auto& viewPosTexture = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->RenderPass->GetColorAttachment(3, i);
			auto& depthTexture = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->RenderPass->GetDepthAttachment(i);
			auto& normalTexture = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->RenderPass->GetColorAttachment(1, i);
			auto& finalImage = m_RenderPassPipeline->GetRenderPassData<VulkanCompositePass>()->RenderPass->GetColorAttachment(0, i);

			// Textures
			vulkanMaterial->Set("u_ColorFrameTex", finalImage);
			vulkanMaterial->Set("u_ViewPosTex", viewPosTexture);
			vulkanMaterial->Set("u_DepthTex", depthTexture);
			vulkanMaterial->Set("u_NormalTex", normalTexture);
			vulkanMaterial->Set("o_FrameTex", m_Data->FinalImage[i]);
			vulkanMaterial->Set("u_PrefilteredColorBuffer", m_Data->BlurredColorBuffer[i]);

			// Uniform buffer data
			vulkanMaterial->Set("UniformBuffer.ScreenSize", glm::vec4(width, height, 0.0f, 0.0f));
			
			vulkanMaterial->Set("UniformBuffer.SampleCount", 4.0f);
			vulkanMaterial->Set("UniformBuffer.RayStep", 0.1f);
			vulkanMaterial->Set("UniformBuffer.IterationCount", 100.0f);
			vulkanMaterial->Set("UniformBuffer.DistanceBias", 0.03f);

			vulkanMaterial->Set("UniformBuffer.DebugDraw", 0.0f);
			vulkanMaterial->Set("UniformBuffer.IsBinarySearchEnabled", 1.0f);
			vulkanMaterial->Set("UniformBuffer.IsAdaptiveStepEnabled", 1.0f);
			vulkanMaterial->Set("UniformBuffer.IsExponentialStepEnabled", 0.0f);

			vulkanMaterial->UpdateVulkanDescriptorIfNeeded();
		}
	}

	void VulkanPostFXPass::OnUpdate(const RenderQueue& renderQueue)
	{
		BlurColorBuffer_Update(renderQueue);
		SSR_Update(renderQueue);
	}

	void VulkanPostFXPass::BlurColorBuffer_Update(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		auto vulkan_BlurPipeline = m_Data->BlurPipeline.As<VulkanComputePipeline>();
		auto vulkan_BlurColorTexture = m_Data->BlurredColorBuffer[currentFrameIndex].As<VulkanImage2D>();

		glm::vec2 currentRes = glm::vec2(renderQueue.ViewPortWidth, renderQueue.ViewPortHeight);
		for (uint32_t mipLevel = 0; mipLevel < m_Data->ScreenMipLevel; mipLevel++)
		{
			auto vulkan_BlurDescriptor = m_Data->BlurShaderDescriptor[currentFrameIndex][mipLevel].As<VulkanMaterial>();
			
			if (mipLevel > 0)
			{
				currentRes.x /= 2;
				currentRes.y /= 2;
			}

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

		uint32_t groupX = (renderQueue.ViewPortWidth  / 32.0f) + (renderQueue.ViewPortWidth  % 32);
		uint32_t groupY = (renderQueue.ViewPortHeight / 32.0f) + (renderQueue.ViewPortHeight % 32);
		ssrPipeline->Dispatch(cmdBuf, groupX, groupY, 1);
	}

	void VulkanPostFXPass::OnResize(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;

		BlurColorBuffer_InitData(width, height);
		SSR_InitData(width, height);
#if 0
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			ImageSpecification imageSpec{};
			imageSpec.Format = ImageFormat::RGBA16F;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.Width = width;
			imageSpec.Height = height;
			m_Data->FinalImage[i] = Image2D::Create(imageSpec);
		}

		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			auto& vulkanMaterial = m_Data->SSRDescriptor[i].As<VulkanMaterial>();

			auto& viewPosTexture = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->RenderPass->GetColorAttachment(3, i);
			auto& depthTexture = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->RenderPass->GetDepthAttachment(i);
			auto& normalTexture = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->RenderPass->GetColorAttachment(1, i);
			auto& finalImage = m_RenderPassPipeline->GetRenderPassData<VulkanCompositePass>()->RenderPass->GetColorAttachment(0, i);

			// Textures
			vulkanMaterial->Set("u_ColorFrameTex", finalImage);
			vulkanMaterial->Set("u_ViewPosTex", viewPosTexture);
			vulkanMaterial->Set("u_DepthTex", depthTexture);
			vulkanMaterial->Set("u_NormalTex", normalTexture);
			vulkanMaterial->Set("o_FrameTex", m_Data->FinalImage[i]);

			// Uniform buffer data
			vulkanMaterial->Set("UniformBuffer.ScreenSize", glm::vec4(width, height, 0.0f, 0.0f));

			vulkanMaterial->UpdateVulkanDescriptorIfNeeded();
		}
#endif
	}

	void VulkanPostFXPass::ShutDown()
	{
		delete m_Data;
	}
}