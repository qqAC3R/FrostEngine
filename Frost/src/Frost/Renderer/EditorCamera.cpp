#include "frostpch.h"
#include "EditorCamera.h"

#include "Frost/Core/Application.h"

#include "Frost/Core/Input.h"
#include "Frost/InputCodes/KeyCodes.h"
#include "Frost/InputCodes/MouseButtonCodes.h"

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/quaternion.hpp"
#include "glm/gtc/matrix_transform.hpp"



namespace Frost
{
	EditorCamera::EditorCamera(float fov, float aspectRatio, float nearClip, float farClip, CameraMode mode)
		: m_FOV(fov), m_AspectRatio(aspectRatio), m_NearClip(nearClip), m_FarClip(farClip),
		m_CameraMode(mode)
	{
		SetProjectionMatrix(glm::perspective(glm::radians(fov), aspectRatio, nearClip, farClip));
		if (m_CameraMode == CameraMode::FIRST_PERSON)
		{
			glm::vec3 front;
			front.x = cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
			front.y = sin(glm::radians(m_Pitch));
			front.z = sin(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
			m_CameraFront = glm::normalize(front);

			m_Position = glm::vec3(0.0f, 0.0f, 0.0f);
			//m_CameraFront = glm::vec3(glm::vec3(0.0f, 0.0f, -1.0f));
			m_CameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
		}
		UpdateView();
	}

	void EditorCamera::UpdateProjection()
	{
		m_AspectRatio = m_ViewportWidth / m_ViewportHeight;
		m_ProjectionMatrix = glm::perspective(glm::radians(m_FOV), m_AspectRatio, m_NearClip, m_FarClip);
		
	}

	void EditorCamera::UpdateView()
	{
		// m_Yaw = m_Pitch = 0.0f; // Lock the camera's rotation
		if (m_CameraMode == CameraMode::ORBIT)
		{
			m_Position = CalculatePosition();

			glm::quat orientation = GetOrientation();
			m_ViewMatrix = glm::translate(glm::mat4(1.0f), m_Position) * glm::toMat4(orientation);
			m_ViewMatrix = glm::inverse(m_ViewMatrix);
		}
		else if (m_CameraMode == CameraMode::FIRST_PERSON)
		{

			m_ViewMatrix = glm::lookAt(m_Position, m_Position + m_CameraFront, m_CameraUp);
		}
	}

	std::pair<float, float> EditorCamera::PanSpeed() const
	{
		float x = std::min(m_ViewportWidth / 1000.0f, 2.4f); // max = 2.4f
		float xFactor = 0.0366f * (x * x) - 0.1778f * x + 0.3021f;

		float y = std::min(m_ViewportHeight / 1000.0f, 2.4f); // max = 2.4f
		float yFactor = 0.0366f * (y * y) - 0.1778f * y + 0.3021f;

		return { xFactor, yFactor };
	}

	float EditorCamera::RotationSpeed() const
	{
		return 0.8f;
	}

	float EditorCamera::ZoomSpeed() const
	{
		float distance = m_Distance * 0.2f;
		distance = std::max(distance, 0.0f);
		float speed = distance * distance;
		speed = std::min(speed, 100.0f); // max speed = 100
		return speed;
	}

	void EditorCamera::OnUpdate(Timestep ts)
	{
		if (m_CameraMode == CameraMode::ORBIT)
		{
			if (!Application::Get().GetImGuiLayer()->IsUsed())
			{
				const glm::vec2& mouse{ Input::GetMouseX(), Input::GetMouseY() };
				glm::vec2 delta = (mouse - m_InitialMousePosition) * 0.005f;
				m_InitialMousePosition = mouse;

				if (Input::IsMouseButtonPressed(Mouse::Button1))
					MouseZoom(delta.y);
				else if (Input::IsMouseButtonPressed(Mouse::Button2))
					MousePan(delta);
				else if (Input::IsMouseButtonPressed(Mouse::Button0))
					MouseRotate(delta);
			}

		}
		else
		{
			if (Input::IsKeyPressed(Key::W))
				m_Position += 10.0f * m_CameraFront * ts.GetSeconds();
			else if (Input::IsKeyPressed(Key::S))
				m_Position -= 5.0f * m_CameraFront * ts.GetSeconds();
			if (Input::IsKeyPressed(Key::A))
				m_Position -= glm::normalize(glm::cross(m_CameraFront, m_CameraUp)) * 10.0f * ts.GetSeconds();
			else if (Input::IsKeyPressed(Key::D))
				m_Position += glm::normalize(glm::cross(m_CameraFront, m_CameraUp)) * 10.0f * ts.GetSeconds();
		}

		UpdateView();
	}

	void EditorCamera::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<MouseScrolledEvent>(FROST_BIND_EVENT_FN(EditorCamera::OnMouseScroll));
		dispatcher.Dispatch<MouseMovedEvent>(FROST_BIND_EVENT_FN(EditorCamera::OnMouseMoved));
		dispatcher.Dispatch<MouseMovedEvent>(FROST_BIND_EVENT_FN(EditorCamera::OnMouseMoved));

		dispatcher.Dispatch<MouseButtonPressedEvent>(FROST_BIND_EVENT_FN(EditorCamera::OnMouseButtonPressed));
		dispatcher.Dispatch<MouseButtonReleasedEvent>(FROST_BIND_EVENT_FN(EditorCamera::OnMouseButtonReleased));

	}

