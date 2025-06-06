#pragma once

#include <GLFW/glfw3.h>

#include "Frost/Core/Window.h"
#include "Frost/Renderer/GraphicsContext.h"

namespace Frost
{
	class WindowsWindow : public Window
	{
	public:
		WindowsWindow(const WindowProps& props);
		virtual ~WindowsWindow();

		virtual void OnUpdate() override;

		virtual uint32_t GetWidth() const override { return m_Data.Width; }
		virtual uint32_t GetHeight() const override { return m_Data.Height; }

		virtual void SetWindowPosition(int32_t x, int32_t y) override;
		virtual void SetWindowName(const std::string& name) override;
		virtual void SetWindowProjectName(const std::string& projectName) override;

		// Window attributes
		virtual void EnableCursour() override;
		virtual void HideCursour() override;
		virtual void DisableCursour() override;
		virtual double GetTime() override;
		virtual void SetEventCallback(const EventCallbackFn& callback) override { m_Data.EventCallback = callback; }
		virtual void SetVSync(bool enabled) override;

		virtual bool IsVSync() const override { return m_Data.VSync; }
		virtual bool IsMaximized() const override;

		virtual void RestoreWindow() const override;
		virtual void MinimizeWindow() const override;
		virtual void MaximizeWindow() const override;

		inline virtual void* GetNativeWindow() const override { return m_Window; }
		virtual const Scope<GraphicsContext>& GetGraphicsContext() const override { return m_Context; }

		virtual void Resize(uint32_t width, uint32_t height) override;

	private:
		void Init(const WindowProps& props);
		void Shutdown();
		void RecreateWindow();

		void SetAppIcon();
	private:
		GLFWwindow* m_Window;

		Scope<GraphicsContext> m_Context;

		struct WindowData
		{
			uint32_t Width;
			uint32_t Height;
			std::string Name;
			EventCallbackFn EventCallback;

			bool VSync = true;
		};

		WindowData m_Data;

		bool m_IsResized = false;

	};
}