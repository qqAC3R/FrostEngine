#include "frostpch.h"
#include "VulkanSceneEnvironment.h"

#include "Frost/Renderer/Renderer.h"
#include "Frost/Platform/Vulkan/VulkanPipelineCompute.h"
#include "Frost/Platform/Vulkan/VulkanMaterial.h"
#include "Frost/Platform/Vulkan/VulkanContext.h"
#include "Frost/Platform/Vulkan/VulkanTexture.h"
#include "Frost/Platform/Vulkan/VulkanImage.h"
#include "Frost/Platform/Vulkan/VulkanPipeline.h"

namespace Frost
{
	VulkanSceneEnvironment::VulkanSceneEnvironment()
		: m_Type(SceneEnvironment::Type::Hillaire)
	{
		HDRMaps_Init();

		TransmittanceLUT_InitData();
		MultiScatterLUT_InitData();
		SkyViewLUT_InitData();

		SkyIrradiance_InitData();
		SkyPrefilter_InitData();

		//AerialPerspective_InitData();
	}

	void VulkanSceneEnvironment::InitCallbackFunctions()
	{
		m_SkyboxDescriptor->Set("CameraData.SkyMode", (float)m_Type);
		for (auto& func : m_EnvMapChangeCallback)
		{
			func(m_SkyPrefilterMap, m_SkyIrradianceMap);
		}
	}

	void VulkanSceneEnvironment::SetDynamicSky()
	{
		if (m_Type == SceneEnvironment::Type::Hillaire) return;

		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		vkDeviceWaitIdle(device);

		m_Type = SceneEnvironment::Type::Hillaire;
		m_RadianceMap = nullptr;
		m_PrefilteredMap = nullptr;
		m_IrradianceMap = nullptr;

		for (auto& func : m_EnvMapChangeCallback)
			func(m_SkyPrefilterMap, m_SkyIrradianceMap);

		m_SkyboxDescriptor->Set("CameraData.SkyMode", (float)SceneEnvironment::Type::Hillaire);
		m_SkyboxDescriptor->Set("u_EnvTexture", m_SkyPrefilterMap);
		m_SkyboxDescriptor.As<VulkanMaterial>()->UpdateVulkanDescriptorIfNeeded();
	}

	void VulkanSceneEnvironment::SetHDREnvironmentMap(const Ref<TextureCubeMap>& radianceMap, const Ref<TextureCubeMap>& prefilteredMap, const Ref<TextureCubeMap>& irradianceMap)
	{
		if (radianceMap && prefilteredMap && irradianceMap)
		{
			m_RadianceMap = radianceMap;
			m_PrefilteredMap = prefilteredMap;
			m_IrradianceMap = irradianceMap;

			
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
			vkDeviceWaitIdle(device);

			m_Type = SceneEnvironment::Type::HDRMap;
			
			for (auto& func : m_EnvMapChangeCallback)
				func(prefilteredMap, irradianceMap);

			m_SkyboxDescriptor->Set("CameraData.SkyMode", (float)SceneEnvironment::Type::HDRMap);
			m_SkyboxDescriptor->Set("u_EnvTexture", prefilteredMap);
			m_SkyboxDescriptor.As<VulkanMaterial>()->UpdateVulkanDescriptorIfNeeded();
		}
		else
		{
			FROST_CORE_WARN("HDR Map which is being set is null!");
		}
	}

	bool VulkanSceneEnvironment::ComputeEnvironmentMap(const std::string& filepath, Ref<TextureCubeMap>& radianceMap, Ref<TextureCubeMap>& prefilteredMap, Ref<TextureCubeMap>& irradianceMap)
	{
		// Creating the equirectangular map
		TextureSpecification textureSpec{};
		textureSpec.Usage = ImageUsage::ReadOnly;
		textureSpec.FlipTexture = false;

		Ref<Texture2D> environmentMap = Texture2D::Create(filepath, textureSpec);
		if (!environmentMap->Loaded())
		{
			FROST_CORE_ERROR("Couldn't load environment map '{0}' !", filepath);
			return false;
		}

		RadianceMapCompute(radianceMap, environmentMap);
		IrradianceMapCompute(irradianceMap, radianceMap);
		PrefilteredMapCompute(prefilteredMap, radianceMap);

		return environmentMap && radianceMap && prefilteredMap && irradianceMap;
	}

