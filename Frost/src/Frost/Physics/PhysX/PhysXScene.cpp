#include "frostpch.h"
#include "PhysXScene.h"

#include "PhysXContactListener.h"
#include "PhysXInternal.h"
#include "PhysXActor.h"
#include "PhysXUtils.h"
#include "PhysXDebugger.h"
#include "Frost/Physics/PhysicsEngine.h"

namespace Frost
{
	static PhysXContactListener s_ContactListener;

	PhysXScene::PhysXScene(const PhysicsSettings& settings)
	{
		physx::PxSceneDesc sceneDesc(PhysXInternal::GetPhysXHandle().getTolerancesScale());
		sceneDesc.flags |= physx::PxSceneFlag::eENABLE_CCD | physx::PxSceneFlag::eENABLE_PCM;
		sceneDesc.flags |= physx::PxSceneFlag::eENABLE_ENHANCED_DETERMINISM;

#if 0
		sceneDesc.flags |= physx::PxSceneFlag::eENABLE_ACTIVE_ACTORS; // TODO: | physx::PxSceneFlag::eEXCLUDE_KINEMATICS_FROM_ACTIVE_ACTORS
#endif

		sceneDesc.gravity = PhysXUtils::ToPhysXVector(settings.Gravity);
		sceneDesc.broadPhaseType = PhysXUtils::GetBroadphaseType(settings.BroadphaseAlgorithm);
		sceneDesc.cpuDispatcher = PhysXInternal::GetCPUDispatcher();
		sceneDesc.filterShader = (physx::PxSimulationFilterShader)PhysXInternal::FilterShader;
		sceneDesc.simulationEventCallback = &s_ContactListener;
		sceneDesc.frictionType = PhysXUtils::GetPhysXFrictionType(settings.FrictionModel);

		FROST_ASSERT_INTERNAL(sceneDesc.isValid());

		m_PhysXScene = PhysXInternal::GetPhysXHandle().createScene(sceneDesc);
		FROST_ASSERT_INTERNAL(m_PhysXScene);

		// Create Regions?


		if(PhysicsEngine::m_EnableDebugRecording)
			PhysXDebugger::StartDebugging("PhysXScene", true);
	}

	PhysXScene::~PhysXScene()
	{
		Destroy();
		//m_PhysXScene->release();
	}

	void PhysXScene::Simulate(float ts, bool callFixedUpdate)
	{
		if (callFixedUpdate)
		{
			for (auto& actor : m_Actors)
				actor->OnFixedUpdate(m_SubStepSize);
		}

		Advance(ts);

		for (auto& actor : m_Actors)
			actor->SynchronizeTransform();
	}

	Ref<PhysicsActor> PhysXScene::GetActor(Entity entity)
	{
		for (auto& actor : m_Actors)
		{
			if (actor->GetEntity() == entity)
				return actor;
		}

		return nullptr;
	}

	Ref<PhysicsActor> PhysXScene::CreateActor(Entity entity)
	{
		Ref<PhysicsActor> actor = PhysicsActor::Create(entity);

		m_Actors.push_back(actor);
		m_PhysXScene->addActor(*actor.As<PhysXActor>()->m_RigidActor);

		return actor;
	}

	void PhysXScene::RemoveActor(Ref<PhysicsActor> actor)
	{
		if (!actor)
			return;

		Ref<PhysXActor> physxActor = actor.As<PhysXActor>();

		for (auto& collider : physxActor->m_Colliders)
		{
			collider->DetachFromActor(actor);
			//collider->Release();
		}

		m_PhysXScene->removeActor(*physxActor->m_RigidActor);
		//physxActor->m_RigidActor->release();
		//physxActor->m_RigidActor = nullptr;

		for (auto it = m_Actors.begin(); it != m_Actors.end(); it++)
		{
			if ((*it)->GetEntity() == actor->GetEntity())
			{
				m_Actors.erase(it);
				break;
			}
		}
	}

	void PhysXScene::SetGravity(const glm::vec3& gravity)
	{
		m_PhysXScene->setGravity(PhysXUtils::ToPhysXVector(gravity));
	}

	void PhysXScene::Destroy()
	{
		FROST_ASSERT_INTERNAL(m_PhysXScene);

		for (auto& actor : m_Actors)
			RemoveActor(actor);

		m_Actors.clear();
		m_PhysXScene->release();
		m_PhysXScene = nullptr;

		if (PhysicsEngine::m_EnableDebugRecording)
			PhysXDebugger::StopDebugging();
	}

	void PhysXScene::Advance(float ts)
	{
		m_PhysXScene->simulate(ts);
		m_PhysXScene->fetchResults(true);
	}

}