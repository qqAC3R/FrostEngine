#pragma once

#include "Frost/Renderer/SceneEnvironment.h"
#include "Frost/Renderer/Shader.h"
#include "Frost/Renderer/PipelineCompute.h"
#include "Frost/Renderer/Material.h"

namespace Frost
{

	class VulkanSceneEnvironment : public SceneEnvironment
	{
	public:
		VulkanSceneEnvironment();
		virtual ~VulkanSceneEnvironment();

		virtual void Load(const std::string& filepath) override;

		virtual Ref<Texture2D> GetEnvironmentMap() override { return m_EnvironmentMap; }
		virtual Ref<TextureCubeMap> GetRadianceMap() override { return m_RadianceMap; }
		virtual Ref<TextureCubeMap> GetIrradianceMap() override { return m_IrradianceMap; }
		virtual Ref<TextureCubeMap> GetPrefilteredMap() override { return m_PrefilteredMap; }
	private:
		void InitShadersAndPipelines();
	private:
		Ref<Texture2D> m_EnvironmentMap;
		Ref<TextureCubeMap> m_RadianceMap;
		Ref<TextureCubeMap> m_IrradianceMap;
		Ref<TextureCubeMap> m_PrefilteredMap;

		std::string m_Filepath;

		Ref<Shader> m_RadianceShader;
		Ref<Shader> m_IrradianceShader;
		Ref<Shader> m_PrefilteredShader;

		Ref<ComputePipeline> m_RadianceCompute;
		Ref<ComputePipeline> m_IrradianceCompute;
		Ref<ComputePipeline> m_PrefilteredCompute;

		Ref<Material> m_RadianceShaderDescriptor;
		Ref<Material> m_IrradianceShaderDescriptor;
		Ref<Material> m_PrefilteredShaderDescriptor;
	};

}