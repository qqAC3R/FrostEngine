#include "frostpch.h"
#include "VulkanSceneEnvironment.h"

#include "Frost/Renderer/Renderer.h"
#include "Frost/Platform/Vulkan/VulkanPipelineCompute.h"
#include "Frost/Platform/Vulkan/VulkanMaterial.h"
#include "Frost/Platform/Vulkan/VulkanContext.h"
#include "Frost/Platform/Vulkan/VulkanTexture.h"

namespace Frost
{
	VulkanSceneEnvironment::VulkanSceneEnvironment()
	{
		InitShadersAndPipelines();
	}

	void VulkanSceneEnvironment::Load(const std::string& filepath)
	{
		// Creating the equirectangular map
		TextureSpecification textureSpec{};
		textureSpec.Usage = ImageUsage::Storage;
		m_EnvironmentMap = Texture2D::Create(filepath, textureSpec);

		// --------------------------------- RADIANCE MAP --------------------------------
		{
			uint32_t envCubeMapSize = Renderer::GetRendererConfig().EnvironmentMapResolution;

			// Creating the radiance cube map
			ImageSpecification imageSpec{};
			imageSpec.Format = ImageFormat::RGBA16F;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.Width = envCubeMapSize;
			imageSpec.Height = envCubeMapSize;
			imageSpec.Mips = UINT32_MAX;
			m_RadianceMap = TextureCubeMap::Create(imageSpec);

			// Recording a cmdbuf for the compute shader
			VkCommandBuffer cmdBuf = VulkanContext::GetCurrentDevice()->AllocateCommandBuffer(RenderQueueType::Compute, true);

			// Setting up the textures
			auto vulkanMaterial = m_RadianceShaderDescriptor.As<VulkanMaterial>();
			vulkanMaterial->Set("u_EquirectangularTex", m_EnvironmentMap);
			vulkanMaterial->Set("o_CubeMap", m_RadianceMap);

			vulkanMaterial->Bind(cmdBuf, m_RadianceCompute);

			// Disptaching the compute shader
			auto vulkanComputePipeline = m_RadianceCompute.As<VulkanComputePipeline>();
			vulkanComputePipeline->Dispatch(cmdBuf, envCubeMapSize / 32, envCubeMapSize / 32, 6);

			// Wait till the compute shader will finish
			auto vulkanRadianceMap = m_RadianceMap.As<VulkanTextureCubeMap>();
			vulkanRadianceMap->TransitionLayout(cmdBuf, vulkanRadianceMap->GetVulkanImageLayout(),
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT
			);

			// Generating the mip maps for the environment map (this is mostly for debugging)
			vulkanRadianceMap->GenerateMipMaps(cmdBuf, vulkanRadianceMap->GetVulkanImageLayout());

			// Flush the compute command buffer
			VulkanContext::GetCurrentDevice()->FlushCommandBuffer(cmdBuf, RenderQueueType::Compute);
		}

		// --------------------------------- IRRADIANCE MAP --------------------------------
		{
			uint32_t irradianceCubeMapSize = Renderer::GetRendererConfig().IrradianceMapResolution;
			uint32_t samples = Renderer::GetRendererConfig().IrradianceMapSamples;

			// Creating the irradiance cube map
			ImageSpecification imageSpec{};
			imageSpec.Format = ImageFormat::RGBA16F;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.Width = irradianceCubeMapSize;
			imageSpec.Height = irradianceCubeMapSize;
			imageSpec.Mips = UINT32_MAX;
			m_IrradianceMap = TextureCubeMap::Create(imageSpec);

			// Recording a cmdbuf for the compute shader
			VkCommandBuffer cmdBuf = VulkanContext::GetCurrentDevice()->AllocateCommandBuffer(RenderQueueType::Compute, true);

			// Setting up the textures
			auto vulkanMaterial = m_IrradianceShaderDescriptor.As<VulkanMaterial>();
			vulkanMaterial->Set("o_IrradianceMap", m_IrradianceMap);
			vulkanMaterial->Set("u_RadianceMap", m_RadianceMap);

			vulkanMaterial->Bind(cmdBuf, m_IrradianceCompute);

			// Disptaching the compute shader
			auto vulkanComputePipeline = m_IrradianceCompute.As<VulkanComputePipeline>();
			vulkanComputePipeline->BindVulkanPushConstant(cmdBuf, "u_Uniforms", &samples);
			vulkanComputePipeline->Dispatch(cmdBuf, irradianceCubeMapSize / 32, irradianceCubeMapSize / 32, 6);

			// Wait till the compute shader will finish
			auto vulkanIrradianceMap = m_IrradianceMap.As<VulkanTextureCubeMap>();
			vulkanIrradianceMap->TransitionLayout(cmdBuf, vulkanIrradianceMap->GetVulkanImageLayout(),
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT
			);

			// Generating the mip maps for the environment map (this is mostly for debugging)
			vulkanIrradianceMap->GenerateMipMaps(cmdBuf, vulkanIrradianceMap->GetVulkanImageLayout());


			// Flush the compute command buffer
			VulkanContext::GetCurrentDevice()->FlushCommandBuffer(cmdBuf, RenderQueueType::Compute);
		}

		// --------------------------------- PREFILTERED MAP --------------------------------
		{
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
			uint32_t prefilteredCubeMapSize = Renderer::GetRendererConfig().EnvironmentMapResolution;

			// Creating the prefiltered cube map
			ImageSpecification imageSpec{};
			imageSpec.Format = ImageFormat::RGBA16F;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.Width = prefilteredCubeMapSize;
			imageSpec.Height = prefilteredCubeMapSize;
			imageSpec.Mips = UINT32_MAX;
			m_PrefilteredMap = TextureCubeMap::Create(imageSpec);

			const float deltaRoughness = 1.0f / glm::max((float)m_PrefilteredMap->GetSpecification().Mips - 1.0f, 1.0f);

			// Setting up the textures
			auto vulkanMaterial = m_PrefilteredShaderDescriptor.As<VulkanMaterial>();
			vulkanMaterial->Set("u_RadianceMap", m_RadianceMap);

			// Getting the vulkan compute pipeline
			auto vulkanComputePipeline = m_PrefilteredCompute.As<VulkanComputePipeline>();

			// Get the shader location of the prefiltered map
			VulkanMaterial::ShaderLocation shaderLocation = vulkanMaterial->GetShaderLocationFromString("o_PrefilteredMap");
			VkDescriptorSet descriptorSet = vulkanMaterial->GetVulkanDescriptorSet(0);
			auto prefilteredMap = m_PrefilteredMap.As<VulkanTextureCubeMap>();

			for (uint32_t i = 0; i < m_PrefilteredMap->GetSpecification().Mips; i++)
			{
				// Recording a cmdbuf for the compute shader
				VkCommandBuffer cmdBuf = VulkanContext::GetCurrentDevice()->AllocateCommandBuffer(RenderQueueType::Compute, true);

				// Setting up the texture (for every mip)
				VkDescriptorImageInfo imageDescriptorInfo{};
				imageDescriptorInfo.imageLayout = prefilteredMap->GetVulkanImageLayout();
				imageDescriptorInfo.imageView = prefilteredMap->GetVulkanImageViewMip(i);
				imageDescriptorInfo.sampler = nullptr;

				// Writting and updating the descriptor with the texture mip
				VkWriteDescriptorSet writeDescriptorSet{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				writeDescriptorSet.dstBinding = shaderLocation.Binding;
				writeDescriptorSet.dstArrayElement = 0;
				writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				writeDescriptorSet.pImageInfo = &imageDescriptorInfo;
				writeDescriptorSet.descriptorCount = 1;
				writeDescriptorSet.dstSet = descriptorSet;
				vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);

				vulkanMaterial->Bind(cmdBuf, m_IrradianceCompute);

				// Setting up the push constant
				float roughness = i * deltaRoughness;
				roughness = glm::max(roughness, 0.05f);
				vulkanComputePipeline->BindVulkanPushConstant(cmdBuf, "u_Uniforms", &roughness);

				// Dispatching the compute shader
				uint32_t numGroups = glm::max(1u, prefilteredCubeMapSize / 32);
				vulkanComputePipeline->Dispatch(cmdBuf, numGroups, numGroups, 6);

				prefilteredMap->TransitionLayout(cmdBuf, prefilteredMap->GetVulkanImageLayout(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

				// Flush the compute command buffer
				VulkanContext::GetCurrentDevice()->FlushCommandBuffer(cmdBuf, RenderQueueType::Compute);

				prefilteredCubeMapSize /= 2;
			}

		}
	}

	VulkanSceneEnvironment::~VulkanSceneEnvironment()
	{
	}

	void VulkanSceneEnvironment::InitShadersAndPipelines()
	{
		m_RadianceShader = Renderer::GetShaderLibrary()->Get("EquirectangularToCubeMap");
		m_IrradianceShader = Renderer::GetShaderLibrary()->Get("EnvironmentIrradiance");
		m_PrefilteredMap = Renderer::GetShaderLibrary()->Get("EnvironmentMipFilter");

		m_RadianceCompute = ComputePipeline::Create({ m_RadianceShader });
		m_IrradianceCompute = ComputePipeline::Create({ m_IrradianceShader });
		m_PrefilteredCompute = ComputePipeline::Create({ m_PrefilteredMap });


		m_RadianceShaderDescriptor = Material::Create(m_RadianceShader, "EquirectangularToCubeMap");
		m_IrradianceShaderDescriptor = Material::Create(m_IrradianceShader, "EnvironmentIrradiance");
		m_PrefilteredShaderDescriptor = Material::Create(m_PrefilteredMap, "EnvironmentMipFilter");
	}

}