#include "frostpch.h"
#include "PhysicsEngine.h"

#include "Frost/Physics/PhysX/PhysXInternal.h"

namespace Frost
{
	PhysicsSettings PhysicsEngine::s_Settings;
	Ref<PhysicsScene> PhysicsEngine::s_Scene;
	PhysicsEngine::API PhysicsEngine::s_API = PhysicsEngine::API::PhysX;
	bool PhysicsEngine::m_EnableDebugRecording = false;

	void PhysicsEngine::Initialize()
	{
		// TODO: API Choice?
		PhysXInternal::Initialize();
	}

	void PhysicsEngine::ShutDown()
	{
		PhysXInternal::Shutdown();
	}

	
	void PhysicsEngine::CreateScene()
	{
		s_Scene = PhysicsScene::Create(s_Settings);
	}

	void PhysicsEngine::DeleteScene()
	{
		s_Scene = nullptr;
	}

	void PhysicsEngine::CreateActors(Scene* scene)
	{
		auto view = scene->m_Registry.view<RigidBodyComponent>();

		for (auto& entity : view)
		{
			Entity e = { entity, scene };
			s_Scene->CreateActor(e);
		}
	}

	void PhysicsEngine::Simulate(Timestep ts)
	{
		s_Scene->Simulate(ts);
	}

}