	bool EditorCamera::OnMouseScroll(MouseScrolledEvent& e)
	{
		if (m_CameraMode == CameraMode::ORBIT)
		{
			float delta = e.GetYOffset() * 0.1f;
			MouseZoom(delta);
			UpdateView();
		}
		return false;
	}

	bool EditorCamera::OnMouseButtonPressed(MouseButtonPressedEvent& event)
	{
		if (m_CameraMode == CameraMode::FIRST_PERSON)
		{
			if (event.GetMouseButton() == Mouse::Button2) Application::Get().GetWindow().DisableCursour();
		}
		return false;
	}

	bool EditorCamera::OnMouseButtonReleased(MouseButtonReleasedEvent& event)
	{
		if (m_CameraMode == CameraMode::FIRST_PERSON)
		{
			if (event.GetMouseButton() == Mouse::Button2) Application::Get().GetWindow().EnableCursour();
		}
		return false;
	}

	bool EditorCamera::OnMouseMoved(MouseMovedEvent& event)
	{
		if (Input::IsMouseButtonPressed(Mouse::Button2) && m_CameraMode == CameraMode::FIRST_PERSON)
		{
			if (m_MouseBool)
			{
				m_InitialMousePosition.x = Input::GetMouseX();
				m_InitialMousePosition.y = Input::GetMouseY();

				m_MouseBool = false;
			}

			const glm::vec2& mouse{ Input::GetMouseX(), Input::GetMouseY() };
			glm::vec2 delta;
			delta.x = mouse.x - m_InitialMousePosition.x;
			delta.y = m_InitialMousePosition.y - mouse.y;

			m_InitialMousePosition = mouse;


			float sensitivity = 0.1f;
			delta *= sensitivity;

			m_Yaw += delta.x;
			m_Pitch += delta.y;


			if (m_Pitch > 89.0f) m_Pitch = 89.0f;
			if (m_Pitch < -89.0f) m_Pitch = -89.0f;

			glm::vec3 front;
			front.x = cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
			front.y = sin(glm::radians(m_Pitch));
			front.z = sin(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
			m_CameraFront = glm::normalize(front);

			return false;
		}

		m_MouseBool = true;

		return false;
	}

	void EditorCamera::MousePan(const glm::vec2& delta)
	{
		auto [xSpeed, ySpeed] = PanSpeed();
		m_FocalPoint += -GetRightDirection() * delta.x * xSpeed * m_Distance;
		m_FocalPoint += GetUpDirection() * delta.y * ySpeed * m_Distance;
	}

	void EditorCamera::MouseRotate(const glm::vec2& delta)
	{
		float yawSign = GetUpDirection().y < 0 ? -1.0f : 1.0f;
		m_Yaw += yawSign * delta.x * RotationSpeed();
		m_Pitch += delta.y * RotationSpeed();
	}

	void EditorCamera::MouseZoom(float delta)
	{
		m_Distance -= delta * ZoomSpeed();
		if (m_Distance < 1.0f)
		{
			m_FocalPoint += GetForwardDirection();
			m_Distance = 1.0f;
		}
	}

	glm::vec3 EditorCamera::GetUpDirection() const
	{
		return glm::rotate(GetOrientation(), glm::vec3(0.0f, 1.0f, 0.0f));
	}

	glm::vec3 EditorCamera::GetRightDirection() const
	{
		return glm::rotate(GetOrientation(), glm::vec3(1.0f, 0.0f, 0.0f));
	}

	glm::vec3 EditorCamera::GetForwardDirection() const
	{
		return glm::rotate(GetOrientation(), glm::vec3(0.0f, 0.0f, -1.0f));
	}

	glm::vec3 EditorCamera::CalculatePosition() const
	{
		return m_FocalPoint - GetForwardDirection() * m_Distance;
	}

	glm::quat EditorCamera::GetOrientation() const
	{
		return glm::quat(glm::vec3(-m_Pitch, -m_Yaw, 0.0f));
	}
}