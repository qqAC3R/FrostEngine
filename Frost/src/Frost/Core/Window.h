#pragma once

//#include <GLFW/glfw3.h>

#include "frostpch.h"

#include "Frost/Events/Event.h"
#include "Frost/Renderer/GraphicsContext.h"
//#include "Frost/Events/ApplicationEvent.h"
//#include "Frost/Events/KeyEvent.h"
//#include "Frost/Events/MouseEvent.h"

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

		// Windows attributes
		virtual void EnableCursour() = 0;
		virtual void DisableCursour() = 0;
		virtual double GetTime() = 0;
		virtual void SetEventCallback(const EventCallbackFn& callback) = 0;
		virtual void SetVSync(bool enabled) = 0;
		virtual bool IsVSync() const = 0;

		virtual void* GetNativeWindow() const = 0;
		virtual const Scope<GraphicsContext>& GetGraphicsContext() const = 0;

		virtual void Resize(uint32_t width, uint32_t height) = 0;

		static Window* Create(const WindowProps& props = WindowProps());
	};


	/*
	class Window
	{
	public:
		Window(uint32_t width, uint32_t height, std::string name);
		virtual ~Window();

		using EventCallbackFn = std::function<void(Event&)>;


		inline uint32_t GetWidth() const{ return m_Data.Width; }
		inline uint32_t GetHeight() const{ return m_Data.Height; }
		
		double GetTime() { return glfwGetTime(); }

		inline GLFWwindow* GetWindow() const { return m_Window; };

		void SetData(uint32_t width, uint32_t height, std::string name);
		void SetVsync(bool enabled);

		void GetSize(int& width, int& height);

		void SetEventCallback(const EventCallbackFn& callback) { m_Data.EventCallback = callback; }

		void OnUpdate();

		void EnableCursour();
		void DisableCursour();

	private:
		void RecreateWindow();

	private:
		GLFWwindow* m_Window;

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
	*/
}