	void VulkanSceneEnvironment::InitSkyBoxPipeline(Ref<RenderPass> renderPass)
	{
		// Skybox pipeline/descriptors
		BufferLayout bufferLayout = {};
		Pipeline::CreateInfo pipelineCreateInfo{};
		pipelineCreateInfo.Shader = m_SkyboxShader;
		pipelineCreateInfo.RenderPass = renderPass;
		pipelineCreateInfo.VertexBufferLayout = bufferLayout;
		pipelineCreateInfo.UseDepthTest = true;
		pipelineCreateInfo.UseDepthWrite = true;
		pipelineCreateInfo.Topology = PrimitiveTopology::TriangleStrip;
		pipelineCreateInfo.DepthCompareOperation = DepthCompare::LessOrEqual;
		m_SkyboxPipeline = Pipeline::Create(pipelineCreateInfo);
		
		//auto envCubeMap = m_PrefilteredMap;
		auto skyViewLut = Renderer::GetSceneEnvironment().As<VulkanSceneEnvironment>()->GetSkyViewLUT();
		auto transmittanceLut = Renderer::GetSceneEnvironment().As<VulkanSceneEnvironment>()->GetTransmittanceLUT();
		auto multiScatterLut = Renderer::GetSceneEnvironment().As<VulkanSceneEnvironment>()->GetMultiScatterLUT();

		//m_SkyboxDescriptor->Set("u_EnvTexture", envCubeMap);
		m_SkyboxDescriptor->Set("u_EnvTexture", m_SkyIrradianceMap);
		m_SkyboxDescriptor->Set("u_HillaireLUT", skyViewLut);
		m_SkyboxDescriptor->Set("u_TransmittanceLUT", transmittanceLut);
		m_SkyboxDescriptor->Set("u_MultiScatterLUT", multiScatterLut);

		m_SkyboxDescriptor->Set("CameraData.Gamma", 2.2f);
		m_SkyboxDescriptor->Set("CameraData.Exposure", 0.1f);
		m_SkyboxDescriptor->Set("CameraData.Lod", 3.0f);
		m_SkyboxDescriptor->Set("CameraData.SkyMode", (float)m_Type);

		AtmosphereParams& atmosphereParams = m_AtmosphereParams;

		m_SkyboxDescriptor->Set("CameraData.SunDir", glm::vec3(atmosphereParams.SunDirection_Intensity));
		m_SkyboxDescriptor->Set("CameraData.SunIntensity", atmosphereParams.SunDirection_Intensity.w);
		m_SkyboxDescriptor->Set("CameraData.SunSize", atmosphereParams.ViewPos_SunSize.w);

		m_SkyboxDescriptor->Set("CameraData.ViewPos", glm::vec3(atmosphereParams.ViewPos_SunSize));
		m_SkyboxDescriptor->Set("CameraData.SkyIntensity", 3.0f);

		m_SkyboxDescriptor->Set("CameraData.GroundRadius", atmosphereParams.PlanetAbledo_Radius.w);
		m_SkyboxDescriptor->Set("CameraData.AtmosphereRadius", atmosphereParams.AtmosphereRadius);

		m_SkyboxDescriptor.As<VulkanMaterial>()->UpdateVulkanDescriptorIfNeeded();
	}

	void VulkanSceneEnvironment::HDRMaps_Init()
	{
		// Shaders
		m_RadianceShader = Renderer::GetShaderLibrary()->Get("EquirectangularToCubeMap");
		m_IrradianceShader = Renderer::GetShaderLibrary()->Get("EnvironmentIrradiance");
		m_PrefilteredMap = Renderer::GetShaderLibrary()->Get("EnvironmentMipFilter");

		m_TransmittanceShader = Renderer::GetShaderLibrary()->Get("Transmittance");
		m_MultiScatterShader = Renderer::GetShaderLibrary()->Get("MultiScatter");
		m_SkyViewShader = Renderer::GetShaderLibrary()->Get("SkyViewBuilder");
		m_SkyIrradianceShader = Renderer::GetShaderLibrary()->Get("SkyViewIrradiance");
		m_SkyPrefilterShader = Renderer::GetShaderLibrary()->Get("SkyViewFilter");
		m_AP_Shader = Renderer::GetShaderLibrary()->Get("AerialPerspective");


		// Pipelines
		m_RadianceCompute = ComputePipeline::Create({ m_RadianceShader });
		m_IrradianceCompute = ComputePipeline::Create({ m_IrradianceShader });
		m_PrefilteredCompute = ComputePipeline::Create({ m_PrefilteredMap });

		// Descriptors
		m_RadianceShaderDescriptor = Material::Create(m_RadianceShader, "EquirectangularToCubeMap");
		m_IrradianceShaderDescriptor = Material::Create(m_IrradianceShader, "EnvironmentIrradiance");
		m_PrefilteredShaderDescriptor = Material::Create(m_PrefilteredMap, "EnvironmentMipFilter");

		// Skybox renderer
		m_SkyboxShader = Renderer::GetShaderLibrary()->Get("RenderSkybox");
		m_SkyboxDescriptor = Material::Create(m_SkyboxShader);
	}

