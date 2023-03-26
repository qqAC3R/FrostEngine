#pragma once

#include "Frost/Renderer/Texture.h"
#include <glm/glm.hpp>

namespace Frost
{

	class RenderQueue;

	struct AtmosphereParams
	{
		glm::vec4 RayleighScattering = glm::vec4(5.802f, 13.558f, 33.1f, 8.0f);
		glm::vec4 RayleighAbsorption = glm::vec4(0.0f, 0.0f, 0.0f, 8.0f);
		glm::vec4 MieScattering = glm::vec4(3.996f, 3.996f, 3.996f, 1.2f);
		glm::vec4 MieAbsorption = glm::vec4(4.4f, 4.4f, 4.4f, 1.2f);

		glm::vec4 OzoneAbsorption = glm::vec4(0.650f, 1.881f, 0.085f, 0.002f);
		glm::vec4 PlanetAbledo_Radius = glm::vec4(0.0f, 0.0f, 0.0f, 6.360f);

		glm::vec4 SunDirection_Intensity = glm::vec4(0.45f, -0.5f, -0.65f, 2.0f);

		glm::vec4 ViewPos_SunSize = glm::vec4(0.0f, 6.360f + 0.0002f, 0.0f, 2.0f);

		float AtmosphereRadius = 6.562f;

		// Used for generating the irradiance map
		float Roughness = 0.0f;
		int NrSamples = 64;
	};

	class SceneEnvironment
	{
	public:
		enum class Type
		{
			Null = -1, HDRMap = 0, Hillaire = 1
		};

		virtual ~SceneEnvironment() {}

		virtual Ref<Texture2D> GetEnvironmentMap() = 0;
		virtual Ref<TextureCubeMap> GetRadianceMap() = 0;
		virtual Ref<TextureCubeMap> GetIrradianceMap() = 0;
		virtual Ref<TextureCubeMap> GetPrefilteredMap() = 0;

		virtual void InitCallbackFunctions() = 0;

		virtual void RenderSkyBox(const RenderQueue& renderQueue) = 0;
		virtual void UpdateAtmosphere(const RenderQueue& renderQueue) = 0;

		virtual void SetEnvironmentMapCallback(const std::function<void(const Ref<TextureCubeMap>& /*Prefiltered*/, const Ref<TextureCubeMap>& /*Irradiance*/)>& func) = 0;

		virtual void SetDynamicSky() = 0;
		virtual void SetHDREnvironmentMap(const Ref<TextureCubeMap>& radianceMap, const Ref<TextureCubeMap>& prefilteredMap, const Ref<TextureCubeMap>& irradianceMap) = 0;

		virtual bool ComputeEnvironmentMap(const std::string& filepath, Ref<TextureCubeMap>& radianceMap, Ref<TextureCubeMap>& prefilteredMap, Ref<TextureCubeMap>& irradianceMap) = 0;

		virtual glm::vec3 GetSunDirection() = 0;
		virtual void SetSunDirection(glm::vec3 sunDir) = 0;

		virtual	AtmosphereParams& GetAtmoshpereParams() = 0;

		static Ref<SceneEnvironment> Create();
	};

}