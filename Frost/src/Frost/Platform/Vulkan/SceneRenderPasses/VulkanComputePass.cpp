#include "frostpch.h"
#include "VulkanComputePass.h"

#include "Frost/Platform/Vulkan/VulkanContext.h"
#include "Frost/Platform/Vulkan/VulkanPipelineCompute.h"
#include "Frost/Platform/Vulkan/VulkanTexture.h"
#include "Frost/Platform/Vulkan/VulkanMaterial.h"

#include <imgui.h>

namespace Frost
{

	VulkanComputePass::VulkanComputePass()
		: m_Name("ComputePass")
	{
	}

	void VulkanComputePass::Init(SceneRenderPassPipeline* renderPassPipeline)
	{
		m_RenderPassPipeline = renderPassPipeline;
		m_Data = new InternalData();

		m_Data->Shader = Renderer::GetShaderLibrary()->Get("PreethamSky");

		{
			ImageSpecification imageSpec{};
			imageSpec.Width = 512;
			imageSpec.Height = 512;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.Format = ImageFormat::RGBA16F;
			m_Data->CubeMap = TextureCubeMap::Create(imageSpec);
		}
		m_Data->UniformBuffer = UniformBuffer::Create(sizeof(m_TurbidityAzimuthInclination));

		m_TurbidityAzimuthInclination = glm::vec3(2.32f, 5.14f, 1.53f);

		m_Data->Descriptor = Material::Create(m_Data->Shader, "ComputeDescriptor");
		m_Data->Descriptor->Set("u_CubeMap", m_Data->CubeMap);
		m_Data->Descriptor->Set("Uniforms", m_Data->UniformBuffer);
		m_Data->Descriptor.As<VulkanMaterial>()->UpdateVulkanDescriptorIfNeeded();



		ComputePipeline::CreateInfo computePipelineCreateInfo{};
		computePipelineCreateInfo.Shader = m_Data->Shader;
		m_Data->ComputePipeline = ComputePipeline::Create(computePipelineCreateInfo);
	}

	void VulkanComputePass::OnUpdate(const RenderQueue& renderQueue)
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		Ref<VulkanComputePipeline> vulkanComputePipeline = m_Data->ComputePipeline.As<VulkanComputePipeline>();
		Ref<VulkanMaterial> vulkanMaterial = m_Data->Descriptor.As<VulkanMaterial>();

		// Allocate the commandbuffer for the compute pipeline
		VkCommandBuffer cmdBuf = VulkanContext::GetCurrentDevice()->AllocateCommandBuffer(RenderQueueType::Compute, true);

		m_Data->UniformBuffer->SetData(&m_TurbidityAzimuthInclination);
		vulkanMaterial->Bind(cmdBuf, m_Data->ComputePipeline);
		vulkanComputePipeline->Dispatch(cmdBuf, 512.0f / 32.0f, 512.0f / 32.0f, 6);

		//m_Data->CubeMap.As<VulkanTextureCubeMap>()->TransitionLayout(cmdBuf, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

		// Flush the compute commandbuffer
		VulkanContext::GetCurrentDevice()->FlushCommandBuffer(cmdBuf, RenderQueueType::Compute);

		ImGui::Begin("Sky Settings");
		ImGui::DragFloat("Turbidity", &m_TurbidityAzimuthInclination.x, 0.05f);
		ImGui::DragFloat("Azimuth", &m_TurbidityAzimuthInclination.y, 0.05f);
		ImGui::DragFloat("Inclination", &m_TurbidityAzimuthInclination.z, 0.05f);
		ImGui::End();
	}

	void VulkanComputePass::OnResize(uint32_t width, uint32_t height)
	{
	}

	void VulkanComputePass::ShutDown()
	{
		delete m_Data;
	}

}