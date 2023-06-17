#pragma once

#include "Frost/Renderer/Camera.h"

namespace Frost
{
	class RuntimeCamera : public Camera
	{
	public:
		RuntimeCamera() = default;
		RuntimeCamera(float fov, float aspectRatio, float nearClip, float farClip);

		const glm::mat4& GetViewMatrix() const override { return m_ViewMatrix; }
		const glm::mat4& GetViewProjection() const override { return m_ViewProjecitonMatrix; }
		const glm::mat4& GetViewProjectionVK() const override { return m_ViewProjecitonMatrix_Vk; }
		const glm::vec3& GetPosition() const override { return m_Position; }

		void SetTransform(const glm::mat4& transform);
		//void SetViewportSize(uint32_t width, uint32_t height);
		//uint32_t GetViewportWidth() const { return m_ViewportWidth; }
		//uint32_t GetViewportHeight() const { return m_ViewportHeight; }
	private:
		void UpdateViewMatrix(const glm::vec3& pos, const glm::vec3& rotation);
	private:
		glm::mat4 m_ViewMatrix;
		glm::vec3 m_Position;

		glm::mat4 m_ViewProjecitonMatrix;
		glm::mat4 m_ViewProjecitonMatrix_Vk;

		friend class RenderQueue;
	};
}