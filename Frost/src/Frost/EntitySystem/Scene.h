#pragma once

#include "Frost/Core/UUID.h"
#include "Frost/Renderer/EditorCamera.h"
#include <entt.hpp>

namespace Frost
{
	class Entity;

	class Scene
	{
	public:
		Scene() = default;
		virtual ~Scene();

		void Update();

		Entity CreateEntity(const std::string name = "New Entity");
		Entity CreateEntityWithID(const UUID& id, const std::string name = "New Entity");
		void DestroyEntity(Entity entity);

		Entity FindEntityByUUID(UUID id);

		entt::registry& GetRegistry() { return m_Registry; }
		const entt::registry& GetRegistry() const { return m_Registry; }

	private:

	private:
		entt::registry m_Registry;
	};
}