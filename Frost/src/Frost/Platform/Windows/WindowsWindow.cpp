#include "frostpch.h"
#include "WindowsWindow.h"

#include "Frost/Core/Application.h"
#include "Frost/Events/ApplicationEvent.h"
#include "Frost/Events/KeyEvent.h"
#include "Frost/Events/MouseEvent.h"

#include "Frost/Renderer/Renderer.h"

#include "stb_image.h"

namespace Frost
{
	bool s_GLFWInitialized = false;
	static void GLFWErrorCallback(int error, const char* description)
	{
		FROST_CORE_ERROR("GLFW Error ({0}): {1}", error, description);
	}

	Window* Window::Create(const WindowProps& props)
	{
		return new WindowsWindow(props);
	}

	WindowsWindow::WindowsWindow(const WindowProps& props)
	{
		Init(props);
		SetAppIcon();
	}

	WindowsWindow::~WindowsWindow()
	{
		Shutdown();
	}

	void WindowsWindow::OnUpdate()
	{
		glfwPollEvents();
	}

	void WindowsWindow::EnableCursour()
	{
		glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}

	void WindowsWindow::HideCursour()
	{
		glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
	}

	void WindowsWindow::DisableCursour()
	{
		glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	}

	double WindowsWindow::GetTime()
	{
		return glfwGetTime();
	}

	void WindowsWindow::SetVSync(bool enabled)
	{
		if (enabled)
			glfwSwapInterval(1);
		else
			glfwSwapInterval(0);

		m_Data.VSync = enabled;
	}

	void WindowsWindow::SetWindowPosition(int32_t x, int32_t y)
	{
		glfwSetWindowPos(m_Window, x, y);
	}

	void WindowsWindow::SetWindowName(const std::string& name)
	{
		m_Data.Name = name;
		glfwSetWindowTitle(m_Window, m_Data.Name.c_str());
	}

	void WindowsWindow::SetWindowProjectName(const std::string& projectName)
	{
		std::string version = Application::GetVersion();
		std::string title = projectName + " | Frost " + version;;

		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::Vulkan:
			title += " (Vulkan)"; break;
		case RendererAPI::API::None:
			FROST_ASSERT(false, "Renderer::API does not have an API");
		}

		title += " | x64 ";

#ifdef FROST_DEBUG
		title += "Debug";
#elif FROST_RELEASE
		title += "Release";
#endif

		m_Data.Name = title;
		glfwSetWindowTitle(m_Window, m_Data.Name.c_str());
	}

	bool WindowsWindow::IsMaximized() const
	{
		return (bool)glfwGetWindowAttrib(m_Window, GLFW_MAXIMIZED);
	}

	void WindowsWindow::RestoreWindow() const
	{
		glfwRestoreWindow(m_Window);
	}

	void WindowsWindow::MinimizeWindow() const
	{
		glfwIconifyWindow(m_Window);
	}

	void WindowsWindow::MaximizeWindow() const
	{
		glfwMaximizeWindow(m_Window);
	}

	void WindowsWindow::Resize(uint32_t width, uint32_t height)
	{
		m_Context->Resize(width, height);
		m_Data.Width = width;
		m_Data.Height = height;
	}

	void WindowsWindow::Init(const WindowProps& props)
	{
		m_Data.Width = props.Width;
		m_Data.Height = props.Height;
		m_Data.Name = props.Title;

		if (!s_GLFWInitialized)
		{
			int success = glfwInit();
			FROST_ASSERT(success, "Could not intialize GLFW!");
			glfwSetErrorCallback(GLFWErrorCallback);

			s_GLFWInitialized = true;
		}

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_DECORATED, false);

		FROST_CORE_INFO("Creating window '{0}' ({1}, {2})", props.Title, props.Width, props.Height);



		m_Window = glfwCreateWindow(m_Data.Width, m_Data.Height, m_Data.Name.c_str(), nullptr, nullptr);
		glfwSetWindowUserPointer(m_Window, &m_Data);

		// Set the window's position to the center of the screen
		const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
		glfwSetWindowPos(m_Window, (mode->width / 2)  - (props.Width / 2), (mode->height / 2) - (props.Height / 2)); // TODO: Add automatic centerization by the resolution

		m_Context = GraphicsContext::Create(m_Window);
		m_Context->Init();

		// Set GLFW Callbacks
		glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* window, int width, int height)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
			data.Width = width;
			data.Height = height;

			WindowResizeEvent event(width, height);
			data.EventCallback(event);
		});

		glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* window)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
			WindowCloseEvent event;
			data.EventCallback(event);
		});

		glfwSetKeyCallback(m_Window, [](GLFWwindow* window, int key, int scancode, int action, int mods)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

			switch (action)
			{
				case GLFW_PRESS:
				{
					KeyPressedEvent event(key, 0);
					data.EventCallback(event);
					break;
				}
				case GLFW_RELEASE:
				{
					KeyReleasedEvent event(key);
					data.EventCallback(event);
					break;
				}
				case GLFW_REPEAT:
				{
					KeyPressedEvent event(key, 1);
					data.EventCallback(event);
					break;
				}
			}
		});

		glfwSetCharCallback(m_Window, [](GLFWwindow* window, unsigned int keycode)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

			KeyTypedEvent event(keycode);
			data.EventCallback(event);
		});

		glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* window, int button, int action, int mods)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

			switch (action)
			{
			case GLFW_PRESS:
			{
				MouseButtonPressedEvent event(button);
				data.EventCallback(event);
				break;
			}
			case GLFW_RELEASE:
			{
				MouseButtonReleasedEvent event(button);
				data.EventCallback(event);
				break;
			}
			}
		});

		glfwSetScrollCallback(m_Window, [](GLFWwindow* window, double xOffset, double yOffset)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

			MouseScrolledEvent event((float)xOffset, (float)yOffset);
			data.EventCallback(event);
		});

		glfwSetCursorPosCallback(m_Window, [](GLFWwindow* window, double xPos, double yPos)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

			MouseMovedEvent event((float)xPos, (float)yPos);
			data.EventCallback(event);
		});
	}

	void WindowsWindow::Shutdown()
	{
		glfwDestroyWindow(m_Window);
	}

	void WindowsWindow::RecreateWindow()
	{

	}

	void WindowsWindow::SetAppIcon()
	{
		GLFWimage icon;
		int channels;
		icon.pixels = stbi_load("Resources/Editor/Logo_Frost_I.png", &icon.width, &icon.height, &channels, 4);
		glfwSetWindowIcon(m_Window, 1, &icon);
		stbi_image_free(icon.pixels);
	}
}