	void VulkanSceneEnvironment::RadianceMapCompute(Ref<TextureCubeMap>& radianceMap, Ref<Texture2D> environmentMap)
	{
		// --------------------------------- RADIANCE MAP --------------------------------
		uint32_t envCubeMapSize = Renderer::GetRendererConfig().EnvironmentMapResolution;

		// Creating the radiance cube map
		ImageSpecification imageSpec{};
		imageSpec.Format = ImageFormat::RGBA16F;
		imageSpec.Usage = ImageUsage::Storage;
		imageSpec.Width = envCubeMapSize;
		imageSpec.Height = envCubeMapSize;
		radianceMap = TextureCubeMap::Create(imageSpec);

		// Recording a cmdbuf for the compute shader
		VkCommandBuffer cmdBuf = VulkanContext::GetCurrentDevice()->AllocateCommandBuffer(RenderQueueType::Compute, true);

		// Setting up the textures
		auto vulkanMaterial = m_RadianceShaderDescriptor.As<VulkanMaterial>();
		vulkanMaterial->Set("u_EquirectangularTex", environmentMap);
		vulkanMaterial->Set("o_CubeMap", radianceMap);

		vulkanMaterial->Bind(cmdBuf, m_RadianceCompute);

		// Disptaching the compute shader
		auto vulkanComputePipeline = m_RadianceCompute.As<VulkanComputePipeline>();
		vulkanComputePipeline->Dispatch(cmdBuf, envCubeMapSize / 32, envCubeMapSize / 32, 6);

		// Wait till the compute shader will finish
		auto vulkanRadianceMap = radianceMap.As<VulkanTextureCubeMap>();
		vulkanRadianceMap->TransitionLayout(cmdBuf, vulkanRadianceMap->GetVulkanImageLayout(),
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT
		);

		// Generating the mip maps for the environment map (this is mostly for debugging)
		vulkanRadianceMap->GenerateMipMaps(cmdBuf, vulkanRadianceMap->GetVulkanImageLayout());

		// Flush the compute command buffer
		VulkanContext::GetCurrentDevice()->FlushCommandBuffer(cmdBuf, RenderQueueType::Compute);
	}

