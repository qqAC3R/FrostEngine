#include "frostpch.h"
#include "VulkanComputeRenderPass.h"

#include "Frost/Platform/Vulkan/VulkanContext.h"
#include "Frost/Platform/Vulkan/VulkanPipelineCompute.h"

namespace Frost
{

	VulkanComputeRenderPass::VulkanComputeRenderPass()
		: m_Name("ComputePass")
	{
	}

	void VulkanComputeRenderPass::Init(SceneRenderPassPipeline* renderPassPipeline)
	{
		m_RenderPassPipeline = renderPassPipeline;

		m_Data.Shader = Shader::Create("assets/shader/compute.glsl");

		TextureSpecs imageSpec{};
		imageSpec.Width = 512;
		imageSpec.Height = 512;
		imageSpec.Usage = { TextureSpecs::UsageSpec::Storage };
		imageSpec.Format = TextureSpecs::FormatSpec::RGBA16F;

		m_Data.DisplayTexture = Image2D::Create(imageSpec);

		m_Data.Descriptor = Material::Create(m_Data.Shader, "ComputeDescriptor");
		m_Data.Descriptor->Set("u_Image", m_Data.DisplayTexture);
		m_Data.Descriptor->UpdateVulkanDescriptor();

		ComputePipeline::CreateInfo computePipelineCreateInfo{};
		computePipelineCreateInfo.Shader = m_Data.Shader;
		m_Data.ComputePipeline = ComputePipeline::Create(computePipelineCreateInfo);
	}

	void VulkanComputeRenderPass::OnUpdate(const RenderQueue& renderQueue)
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		Ref<VulkanComputePipeline> computePipeline = m_Data.ComputePipeline.As<VulkanComputePipeline>();

		m_Data.ComputePipeline->Bind();
		m_Data.Descriptor->Bind(computePipeline->GetVulkanPipelineLayout(), GraphicsType::Compute);
		m_Data.ComputePipeline->Dispatch(16, 16, 1);
	}

	void VulkanComputeRenderPass::OnResize(uint32_t width, uint32_t height)
	{
	}

	void VulkanComputeRenderPass::ShutDown()
	{
		m_Data.DisplayTexture->Destroy();
		m_Data.Descriptor->Destroy();
		m_Data.ComputePipeline->Destroy();
		m_Data.Shader->Destroy();
	}

}