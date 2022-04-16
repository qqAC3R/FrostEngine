#pragma once

#include "Frost/Renderer/SceneRenderPass.h"

namespace Frost
{
	class VulkanDynamicSkyPass : public SceneRenderPass
	{
	public:
		VulkanDynamicSkyPass();
		virtual ~VulkanDynamicSkyPass();

		virtual void Init(SceneRenderPassPipeline* renderPassPipeline) override;
		virtual void InitLate() override;
		virtual void OnUpdate(const RenderQueue& renderQueue);
		virtual void OnResize(uint32_t width, uint32_t height) override;
		virtual void OnResizeLate(uint32_t width, uint32_t height) override;
		virtual void ShutDown() override;

		virtual void* GetInternalData() override { return (void*)m_Data; }

		virtual const std::string& GetName() override { return m_Name; }
	private:
		void TransmittanceLUT_InitData(uint32_t width, uint32_t height);
		void TransmittanceLUT_Update(const RenderQueue& renderQueue);

		void MultiScatterLUT_InitData(uint32_t width, uint32_t height);
		void MultiScatterLUT_Update(const RenderQueue& renderQueue);

		void SkyViewLUT_InitData(uint32_t width, uint32_t height);
		void SkyViewLUT_Update(const RenderQueue& renderQueue);

		void SkyIrradiance_InitData(uint32_t width, uint32_t height);
		void SkyIrradiance_Update(const RenderQueue& renderQueue);

		void SkyPrefilter_InitData(uint32_t width, uint32_t height);
		void SkyPrefilter_Update(const RenderQueue& renderQueue);

		void AerialPerspective_InitData(uint32_t width, uint32_t height);
		void AerialPerspective_Update(const RenderQueue& renderQueue);

	public:
#define SUN_POS glm::vec3(0.45f, -0.5f, -0.65f)

		struct HillaireParams
		{
			glm::vec4 RayleighScattering = glm::vec4(5.802f, 13.558f, 33.1f, 8.0f);
			glm::vec4 RayleighAbsorption = glm::vec4(0.0f, 0.0f, 0.0f, 8.0f);
			glm::vec4 MieScattering = glm::vec4(3.996f, 3.996f, 3.996f, 1.2f);
			glm::vec4 MieAbsorption = glm::vec4(4.4f, 4.4f, 4.4f, 1.2f);
			glm::vec4 OzoneAbsorption = glm::vec4(0.650f, 1.881f, 0.085f, 0.002f);
			glm::vec4 PlanetAbledo_Radius = glm::vec4(0.0f, 0.0f, 0.0f, 6.360f); // Planet albedo (x, y, z) and radius.
			glm::vec4 SunDir_AtmRadius = glm::vec4(SUN_POS, 6.460f);              // Sun direction (x, y, z) and atmosphere radius (w).
			glm::vec4 ViewPos = glm::vec4(0.0f, 6.360f + 0.0002f, 0.0f, 0.0f);   // View position (x, y, z). w is unused.
		};

		struct SkyDiffuseParams
		{
			glm::vec4 SunDirection = glm::vec4(SUN_POS, 0.0f); // sun direction (x, y, z)

			float SunIntensity = 1.0f; //
			float SunSize = 2.0f; //
			float GroundRadius = 6.360f;
			float AtmosphereRadius = 6.460f; //

			glm::vec4 ViewPos_SkyIntensity = glm::vec4(0.0f, 6.360f + 0.0002f, 0.0f, 3.0f);

			float Roughness = 0.0f;
			int NrSamples = 64;
			float Unused1 = 0.0f;
			float Unused2 = 0.0f;

		};
	private:
		SceneRenderPassPipeline* m_RenderPassPipeline;

		struct InternalData
		{
			// Transmittance LUT
			Ref<Shader> TransmittanceShader;
			Ref<ComputePipeline> TransmittancePipeline;
			Ref<Material> TransmittanceDescriptor;
			Ref<Image2D> TransmittanceLUT;

			// Multi scattering LUT
			Ref<Shader> MultiScatterShader;
			Ref<ComputePipeline> MultiScatterPipeline;
			Ref<Material> MultiScatterDescriptor;
			Ref<Image2D> MultiScatterLUT;

			// Sky view LUT
			Ref<Shader> SkyViewShader;
			Ref<ComputePipeline> SkyViewPipeline;
			Ref<Material> SkyViewDescriptor;
			Ref<Image2D> SkyViewLUT;

			// Iradiance cubemap
			Ref<Shader> SkyIrradianceShader;
			Ref<ComputePipeline> SkyIrradiancePipeline;
			Ref<Material> SkyIrradianceDescriptor;
			Ref<TextureCubeMap> SkyIrradianceMap;

			// Prefiltered cubemap
			Ref<Shader> SkyPrefilterShader;
			Ref<ComputePipeline> SkyPrefilterPipeline;
			Vector<Ref<Material>> SkyPrefilterDescriptor;
			Ref<TextureCubeMap> SkyPrefilterMap;

			// Aerial Perspective
			Ref<Shader> AP_Shader;
			Ref<ComputePipeline> AP_Pipeline;
			Vector<Ref<Material>> AP_Descriptor;
			Vector<Ref<Texture3D>> AerialLUT;

			SkyDiffuseParams m_SkyDiffuseParams;
			HillaireParams m_SkyParams;
		};

		friend class SceneRenderPassPipeline;
		InternalData* m_Data;
		std::string m_Name;
	};

}