	void VulkanSceneEnvironment::PrefilteredMapCompute(Ref<TextureCubeMap>& prefilteredMap, Ref<TextureCubeMap> radianceMap)
	{
		// --------------------------------- PREFILTERED MAP --------------------------------
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		uint32_t prefilteredCubeMapSize = Renderer::GetRendererConfig().EnvironmentMapResolution;

		// Creating the prefiltered cube map
		ImageSpecification imageSpec{};
		imageSpec.Format = ImageFormat::RGBA16F;
		imageSpec.Usage = ImageUsage::Storage;
		imageSpec.Width = prefilteredCubeMapSize;
		imageSpec.Height = prefilteredCubeMapSize;
		prefilteredMap = TextureCubeMap::Create(imageSpec);

		const float deltaRoughness = 1.0f / glm::max((float)prefilteredMap->GetMipChainLevels() - 1.0f, 1.0f);

		// Setting up the textures
		auto vulkanMaterial = m_PrefilteredShaderDescriptor.As<VulkanMaterial>();
		vulkanMaterial->Set("u_RadianceMap", radianceMap);

		// Getting the vulkan compute pipeline
		auto vulkanComputePipeline = m_PrefilteredCompute.As<VulkanComputePipeline>();

		// Get the shader location of the prefiltered map
		VulkanMaterial::ShaderLocation shaderLocation = vulkanMaterial->GetShaderLocationFromString("o_PrefilteredMap");
		VkDescriptorSet descriptorSet = vulkanMaterial->GetVulkanDescriptorSet(0);
		auto vulkanPrefilteredMap = prefilteredMap.As<VulkanTextureCubeMap>();

		for (uint32_t i = 0; i < prefilteredMap->GetMipChainLevels(); i++)
		{
			// Recording a cmdbuf for the compute shader
			VkCommandBuffer cmdBuf = VulkanContext::GetCurrentDevice()->AllocateCommandBuffer(RenderQueueType::Compute, true);

			// Setting up the texture (for every mip)
			VkDescriptorImageInfo imageDescriptorInfo{};
			imageDescriptorInfo.imageLayout = vulkanPrefilteredMap->GetVulkanImageLayout();
			imageDescriptorInfo.imageView = vulkanPrefilteredMap->GetVulkanImageViewMip(i);
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

			vulkanPrefilteredMap->TransitionLayout(cmdBuf, vulkanPrefilteredMap->GetVulkanImageLayout(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

			// Flush the compute command buffer
			VulkanContext::GetCurrentDevice()->FlushCommandBuffer(cmdBuf, RenderQueueType::Compute);

			prefilteredCubeMapSize /= 2;
		}
	}

	void VulkanSceneEnvironment::IrradianceMapCompute(Ref<TextureCubeMap>& irradianceMap, Ref<TextureCubeMap> radianceMap)
	{
		// --------------------------------- IRRADIANCE MAP --------------------------------
		uint32_t irradianceCubeMapSize = Renderer::GetRendererConfig().IrradianceMapResolution;
		uint32_t samples = Renderer::GetRendererConfig().IrradianceMapSamples;

		// Creating the irradiance cube map
		ImageSpecification imageSpec{};
		imageSpec.Format = ImageFormat::RGBA16F;
		imageSpec.Usage = ImageUsage::Storage;
		imageSpec.Width = irradianceCubeMapSize;
		imageSpec.Height = irradianceCubeMapSize;
		irradianceMap = TextureCubeMap::Create(imageSpec);

		// Recording a cmdbuf for the compute shader
		VkCommandBuffer cmdBuf = VulkanContext::GetCurrentDevice()->AllocateCommandBuffer(RenderQueueType::Compute, true);

		// Setting up the textures
		auto vulkanMaterial = m_IrradianceShaderDescriptor.As<VulkanMaterial>();
		vulkanMaterial->Set("o_IrradianceMap", irradianceMap);
		vulkanMaterial->Set("u_RadianceMap", radianceMap);

		vulkanMaterial->Bind(cmdBuf, m_IrradianceCompute);

		// Disptaching the compute shader
		auto vulkanComputePipeline = m_IrradianceCompute.As<VulkanComputePipeline>();
		vulkanComputePipeline->BindVulkanPushConstant(cmdBuf, "u_Uniforms", &samples);
		vulkanComputePipeline->Dispatch(cmdBuf, irradianceCubeMapSize / 32, irradianceCubeMapSize / 32, 6);

		// Wait till the compute shader will finish
		auto vulkanIrradianceMap = irradianceMap.As<VulkanTextureCubeMap>();
		vulkanIrradianceMap->TransitionLayout(cmdBuf, vulkanIrradianceMap->GetVulkanImageLayout(),
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT
		);

		// Generating the mip maps for the environment map (this is mostly for debugging)
		vulkanIrradianceMap->GenerateMipMaps(cmdBuf, vulkanIrradianceMap->GetVulkanImageLayout());


		// Flush the compute command buffer
		VulkanContext::GetCurrentDevice()->FlushCommandBuffer(cmdBuf, RenderQueueType::Compute);
	}

	void VulkanSceneEnvironment::TransmittanceLUT_InitData()
	{
		// Transmittance pipeline creation
		ComputePipeline::CreateInfo computePipelineCreateInfo{};
		computePipelineCreateInfo.Shader = m_TransmittanceShader;
		m_TransmittancePipeline = ComputePipeline::Create(computePipelineCreateInfo);

		// Transmittance LUT
		ImageSpecification imageSpec{};
		imageSpec.Width = 256;
		imageSpec.Height = 64;
		imageSpec.Sampler.SamplerFilter = ImageFilter::Linear;
		imageSpec.Sampler.SamplerWrap = ImageWrap::ClampToEdge;
		imageSpec.Format = ImageFormat::RGBA16F;
		imageSpec.Usage = ImageUsage::Storage;

		m_TransmittanceLUT = Image2D::Create(imageSpec);

		// Descriptor data
		m_TransmittanceDescriptor = Material::Create(m_TransmittanceShader, "TransmittanceShader");
		m_TransmittanceDescriptor->Set("u_TransmittanceLUT", m_TransmittanceLUT);
	}

	void VulkanSceneEnvironment::MultiScatterLUT_InitData()
	{
		// MultiScatter pipeline creation
		ComputePipeline::CreateInfo computePipelineCreateInfo{};
		computePipelineCreateInfo.Shader = m_MultiScatterShader;
		m_MultiScatterPipeline = ComputePipeline::Create(computePipelineCreateInfo);

		// Multi Scatter LUT
		ImageSpecification imageSpec{};
		imageSpec.Width = 32;
		imageSpec.Height = 32;
		imageSpec.Sampler.SamplerFilter = ImageFilter::Linear;
		imageSpec.Sampler.SamplerWrap = ImageWrap::ClampToEdge;
		imageSpec.Format = ImageFormat::RGBA16F;
		imageSpec.Usage = ImageUsage::Storage;

		m_MultiScatterLUT = Image2D::Create(imageSpec);

		// Descriptor data
		m_MultiScatterDescriptor = Material::Create(m_MultiScatterShader, "MultiScatterShader");
		m_MultiScatterDescriptor->Set("u_TransmittanceLUT", m_TransmittanceLUT);
		m_MultiScatterDescriptor->Set("u_MultiScatterLUT", m_MultiScatterLUT);
	}

	void VulkanSceneEnvironment::SkyViewLUT_InitData()
	{
		// SkyView pipeline creation
		ComputePipeline::CreateInfo computePipelineCreateInfo{};
		computePipelineCreateInfo.Shader = m_SkyViewShader;
		m_SkyViewPipeline = ComputePipeline::Create(computePipelineCreateInfo);

		// SkyView LUT
		ImageSpecification imageSpec{};
		imageSpec.Width = 256;
		imageSpec.Height = 128;
		imageSpec.Sampler.SamplerFilter = ImageFilter::Linear;
		imageSpec.Sampler.SamplerWrap = ImageWrap::ClampToEdge;
		imageSpec.Format = ImageFormat::RGBA16F;
		imageSpec.Usage = ImageUsage::Storage;

		m_SkyViewLUT = Image2D::Create(imageSpec);


		// Descriptor data
		m_SkyViewDescriptor = Material::Create(m_SkyViewShader, "SkyViewBuilder_Shader");

		m_SkyViewDescriptor->Set("u_TransmittanceLUT", m_TransmittanceLUT);
		m_SkyViewDescriptor->Set("u_MultiScatterLUT", m_MultiScatterLUT);
		m_SkyViewDescriptor->Set("u_SkyViewImage", m_SkyViewLUT);
	}

	void VulkanSceneEnvironment::SkyIrradiance_InitData()
	{
		// SkyView Irradiance pipeline creation
		ComputePipeline::CreateInfo computePipelineCreateInfo{};
		computePipelineCreateInfo.Shader = m_SkyIrradianceShader;
		m_SkyIrradiancePipeline = ComputePipeline::Create(computePipelineCreateInfo);

		// SkyView Irradiance Cubemap
		ImageSpecification imageSpec{};
		imageSpec.Width = 32;
		imageSpec.Height = 32;
		imageSpec.Format = ImageFormat::RGBA16F;
		imageSpec.Usage = ImageUsage::Storage;
		imageSpec.UseMipChain = false;

		m_SkyIrradianceMap = TextureCubeMap::Create(imageSpec);


		// Descriptor data
		m_SkyIrradianceDescriptor = Material::Create(m_SkyIrradianceShader, "SkyViewIrradiance_Shader");

		m_SkyIrradianceDescriptor->Set("u_SkyViewLUT", m_SkyViewLUT);
		m_SkyIrradianceDescriptor->Set("u_IrradianceMap", m_SkyIrradianceMap);
	}

	void VulkanSceneEnvironment::SkyPrefilter_InitData()
	{
		// SkyView Prefilter pipeline creation
		ComputePipeline::CreateInfo computePipelineCreateInfo{};
		computePipelineCreateInfo.Shader = m_SkyPrefilterShader;
		m_SkyPrefilterPipeline = ComputePipeline::Create(computePipelineCreateInfo);

		// SkyView Prefiltered Cubemap
		ImageSpecification imageSpec{};
		imageSpec.Width = 64;
		imageSpec.Height = 64;
		imageSpec.Format = ImageFormat::RGBA16F;
		imageSpec.Usage = ImageUsage::Storage;

		m_SkyPrefilterMap = TextureCubeMap::Create(imageSpec);


		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		Ref<VulkanTextureCubeMap> vulkanPrefilteredMap = m_SkyPrefilterMap.As<VulkanTextureCubeMap>();
		// Descriptor data
		m_SkyPrefilterDescriptor.resize(7);
		for (uint32_t i = 0; i < 7; i++)
		{
			m_SkyPrefilterDescriptor[i] = Material::Create(m_SkyPrefilterShader, "SkyViewPrefilter_Shader");

			Ref<VulkanMaterial> vulkanMaterial = m_SkyPrefilterDescriptor[i].As<VulkanMaterial>();
			VkDescriptorSet descriptorSet = vulkanMaterial->GetVulkanDescriptorSet(0);

			vulkanMaterial->Set("u_SkyViewLUT", m_SkyViewLUT);

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

	void VulkanSceneEnvironment::AerialPerspective_InitData()
	{
#if 0
		// SkyView Prefilter pipeline creation
		ComputePipeline::CreateInfo computePipelineCreateInfo{};
		computePipelineCreateInfo.Shader = m_AP_Shader;
		m_AP_Pipeline = ComputePipeline::Create(computePipelineCreateInfo);

		ImageSpecification imageSpec{};
		imageSpec.Width = 32;
		imageSpec.Height = 32;
		imageSpec.Depth = 32;
		imageSpec.Format = ImageFormat::RGBA8;
		imageSpec.Usage = ImageUsage::Storage;
		imageSpec.UseMipChain = false;

		m_AerialLUT = Texture3D::Create(imageSpec);

		// Descriptor data
		m_AP_Descriptor = Material::Create(m_AP_Shader, "AerialPerspective");

		Ref<VulkanMaterial> vulkanMaterial = m_AP_Descriptor.As<VulkanMaterial>();

		vulkanMaterial->Set("u_AerialLUT", m_AerialLUT);
		vulkanMaterial->Set("u_TransmittanceLUT", m_TransmittanceLUT);
		vulkanMaterial->Set("u_MultiScatterLUT", m_MultiScatterLUT);
		vulkanMaterial->UpdateVulkanDescriptorIfNeeded();
#endif
	}

	void VulkanSceneEnvironment::SetEnvironmentMapCallback(const std::function<void(const Ref<TextureCubeMap>&, const Ref<TextureCubeMap>&)>& func)
	{
		m_EnvMapChangeCallback.push_back(func);
	}

	void VulkanSceneEnvironment::RenderSkyBox(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		// Getting the vulkan skybox pipline
		Ref<VulkanPipeline> vulkanSkyboxPipeline = m_SkyboxPipeline.As<VulkanPipeline>();
		vulkanSkyboxPipeline->Bind();
		
		AtmosphereParams& atmosphereParams = m_AtmosphereParams;

		
		m_SkyboxDescriptor->Set("CameraData.SunDir", glm::vec3(atmosphereParams.SunDirection_Intensity));
		m_SkyboxDescriptor->Set("CameraData.SunIntensity", atmosphereParams.SunDirection_Intensity.w);
		m_SkyboxDescriptor->Set("CameraData.SunSize", atmosphereParams.ViewPos_SunSize.w);

		m_SkyboxDescriptor->Set("CameraData.Exposure", renderQueue.m_Camera->GetExposure());
		m_SkyboxDescriptor->Set("CameraData.Lod", renderQueue.m_Camera->GetDOF());


		
		m_SkyboxDescriptor->Set("CameraData.RayleighScattering", atmosphereParams.RayleighScattering);
		m_SkyboxDescriptor->Set("CameraData.RayleighAbsorption", atmosphereParams.RayleighAbsorption);
		m_SkyboxDescriptor->Set("CameraData.MieScattering", atmosphereParams.MieScattering);
		m_SkyboxDescriptor->Set("CameraData.MieAbsorption", atmosphereParams.MieAbsorption);
		m_SkyboxDescriptor->Set("CameraData.OzoneAbsorption", atmosphereParams.OzoneAbsorption);
		m_SkyboxDescriptor->Set("CameraData.ViewPos", glm::vec3(atmosphereParams.ViewPos_SunSize));

		m_SkyboxDescriptor->Set("CameraData.GroundRadius", atmosphereParams.PlanetAbledo_Radius.w);
		m_SkyboxDescriptor->Set("CameraData.AtmosphereRadius", atmosphereParams.AtmosphereRadius);


		m_SkyboxDescriptor->Bind(m_SkyboxPipeline);
		Vector<glm::mat4> pushConstant(3);
		pushConstant[0] = renderQueue.m_Camera->GetProjectionMatrix();
		pushConstant[0][1][1] *= -1;
		pushConstant[1] = renderQueue.m_Camera->GetViewMatrix();
		pushConstant[2] = glm::inverse(renderQueue.m_Camera->GetViewProjectionVK());

		vulkanSkyboxPipeline->BindVulkanPushConstant("u_PushConstant", pushConstant.data());

		vkCmdDraw(cmdBuf, 4, 1, 0, 0);
	}



	void VulkanSceneEnvironment::UpdateAtmosphere(const RenderQueue& renderQueue)
	{
		AtmosphereParams& atmosphereParams = m_AtmosphereParams;

		glm::vec3 sunDir = glm::radians(-renderQueue.m_LightData.DirLight.Direction);

		glm::vec4 sunDir_Intensity = { sunDir.x, sunDir.y, sunDir.z, renderQueue.m_LightData.DirLight.Specification.Intensity };

//#define DYNAMIC_SKY
#ifdef DYNAMIC_SKY
		float viewPosX = (renderQueue.CameraPosition.x / 100000.0f);
		float viewPosY = (atmosphereParams.PlanetAbledo_Radius.w + 0.0002f) + (renderQueue.CameraPosition.y / 100000.0f);
		float viewPosZ = (renderQueue.CameraPosition.z / 100000.0f);
		
		atmosphereParams.ViewPos_SunSize.y = viewPosY;
		atmosphereParams.ViewPos_SunSize.x = viewPosX;
		atmosphereParams.ViewPos_SunSize.z = viewPosZ;
#else
		// If nothing changed since the last frame, don't compute it again since its useless
		if (atmosphereParams.SunDirection_Intensity == sunDir_Intensity)
		{
			return;
		}

		atmosphereParams.SunDirection_Intensity = sunDir_Intensity;
		float sunSize = renderQueue.m_LightData.DirLight.Specification.Size;
		atmosphereParams.ViewPos_SunSize.w = sunSize;
#endif

		if (glm::length(glm::vec3(atmosphereParams.ViewPos_SunSize)) < atmosphereParams.PlanetAbledo_Radius.w)
		{
			glm::vec3 pos = glm::vec3(atmosphereParams.ViewPos_SunSize) * ((atmosphereParams.PlanetAbledo_Radius.w + 0.0002f) / glm::length(glm::vec3(atmosphereParams.ViewPos_SunSize)));
			atmosphereParams.ViewPos_SunSize.x = pos.x;
			atmosphereParams.ViewPos_SunSize.y = pos.y;
			atmosphereParams.ViewPos_SunSize.z = pos.z;
		}

		TransmittanceLUT_Update();
		MultiScatterLUT_Update();
		SkyViewLUT_Update();

		SkyIrradiance_Update();
		SkyPrefilter_Update();
		//AerialPerspective_Update(renderQueue);
	}

	void VulkanSceneEnvironment::TransmittanceLUT_Update()
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		// Getting the vulkan transmittance pipeline
		Ref<VulkanComputePipeline> vulkanTransmittancePipeline = m_TransmittancePipeline.As<VulkanComputePipeline>();
		Ref<VulkanMaterial> vulkanTransmittanceMaterial = m_TransmittanceDescriptor.As<VulkanMaterial>();

		vulkanTransmittanceMaterial->Bind(cmdBuf, m_TransmittancePipeline);
		vulkanTransmittancePipeline->BindVulkanPushConstant(cmdBuf, "m_SkyParams", &m_AtmosphereParams);

		vulkanTransmittancePipeline->Dispatch(cmdBuf, 256 / 8, 64 / 8, 1);

		// Memory barrier
		Ref<VulkanImage2D> transmittanceLUT = m_TransmittanceLUT.As<VulkanImage2D>();
		transmittanceLUT->TransitionLayout(cmdBuf, transmittanceLUT->GetVulkanImageLayout(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	}

	void VulkanSceneEnvironment::MultiScatterLUT_Update()
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		// Getting the vulkan multi scatter pipeline
		Ref<VulkanComputePipeline> vulkanMultiScatterPipeline = m_MultiScatterPipeline.As<VulkanComputePipeline>();
		Ref<VulkanMaterial> vulkanMultiScatterMaterial = m_MultiScatterDescriptor.As<VulkanMaterial>();

		vulkanMultiScatterMaterial->Bind(cmdBuf, m_MultiScatterPipeline);
		vulkanMultiScatterPipeline->BindVulkanPushConstant(cmdBuf, "m_SkyParams", &m_AtmosphereParams);

		vulkanMultiScatterPipeline->Dispatch(cmdBuf, 32, 32, 1);

		// Memory barrier
		Ref<VulkanImage2D> multiScatterLUT = m_MultiScatterLUT.As<VulkanImage2D>();
		multiScatterLUT->TransitionLayout(cmdBuf, multiScatterLUT->GetVulkanImageLayout(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	}

	void VulkanSceneEnvironment::SkyViewLUT_Update()
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		// Getting the vulkan skyview_builder pipeline
		Ref<VulkanComputePipeline> vulkanSkyViewPipeline = m_SkyViewPipeline.As<VulkanComputePipeline>();
		Ref<VulkanMaterial> vulkanSkyViewMaterial = m_SkyViewDescriptor.As<VulkanMaterial>();

		vulkanSkyViewMaterial->Bind(cmdBuf, m_SkyViewPipeline);
		vulkanSkyViewPipeline->BindVulkanPushConstant(cmdBuf, "m_SkyParams", &m_AtmosphereParams);

		vulkanSkyViewPipeline->Dispatch(cmdBuf, 256 / 8, 128 / 8, 1);

		// Memory barrier
		Ref<VulkanImage2D> skyViewLUT = m_SkyViewLUT.As<VulkanImage2D>();
		skyViewLUT->TransitionLayout(cmdBuf, skyViewLUT->GetVulkanImageLayout(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

		skyViewLUT->GenerateMipMaps(cmdBuf, skyViewLUT->GetVulkanImageLayout());
	}

	void VulkanSceneEnvironment::SkyIrradiance_Update()
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		// Getting the vulkan skyview_builder pipeline
		Ref<VulkanComputePipeline> vulkanSkyIrradiancePipeline = m_SkyIrradiancePipeline.As<VulkanComputePipeline>();
		Ref<VulkanMaterial> vulkanSkyIrradianceMaterial = m_SkyIrradianceDescriptor.As<VulkanMaterial>();

		vulkanSkyIrradianceMaterial->Bind(cmdBuf, m_SkyIrradiancePipeline);
		vulkanSkyIrradiancePipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &m_AtmosphereParams);

		vulkanSkyIrradiancePipeline->Dispatch(cmdBuf, 32 / 8, 32 / 8, 6);

		Ref<VulkanImage2D> vulkanIrradianceMap = m_SkyIrradianceMap.As<VulkanImage2D>();
		vulkanIrradianceMap->TransitionLayout(cmdBuf, vulkanIrradianceMap->GetVulkanImageLayout(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
	}

	void VulkanSceneEnvironment::SkyPrefilter_Update()
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		// Getting the vulkan skyview_builder pipeline
		Ref<VulkanComputePipeline> vulkanSkyPrefilterPipeline = m_SkyPrefilterPipeline.As<VulkanComputePipeline>();
		Ref<VulkanTextureCubeMap> vulkanPrefilteredMap = m_SkyPrefilterMap.As<VulkanTextureCubeMap>();

		for (uint32_t i = 0; i < 7; i++)
		{
			Ref<VulkanMaterial> vulkanSkyPrefilterMaterial = m_SkyPrefilterDescriptor[i].As<VulkanMaterial>();

			// Compute the current mip levels.
			uint32_t mipWidth = vulkanPrefilteredMap->GetMipWidth(i);
			uint32_t mipHeight = vulkanPrefilteredMap->GetMipHeight(i);

			uint32_t groupX = static_cast<uint32_t>(glm::ceil(static_cast<float>(mipWidth) / 8.0f));
			uint32_t groupY = static_cast<uint32_t>(glm::ceil(static_cast<float>(mipHeight) / 8.0f));

			m_AtmosphereParams.Roughness = static_cast<float>(i) / 4.0f;
			m_AtmosphereParams.NrSamples = 64;

			vulkanSkyPrefilterMaterial->Bind(cmdBuf, m_SkyPrefilterPipeline);
			vulkanSkyPrefilterPipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &m_AtmosphereParams);

			vulkanSkyPrefilterPipeline->Dispatch(cmdBuf, groupX, groupY, 6);

			vulkanPrefilteredMap->TransitionLayout(cmdBuf, vulkanPrefilteredMap->GetVulkanImageLayout(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		}
	}

	void VulkanSceneEnvironment::AerialPerspective_Update(const RenderQueue& renderQueue)
	{
#if 0
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		// Getting the vulkan skyview_builder pipeline
		Ref<VulkanComputePipeline> vulkanAerialPipeline = m_AP_Pipeline.As<VulkanComputePipeline>();
		Ref<VulkanMaterial> vulkanAerialMaterial = m_AP_Descriptor.As<VulkanMaterial>();

		vulkanAerialMaterial->Bind(cmdBuf, m_AP_Pipeline);
		vulkanAerialPipeline->BindVulkanPushConstant(cmdBuf, "m_SkyParams", &m_AtmosphereParams);

		glm::mat4 invViewProjMatrix = glm::inverse(renderQueue.m_Camera->GetViewProjection());
		vulkanAerialMaterial->Set("CameraBlock.ViewMatrix", renderQueue.CameraViewMatrix);
		vulkanAerialMaterial->Set("CameraBlock.ProjMatrix", renderQueue.CameraProjectionMatrix);
		vulkanAerialMaterial->Set("CameraBlock.InvViewProjMatrix", invViewProjMatrix);
		vulkanAerialMaterial->Set("CameraBlock.CamPosition", glm::vec4(renderQueue.CameraPosition, 0.0f));
		vulkanAerialMaterial->Set("CameraBlock.NearFarPlane", glm::vec4(renderQueue.m_Camera->GetNearClip(), renderQueue.m_Camera->GetFarClip(), 0.0f, 0.0f));

		vulkanAerialPipeline->Dispatch(cmdBuf, 32 / 8, 32 / 8, 32 / 8);

		Ref<VulkanTexture3D> vulkanAerialLUT = m_AerialLUT.As<VulkanTexture3D>();
		vulkanAerialLUT->TransitionLayout(cmdBuf, vulkanAerialLUT->GetVulkanImageLayout(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
#endif
	}

	VulkanSceneEnvironment::~VulkanSceneEnvironment()
	{
	}
}