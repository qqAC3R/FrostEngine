#include "frostpch.h"
#include "PhysicsActor.h"

#include "PhysX/PhysXActor.h"

namespace Frost
{
	Ref<PhysicsActor> PhysicsActor::Create(Entity entity)
	{
		// TODO: API Choice?
		return Ref<PhysXActor>::Create(entity);
	}
}