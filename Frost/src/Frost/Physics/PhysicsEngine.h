	#pragma once

#include "Frost/EntitySystem/Scene.h"
#include "PhysicsScene.h"

#include <glm/glm.hpp>

namespace Frost
{

	class PhysicsEngine
	{
	public:
		enum class API
		{
			None = 0, PhysX = 1
		};
		static PhysicsEngine::API GetAPI() { return s_API; }
	public:
		static void Initialize();
		static void ShutDown();

		static void CreateScene();
		static void DeleteScene();
		
		static Ref<PhysicsScene> GetScene() { return s_Scene; }

		static void CreateActors(Scene* scene);
		static void Simulate(Timestep ts);

		static PhysicsSettings& GetSettings() { return s_Settings; }

		static void EnableDebugRecording(bool enable) { m_EnableDebugRecording = enable; }
	private:
		static PhysicsSettings s_Settings;
		static Ref<PhysicsScene> s_Scene;
		static PhysicsEngine::API s_API;
		static bool m_EnableDebugRecording;

		friend class PhysXScene;
	};

}