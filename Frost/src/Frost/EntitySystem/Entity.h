#pragma once

#include "Frost/EntitySystem/Components.h"
#include "Frost/EntitySystem/Scene.h"
#include <entt.hpp>

namespace Frost
{
	class Scene;

	class Entity
	{
	public:
		Entity() = default;
		Entity(const entt::entity& handle, Scene* scene);

		template <typename T, typename... Args>
		T& AddComponent(Args&&... args)
		{
			T& component = m_Scene->GetRegistry().emplace<T>(m_Handle, std::forward<Args>(args)...);
			return component;
		}

		template <typename T>
		void RemoveComponent()
		{
			m_Scene->GetRegistry().remove<T>(m_Handle);
		}

		template <typename T>
		T& GetComponent()
		{
			T& component = m_Scene->GetRegistry().get<T>(m_Handle);
			return component;
		}

		template <typename T>
		bool HasComponent()
		{
			return m_Scene->GetRegistry().any_of<T>(m_Handle);
		}

		UUID GetUUID() { return GetComponent<IDComponent>().ID; }
		entt::entity Raw() { return m_Handle; }

		operator bool() const { return m_Handle != entt::null; }
		operator entt::entity() const { return m_Handle; }
		bool operator==(const Entity& other) const { return m_Handle == other.m_Handle && m_Scene == other.m_Scene; }
		bool operator!=(const Entity& other) const { return !(*this == other); }

	private:
		entt::entity m_Handle = entt::null;
		Scene* m_Scene = nullptr;
	};


}