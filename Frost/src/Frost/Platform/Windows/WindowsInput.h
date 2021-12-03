#pragma once

#include "Frost/Core/Input.h"

namespace Frost
{

	class WindowsInput : public Input
	{
	protected:
		virtual bool IsKeyPressedImpl(int keycode) override;

		virtual bool IsMouseButtonPressedImpl(int button) override;
		virtual std::pair<float, float> GetMousePosImpl() override;
		virtual float GetMouseXImpl() override;
		virtual float GetMouseYImpl() override;

		virtual void SetImplCursorMode(CursorMode mode) override;
		virtual CursorMode GetImplCursorMode() override;
	};

}