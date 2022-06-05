#pragma once

// Frost libraries
#include "Frost/Core/UUID.h"
#include "Frost/Renderer/Mesh.h"

// Math
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Frost
{
	struct IDComponent
	{
		IDComponent() = default;
		IDComponent(const UUID& id)
			: ID(id)
		{
		}

		UUID ID;
	};

	struct TagComponent
	{
		TagComponent() = default;
		TagComponent(const std::string& tag)
			: Tag(tag)
		{
		}

		std::string Tag;
	};

	struct TransformComponent
	{
		TransformComponent() = default;
		TransformComponent(const glm::vec3& translation, const glm::vec3& rotation, const glm::vec3& scale)
			: Translation(translation), Rotation(rotation), Scale(scale)
		{
		}

		glm::mat4 GetTransform()
		{
			glm::mat4 rotation =  glm::toMat4(glm::quat(glm::radians(Rotation)));
			glm::mat4 transform = glm::translate(glm::mat4(1.0f), Translation) * rotation * glm::scale(glm::mat4(1.0f), Scale);
			return transform;
		}

		glm::vec3 Translation = glm::vec3(0.0f, 0.0f, 0.0f);
		glm::vec3 Rotation    = glm::vec3(0.0f, 0.0f, 0.0f);
		glm::vec3 Scale       = glm::vec3(1.0f, 1.0f, 1.0f);
	};

	struct MeshComponent
	{
		MeshComponent() = default;
		MeshComponent(const Ref<Mesh>& mesh)
			: Mesh(mesh)
		{
		}

		Ref<Mesh> Mesh;
	};

	struct PointLightComponent
	{
		PointLightComponent() = default;
		PointLightComponent(glm::vec3 color, float intensity, float radius, float falloff)
			: Color(color), Intensity(intensity), Radius(radius), Falloff(falloff)
		{
		}

		glm::vec3 Color = { 1.0f, 1.0f, 1.0f };
		float Intensity = 1.0f;
		float Radius = 3.0f;
		float Falloff = 0.0f;
	};

	struct DirectionalLightComponent
	{
		DirectionalLightComponent() = default;
		DirectionalLightComponent(glm::vec3 direction)
			: Direction(direction)
		{
		}

		glm::vec3 Direction = { 1.0f, 1.0f, 1.0f };
		glm::vec3 Color = { 1.0f, 1.0f, 1.0f };
		float Intensity = 10.0f;
		float Size = 2.0f;
	};
}