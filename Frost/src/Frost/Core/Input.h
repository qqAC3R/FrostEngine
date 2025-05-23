#pragma once

#include "Frost/Core/Engine.h"

namespace Frost
{
	enum class CursorMode
	{
		Normal = 0,
		Hidden = 1,
		Locked = 2
	};

	class Input
	{
	public:
		inline static bool IsKeyPressed(int keycode) { return s_Instance->IsKeyPressedImpl(keycode); }

		inline static bool IsMouseButtonPressed(int button) { return s_Instance->IsMouseButtonPressedImpl(button); }
		inline static std::pair<float, float> GetMousePosition() { return s_Instance->GetMousePosImpl(); }
		inline static float GetMouseX() { return s_Instance->GetMouseXImpl(); }
		inline static float GetMouseY() { return s_Instance->GetMouseYImpl(); }

		static void SetCursorMode(CursorMode mode) { return s_Instance->SetImplCursorMode(mode); }
		static CursorMode GetCursorMode() { return s_Instance->GetImplCursorMode(); }
	protected:
		virtual bool IsKeyPressedImpl(int keycode) = 0;

		virtual bool IsMouseButtonPressedImpl(int button) = 0;
		virtual std::pair<float, float> GetMousePosImpl() = 0;
		virtual float GetMouseXImpl() = 0;
		virtual float GetMouseYImpl() = 0;

		virtual void SetImplCursorMode(CursorMode mode) = 0;
		virtual CursorMode GetImplCursorMode() = 0;
	private:
		static Input* s_Instance;
	};
}