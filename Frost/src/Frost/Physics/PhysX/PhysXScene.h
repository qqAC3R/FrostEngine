#pragma once

#include "PhysXUtils.h"
#include "Frost/Physics/PhysicsScene.h"

namespace Frost
{

	class PhysXScene : public PhysicsScene
	{
	public:
		PhysXScene(const PhysicsSettings& settings);
		~PhysXScene();

		virtual void Simulate(float ts, bool callFixedUpdate = true) override;

		virtual Ref<PhysicsActor> GetActor(Entity entity) override;

		virtual Ref<PhysicsActor> CreateActor(Entity entity) override;
		virtual void RemoveActor(Ref<PhysicsActor> actor) override;

		virtual glm::vec3 GetGravity() const override { return PhysXUtils::FromPhysXVector(m_PhysXScene->getGravity()); }
		virtual void SetGravity(const glm::vec3& gravity) override;

		virtual bool IsValid() const override { return m_PhysXScene != nullptr; }

	private:
		void Destroy();

		void Advance(float ts);

	private:
		physx::PxScene* m_PhysXScene;

		Vector<Ref<PhysicsActor>> m_Actors;

		float m_SubStepSize;
		float m_Accumulator;
		uint32_t m_NumSubSteps = 0;
		const uint32_t c_MaxSubSteps = 8;
	};

}