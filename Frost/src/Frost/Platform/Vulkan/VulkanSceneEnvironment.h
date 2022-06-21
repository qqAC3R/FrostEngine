#pragma once

#include "Frost/Renderer/SceneEnvironment.h"
#include "Frost/Renderer/Shader.h"
#include "Frost/Renderer/PipelineCompute.h"
#include "Frost/Renderer/Material.h"
#include "Frost/Renderer/Renderer.h"

namespace Frost
{

	class VulkanSceneEnvironment : public SceneEnvironment
	{
	public:
		VulkanSceneEnvironment();
		virtual ~VulkanSceneEnvironment();

		virtual void InitCallbackFunctions() override;

		virtual void LoadEnvMap(const std::string& filepath) override;

		Ref<Texture2D> GetEnvironmentMap() { return m_EnvironmentMap; }
		Ref<TextureCubeMap> GetRadianceMap() { return m_RadianceMap; }
		Ref<TextureCubeMap> GetIrradianceMap() { return m_IrradianceMap; }
		Ref<TextureCubeMap> GetPrefilteredMap() { return m_PrefilteredMap; }

		virtual void SetType(SceneEnvironment::Type type) override;
		
		virtual glm::vec3 GetSunDirection() override { return m_SunDir; }
		virtual void SetSunDirection(glm::vec3 sunDir) override { m_SunDir = sunDir; }

		Ref<Image2D> GetTransmittanceLUT() const { return m_TransmittanceLUT; }
		Ref<Image2D> GetMultiScatterLUT() const { return m_MultiScatterLUT; }
		Ref<Image2D> GetSkyViewLUT() const { return m_SkyViewLUT; }
		Ref<TextureCubeMap> GetAtmoshpereIrradianceMap() const { return m_SkyIrradianceMap; }
		Ref<TextureCubeMap> GetAtmoshperePrefilterMap() const { return m_SkyPrefilterMap; }
		Ref<Texture3D> GetAerialPerspectiveLUT() const { return m_AerialLUT; }

		virtual void RenderSkyBox(const RenderQueue& renderQueue) override;
		virtual void UpdateAtmosphere(const RenderQueue& renderQueue) override;

		virtual void SetEnvironmentMapCallback(const std::function<void(const Ref<TextureCubeMap>&, const Ref<TextureCubeMap>&)>& func) override;
		virtual AtmosphereParams& GetAtmoshpereParams() override { return m_AtmosphereParams; }

		/* This should be called by the composite pass, because we are rendering on the PBR renderpass */
		void InitSkyBoxPipeline(Ref<RenderPass> renderPass);
	private:
		void HDRMaps_Init();
		void RadianceMapCompute();
		void PrefilteredMapCompute();
		void IrradianceMapCompute();

		void TransmittanceLUT_InitData();
		void TransmittanceLUT_Update();

		void MultiScatterLUT_InitData();
		void MultiScatterLUT_Update();

		void SkyViewLUT_InitData();
		void SkyViewLUT_Update();

		void SkyIrradiance_InitData();
		void SkyIrradiance_Update();

		void SkyPrefilter_InitData();
		void SkyPrefilter_Update();

		void AerialPerspective_InitData();
		void AerialPerspective_Update(const RenderQueue& renderQueue);
	private:
		Ref<Texture2D> m_EnvironmentMap;
		Ref<TextureCubeMap> m_RadianceMap;
		Ref<TextureCubeMap> m_IrradianceMap;
		Ref<TextureCubeMap> m_PrefilteredMap;

		std::string m_Filepath;
		SceneEnvironment::Type m_Type;
		std::vector<std::function<void(const Ref<TextureCubeMap>&, const Ref<TextureCubeMap>&)>> m_EnvMapChangeCallback;
		glm::vec3 m_SunDir;

		Ref<Shader> m_RadianceShader;
		Ref<Shader> m_IrradianceShader;
		Ref<Shader> m_PrefilteredShader;

		Ref<ComputePipeline> m_RadianceCompute;
		Ref<ComputePipeline> m_IrradianceCompute;
		Ref<ComputePipeline> m_PrefilteredCompute;

		Ref<Material> m_RadianceShaderDescriptor;
		Ref<Material> m_IrradianceShaderDescriptor;
		Ref<Material> m_PrefilteredShaderDescriptor;

		Ref<Shader> m_SkyboxShader;
		Ref<Pipeline> m_SkyboxPipeline;
		Ref<Material> m_SkyboxDescriptor;


		// Transmittance LUT
		Ref<Shader> m_TransmittanceShader;
		Ref<ComputePipeline> m_TransmittancePipeline;
		Ref<Material> m_TransmittanceDescriptor;
		Ref<Image2D> m_TransmittanceLUT;

		// Multi scattering LUT
		Ref<Shader> m_MultiScatterShader;
		Ref<ComputePipeline> m_MultiScatterPipeline;
		Ref<Material> m_MultiScatterDescriptor;
		Ref<Image2D> m_MultiScatterLUT;

		// Sky view LUT
		Ref<Shader> m_SkyViewShader;
		Ref<ComputePipeline> m_SkyViewPipeline;
		Ref<Material> m_SkyViewDescriptor;
		Ref<Image2D> m_SkyViewLUT;

		// Iradiance cubemap
		Ref<Shader> m_SkyIrradianceShader;
		Ref<ComputePipeline> m_SkyIrradiancePipeline;
		Ref<Material> m_SkyIrradianceDescriptor;
		Ref<TextureCubeMap> m_SkyIrradianceMap;

		// Prefiltered cubemap
		Ref<Shader> m_SkyPrefilterShader;
		Ref<ComputePipeline> m_SkyPrefilterPipeline;
		Vector<Ref<Material>> m_SkyPrefilterDescriptor;
		Ref<TextureCubeMap> m_SkyPrefilterMap;

		// Aerial Perspective
		Ref<Shader> m_AP_Shader;
		Ref<ComputePipeline> m_AP_Pipeline;
		Ref<Texture3D> m_AerialLUT;
		Ref<Material> m_AP_Descriptor;

		AtmosphereParams m_AtmosphereParams;
	};

}