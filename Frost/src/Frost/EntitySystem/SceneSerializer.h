#pragma once

#include "Scene.h"
#include <json/nlohmann/json.hpp>

namespace Frost
{
	class SceneSerializer
	{
	public:
		SceneSerializer(const Ref<Scene>& scene);

		void Serialize(const std::string& filepath);
		//void SerializeRuntime(const std::string& filepath);

		bool Deserialize(const std::string& filepath);
		//bool DeserializeRuntime(const std::string& filepath);

		const std::string& GetSceneName() const { return m_SceneName; }

	private:
		void SerializeEntity(nlohmann::ordered_json& out, Entity entity);

	private:
		Ref<Scene> m_Scene;
		std::string m_SceneName;
	};


}