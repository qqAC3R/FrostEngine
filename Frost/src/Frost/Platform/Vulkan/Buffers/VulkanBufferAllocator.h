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
		CPU_ONLY,
		CPU_TO_GPU,
		GPU_TO_CPU,
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