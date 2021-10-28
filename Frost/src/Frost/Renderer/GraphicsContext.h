#pragma once

#include "Frost/Platform/Vulkan/VulkanDevice.h"

struct GLFWwindow;

namespace Frost
{
	class WindowDimension
	{
		uint32_t Width, Height;
	};

	enum class GraphicsType
	{
		Graphics, Compute, Raytracing
	};

	class GraphicsContext
	{
	public:
		virtual ~GraphicsContext() {}

		virtual void Init() = 0;
		virtual void Resize(uint32_t width, uint32_t height) = 0;

		/* OpenGL Specific */
		virtual void SwapBuffers() = 0;

		/* Vulkan Specific */
		virtual WindowDimension GetWindowDimension() = 0;
		virtual void WaitDevice() = 0;

		static Scope<GraphicsContext> Create(GLFWwindow* window);

	};

}