#pragma once

#include "Frost/EntitySystem/Entity.h"

#include "PhysicsSettings.h"
#include "PhysicsActor.h"

namespace Frost
{
	struct RaycastHit
	{
		uint64_t HitEntity;
		glm::vec3 Position;
		glm::vec3 Normal;
		float Distance;
	};

	class PhysicsScene
	{
	public:
		virtual ~PhysicsScene() {}

		virtual void Simulate(float ts, bool callFixedUpdate = true) = 0;

		virtual Ref<PhysicsActor> GetActor(Entity entity) = 0;

		virtual Ref<PhysicsActor> CreateActor(Entity entity) = 0;
		virtual void RemoveActor(Ref<PhysicsActor> actor) = 0;

		virtual bool Raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, RaycastHit* outHit) = 0;

		virtual glm::vec3 GetGravity() const = 0;
		virtual void SetGravity(const glm::vec3& gravity) = 0;

		virtual bool IsValid() const = 0;

		static Ref<PhysicsScene> Create(const PhysicsSettings& settings);
	};

}