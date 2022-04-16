#pragma once

#include "Frost/Renderer/Texture.h"

namespace Frost
{

	class SceneEnvironment
	{
	public:
		virtual ~SceneEnvironment() {}

		virtual Ref<Texture2D> GetEnvironmentMap() = 0;
		virtual Ref<TextureCubeMap> GetRadianceMap() = 0;
		virtual Ref<TextureCubeMap> GetIrradianceMap() = 0;
		virtual Ref<TextureCubeMap> GetPrefilteredMap() = 0;

		//virtual void RenderSkyBox() = 0;

		virtual void Load(const std::string& filepath) = 0;

		static Ref<SceneEnvironment> Create();
	};

}