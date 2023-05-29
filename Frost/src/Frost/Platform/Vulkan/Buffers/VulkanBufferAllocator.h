#pragma once

#include "Frost/Platform/Vulkan/Vulkan.h"
#include "Frost/Renderer/Buffers/BufferDevice.h"

using VmaAllocation = struct VmaAllocation_T*;
enum VmaMemoryUsage;

namespace Frost
{
	struct GPUMemoryStats;

	struct VulkanMemoryInfo
	{
		VmaAllocation allocation;
	};

	struct VkAccelerationStructure
	{
		VkAccelerationStructureKHR Structure;
		VkBuffer Buffer;
		VkDeviceMemory BufferMemory;
	};

	enum class MemoryUsage
	{
		// Memory will be mappable on host. It usually means CPU(system) memory.
		CPU_ONLY,

		// Memory that is both mappable on host (guarantees to be `HOST_VISIBLE`) and preferably fast to access by GPU.
		// Usage: Resources written frequently by host (dynamic), read by device. E.g. textures (with LINEAR layout), vertex buffers, uniform buffers updated every frame or every draw call.
		CPU_TO_GPU,
		
		// Memory mappable on host (guarantees to be `HOST_VISIBLE`) and cached.
		// Usage: Resources written by device, read by host - results of some computations, e.g.screen capture, average scene luminance for HDR tone mapping.
		GPU_TO_CPU,

		// Memory will be used on device only, so fast access from the device is preferred.
		GPU_ONLY
	};

	class VulkanAllocator
	{
	private:
		static void Init();
		static void ShutDown();

	public:
		static void AllocateBuffer(VkDeviceSize size, std::vector<BufferUsage> usage, MemoryUsage memoryFlags, VkBuffer& buffer, VulkanMemoryInfo& bufferMemory);
		static void AllocateBuffer(VkDeviceSize size, std::vector<BufferUsage> usage, VkBuffer& buffer, VulkanMemoryInfo& bufferMemory, void* data);

		static void BindBuffer(VkBuffer& buffer, VulkanMemoryInfo& memory, void** data);
		static void UnbindBuffer(VulkanMemoryInfo& memory);

		static void AllocateMemoryForImage(VkImage& image, VulkanMemoryInfo& memory);
		static void AllocateImage(VkImageCreateInfo imageCreateInfo, MemoryUsage memoryFlags, VkImage& image, VulkanMemoryInfo& memory);
		static void DestroyImage(const VkImage& image, const VulkanMemoryInfo& memory);

		static void DeleteBuffer(VkBuffer& buffer, VulkanMemoryInfo& memory);
		static void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

		static GPUMemoryStats GetMemoryStats();
	private:
		friend class VulkanContext;
	};

}