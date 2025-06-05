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

	struct GPUMemoryStats
	{
		GPUMemoryStats() = default;
		GPUMemoryStats(uint64_t used, uint64_t free) : UsedMemory(used), FreeMemory(free) {}

		uint64_t UsedMemory = 0;
		uint64_t FreeMemory = 0;
	};

	class GraphicsContext
	{
	public:
		virtual ~GraphicsContext() {}

		virtual void Init() = 0;
		virtual void Resize(uint32_t width, uint32_t height) = 0;

		virtual WindowDimension GetWindowDimension() = 0;
		virtual void WaitDevice() const = 0;
		virtual GPUMemoryStats GetGPUMemoryStats() const = 0;

		static Scope<GraphicsContext> Create(GLFWwindow* window);
	};

}