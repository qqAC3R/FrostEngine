#include "frostpch.h"
#include "Entity.h"

namespace Frost
{
	Entity::Entity(const entt::entity& handle, Scene* scene)
		: m_Handle(handle), m_Scene(scene)
	{
	}
}