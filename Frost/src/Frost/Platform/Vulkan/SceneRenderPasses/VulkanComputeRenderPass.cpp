#include "frostpch.h"
#include "VulkanComputeRenderPass.h"

#include "Frost/Platform/Vulkan/VulkanContext.h"
#include "Frost/Platform/Vulkan/VulkanPipelineCompute.h"
#include "Frost/Platform/Vulkan/VulkanTexture.h"

#include "imgui.h"

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

		{
			TextureSpecs imageSpec{};
			imageSpec.Width = 512;
			imageSpec.Height = 512;
			imageSpec.Usage = { TextureSpecs::UsageSpec::Storage };
			imageSpec.Format = TextureSpecs::FormatSpec::RGBA16F;
			m_Data.CubeMap = TextureCubeMap::Create(imageSpec);
		}
		m_Data.UniformBuffer = UniformBuffer::Create(sizeof(m_TurbidityAzimuthInclination));

		m_TurbidityAzimuthInclination = glm::vec3(2.32f, 5.14f, 1.53f);

		m_Data.Descriptor = Material::Create(m_Data.Shader, "ComputeDescriptor");
		m_Data.Descriptor->Set("u_CubeMap", m_Data.CubeMap);
		m_Data.Descriptor->Set("Uniforms", m_Data.UniformBuffer);
		m_Data.Descriptor->UpdateVulkanDescriptor();



		ComputePipeline::CreateInfo computePipelineCreateInfo{};
		computePipelineCreateInfo.Shader = m_Data.Shader;
		m_Data.ComputePipeline = ComputePipeline::Create(computePipelineCreateInfo);
	}

	void VulkanComputeRenderPass::OnUpdate(const RenderQueue& renderQueue)
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		Ref<VulkanComputePipeline> computePipeline = m_Data.ComputePipeline.As<VulkanComputePipeline>();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		m_Data.UniformBuffer->SetData(&m_TurbidityAzimuthInclination);
		m_Data.ComputePipeline->Bind();
		m_Data.Descriptor->Bind(computePipeline->GetVulkanPipelineLayout(), GraphicsType::Compute);
		m_Data.ComputePipeline->Dispatch(512.0f / 32.0f, 512.0f / 32.0f, 6);

		m_Data.CubeMap.As<VulkanTextureCubeMap>()->TransitionLayout(cmdBuf, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

		ImGui::Begin("Sky Settings");
		ImGui::DragFloat("Turbidity", &m_TurbidityAzimuthInclination.x, 0.05f);
		ImGui::DragFloat("Azimuth", &m_TurbidityAzimuthInclination.y, 0.05f);
		ImGui::DragFloat("Inclination", &m_TurbidityAzimuthInclination.z, 0.05f);
		ImGui::End();
	}

	void VulkanComputeRenderPass::OnResize(uint32_t width, uint32_t height)
	{
	}

	void VulkanComputeRenderPass::ShutDown()
	{
		m_Data.CubeMap->Destroy();
		m_Data.UniformBuffer->Destroy();

		m_Data.Descriptor->Destroy();
		m_Data.ComputePipeline->Destroy();
		m_Data.Shader->Destroy();
	}

}