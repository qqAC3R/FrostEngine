#pragma once

#include "Frost/Events/Event.h"
#include "Frost/Renderer/GraphicsContext.h"

namespace Frost
{
	struct WindowProps
	{
		std::string Title;
		unsigned int Width;
		unsigned int Height;

		WindowProps(const std::string& title = "Frost Engine",
			unsigned int width = 1600,
			unsigned int height = 900)
			: Title(title), Width(width), Height(height)
		{
		}
	};

	class Window
	{
	public:
		using EventCallbackFn = std::function<void(Event&)>;

		virtual ~Window() {}

		virtual void OnUpdate() = 0;

		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;

		virtual void SetWindowPosition(int32_t x, int32_t y) = 0;
		virtual void SetWindowName(const std::string& name) = 0;
		virtual void SetWindowProjectName(const std::string& projectName) = 0;

		// Windows attributes
		virtual void EnableCursour() = 0;
		virtual void DisableCursour() = 0;
		virtual double GetTime() = 0;
		virtual void SetEventCallback(const EventCallbackFn& callback) = 0;
		virtual void SetVSync(bool enabled) = 0;

		virtual bool IsVSync() const = 0;
		virtual bool IsMaximized() const = 0;

		virtual void RestoreWindow() const = 0;
		virtual void MinimizeWindow() const = 0;
		virtual void MaximizeWindow() const = 0;

		virtual void* GetNativeWindow() const = 0;
		virtual const Scope<GraphicsContext>& GetGraphicsContext() const = 0;

		virtual void Resize(uint32_t width, uint32_t height) = 0;

		static Window* Create(const WindowProps& props = WindowProps());
	};
}