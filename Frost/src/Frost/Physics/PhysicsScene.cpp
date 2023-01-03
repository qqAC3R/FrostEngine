#include "frostpch.h"
#include "PhysicsScene.h"

#include "PhysX/PhysXScene.h"

namespace Frost
{
	Ref<PhysicsScene> PhysicsScene::Create(const PhysicsSettings& settings)
	{
		// TODO: API Choice?
		return Ref<PhysXScene>::Create(settings);
	}

}