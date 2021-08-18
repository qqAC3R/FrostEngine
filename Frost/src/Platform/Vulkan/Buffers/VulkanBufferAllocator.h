#pragma once

#include <vulkan/vulkan.hpp>

#include "Frost/Renderer/Buffers/Buffer.h"

using VmaAllocation = struct VmaAllocation_T*;

namespace Frost
{

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

	class VulkanBufferAllocator
	{
	public:
		static void Init();
		static void ShutDown();


		static void CreateBuffer(VkDeviceSize size, std::vector<BufferType> usage, VkMemoryPropertyFlags memoryFlags,
			VkBuffer& buffer, VulkanMemoryInfo& bufferMemory);

		static void CreateBuffer(VkDeviceSize size, std::vector<BufferType> usage, VkMemoryPropertyFlags memoryFlags,
			VkBuffer& buffer, VulkanMemoryInfo& bufferMemory, void* data);

		static void DeleteBuffer(VkBuffer& buffer, VulkanMemoryInfo& memory);
		static void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

		static void BindBuffer(VkBuffer& buffer, VulkanMemoryInfo& memory, void** data);
		static void UnbindBuffer(VulkanMemoryInfo& memory);

		static void CreateMemoryForImage(VkImage& image, VulkanMemoryInfo& memory);
		static void DestroyImage(const VkImage& image, const VulkanMemoryInfo& memory);

#if 0
		static void CreateBuffer(VkDeviceSize size, std::vector<BufferType> usage, VkMemoryPropertyFlags memoryFlags,
								 VkBuffer& buffer, VkDeviceMemory& bufferMemory);

		static void CreateBuffer(VkDeviceSize size, std::vector<BufferType> usage, VkMemoryPropertyFlags memoryFlags,
								 VkBuffer& buffer, VkDeviceMemory& bufferMemory, void* data);

		static VkAccelerationStructure CreateAccelerationStructure(VkAccelerationStructureCreateInfoKHR& asInfo);

		static void DeleteBuffer(VkBuffer& buffer, VkDeviceMemory& memory);
		//static void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
#endif

		static uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

	};

}