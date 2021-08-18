#include "frostpch.h"
#include "VulkanRayTracingPass.h"

#include "Platform/Vulkan/VulkanContext.h"

namespace Frost
{

	VulkanRayTracingPass::VulkanRayTracingPass()
		: m_Name("RayTracingPass")
	{
	}

	static bool s_UpdateTLAS[3];
	void VulkanRayTracingPass::Init(SceneRenderPassPipeline* renderPassPipeline)
	{
		m_RenderPassPipeline = renderPassPipeline;

		m_Data.Shader = Shader::Create("assets/shader/path_tracer_demo.glsl");

		TextureSpecs imageSpec{};
		imageSpec.Width = 1600;
		imageSpec.Height = 900;
		imageSpec.Usage = { TextureSpecs::UsageSpec::Storage };
		imageSpec.Format = TextureSpecs::FormatSpec::RGBA16F;

		for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			m_Data.DisplayTexture[i] = Image2D::Create(imageSpec);


			m_Data.Descriptor[i] = Material::Create(m_Data.Shader, "RayTracingDescriptor");
			m_Data.Descriptor[i]->Set("image", m_Data.DisplayTexture[i]);

		}

		
		m_Data.SBT = ShaderBindingTable::Create(m_Data.Shader);


		RayTracingPipeline::CreateInfo createInfo{};
		createInfo.ShaderBindingTable = m_Data.SBT;
		createInfo.Shader = m_Data.Shader;
		createInfo.PushConstant.PushConstantRange = sizeof(CameraInfo);
		createInfo.PushConstant.PushConstantShaderStages = { ShaderType::RayGen, ShaderType::Miss, ShaderType::ClosestHit };
		m_Data.Pipeline = RayTracingPipeline::Create(createInfo);



		for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++)
			m_Data.TopLevelAS[i] = TopLevelAccelertionStructure::Create();



		for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++)
			s_UpdateTLAS[i] = true;

	}

	void VulkanRayTracingPass::OnUpdate(const RenderQueue& renderQueue, void* cmdBuf, uint32_t swapChainIndex)
	{

		Vector<std::pair<Ref<Mesh>, glm::mat4>> meshes;
		for (auto& mesh : renderQueue.m_Data)
			meshes.push_back(std::make_pair(mesh.Mesh, mesh.Transform));
		m_Data.TopLevelAS[swapChainIndex]->UpdateAccelerationStructure(meshes);


		m_CameraInfo.InverseProjection = glm::inverse(renderQueue.CameraProjectionMatrix);
		m_CameraInfo.InverseView = glm::inverse(renderQueue.CameraViewMatrix);

		// Inverting the y coordonate because glm was designed for OpenGL :/
		m_CameraInfo.InverseProjection[1][1] *= -1;



		// TODO: This should be made better
		if (s_UpdateTLAS[swapChainIndex])
		{
			m_Data.Descriptor[swapChainIndex]->Set("topLevelAS", m_Data.TopLevelAS[swapChainIndex]);
			m_Data.Descriptor[swapChainIndex]->UpdateVulkanDescriptor();
			s_UpdateTLAS[swapChainIndex] = false;
		}



		VkCommandBuffer vkCmdBuf = (VkCommandBuffer)cmdBuf;
		
		m_Data.Pipeline->Bind(vkCmdBuf);
		m_Data.Pipeline->BindVulkanPushConstant(vkCmdBuf, &m_CameraInfo);

		m_Data.Descriptor[swapChainIndex]->Bind(cmdBuf, m_Data.Pipeline->GetVulkanPipelineLayout(), GraphicsType::Raytracing);

		auto strideAddresses = m_Data.SBT->GetVulkanShaderAddresses();

		vkCmdTraceRaysKHR(vkCmdBuf,
						  &strideAddresses[0],
						  &strideAddresses[1],
						  &strideAddresses[2],
						  &strideAddresses[3],
						  1600, 900, 1);



	}

	void VulkanRayTracingPass::OnResize(uint32_t width, uint32_t height)
	{
		TextureSpecs imageSpec{};
		imageSpec.Width = width;
		imageSpec.Height = height;
		imageSpec.Usage = { TextureSpecs::UsageSpec::Storage };
		imageSpec.Format = TextureSpecs::FormatSpec::RGBA16F;


		for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			m_Data.DisplayTexture[i]->Destroy();
			m_Data.DisplayTexture[i] = Image2D::Create(imageSpec);
			m_Data.Descriptor[i]->Set("image", m_Data.DisplayTexture[i]);
		}
	}

	void VulkanRayTracingPass::ShutDown()
	{
		for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			m_Data.DisplayTexture[i]->Destroy();
			m_Data.TopLevelAS[i]->Destroy();
		}

		m_Data.SBT->Destroy();
		m_Data.Shader->Destroy();

		m_Data.Pipeline->Destroy();
	}

}