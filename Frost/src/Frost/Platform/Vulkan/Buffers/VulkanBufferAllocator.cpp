#include "frostpch.h"
#include "VulkanBufferAllocator.h"

#include "Frost/Platform/Vulkan/VulkanContext.h"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

namespace Frost
{
	static VmaAllocator s_Allocator;
	static bool s_AllocatorDestroyed = true;

	namespace Utils
	{
		static VkBufferUsageFlagBits BufferTypeToVk(BufferUsage usage);
		static VmaMemoryUsage GetVmaMemoryUsage(MemoryUsage usage);
		static uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
	}

	void VulkanAllocator::Init()
	{
		VkPhysicalDevice physicalDevice = VulkanContext::GetCurrentDevice()->GetPhysicalDevice();
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		VkInstance instance = VulkanContext::GetInstance();

		VmaAllocatorCreateInfo allocatorInfo = {};
		allocatorInfo.physicalDevice = physicalDevice;
		allocatorInfo.device = device;
		allocatorInfo.instance = instance;
		allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
		allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_2;

		FROST_VKCHECK(vmaCreateAllocator(&allocatorInfo, &s_Allocator));
		s_AllocatorDestroyed = false;
	}

	void VulkanAllocator::ShutDown()
	{
		vmaDestroyAllocator(s_Allocator);
		s_AllocatorDestroyed = true;
	}

	void VulkanAllocator::AllocateBuffer(VkDeviceSize size, std::vector<BufferUsage> usage, MemoryUsage memoryFlags,
										 VkBuffer& buffer, VulkanMemoryInfo& bufferMemory)
	{

		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		VkBufferUsageFlags bufferUsage{};
		for (auto& type : usage) { bufferUsage |= Utils::BufferTypeToVk(type); }

		VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		bufferInfo.usage = bufferUsage;
		bufferInfo.size = size;

		VmaAllocationCreateInfo allocCreateInfo{};
		allocCreateInfo.usage = Utils::GetVmaMemoryUsage(memoryFlags);

		FROST_VKCHECK(vmaCreateBuffer(s_Allocator, &bufferInfo, &allocCreateInfo, &buffer, &bufferMemory.allocation, nullptr));
	}

	void VulkanAllocator::AllocateBuffer(VkDeviceSize size, std::vector<BufferUsage> usage,
										 VkBuffer& buffer, VulkanMemoryInfo& bufferMemory, void* data)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		usage.push_back(BufferUsage::TransferDst);


		// Initializating the staging buffer
		VkBuffer stagingBuffer;
		VulkanMemoryInfo stagingBufferMemory;
		AllocateBuffer(size, { BufferUsage::TransferSrc }, MemoryUsage::CPU_ONLY, stagingBuffer, stagingBufferMemory);

		// Binding the data to the staging buffer
		void* stageData;
		vmaMapMemory(s_Allocator, stagingBufferMemory.allocation, &stageData);
		memcpy(stageData, data, (size_t)size);
		vmaUnmapMemory(s_Allocator, stagingBufferMemory.allocation);

		// Creating the buffer allocated on the gpu
		AllocateBuffer(size, usage, MemoryUsage::GPU_ONLY, buffer, bufferMemory);


		// Copying the data from the staging buffer to the allocated one on the gpu
		VkCommandBuffer cmdBuf = VulkanContext::GetCurrentDevice()->AllocateCommandBuffer(RenderQueueType::Graphics, true);
		VkBufferCopy copyRegion{};
		copyRegion.size = size;
		vkCmdCopyBuffer(cmdBuf, stagingBuffer, buffer, 1, &copyRegion);
		VulkanContext::GetCurrentDevice()->FlushCommandBuffer(cmdBuf);

		vmaDestroyBuffer(s_Allocator, stagingBuffer, stagingBufferMemory.allocation);
	}

	void VulkanAllocator::AllocateMemoryForImage(VkImage& image, VulkanMemoryInfo& memory)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		// Getting the memory requirments for the image (size, memory type)
		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(device, image, &memRequirements);

		auto allocationSize = memRequirements.size;
		auto memoryType = Utils::FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VmaAllocationCreateInfo allocationCreateInfo{};
		allocationCreateInfo.memoryTypeBits = memoryType;

		VmaAllocationInfo allocationInfo{};
		allocationInfo.memoryType = memoryType;
		allocationInfo.size = allocationSize;

		FROST_VKCHECK(vmaAllocateMemoryForImage(s_Allocator, image, &allocationCreateInfo, &memory.allocation, &allocationInfo));
		FROST_VKCHECK(vmaBindImageMemory(s_Allocator, memory.allocation, image));
	}

	void VulkanAllocator::AllocateImage(VkImageCreateInfo imageCreateInfo, MemoryUsage memoryFlags, VkImage& image, VulkanMemoryInfo& memory)
	{
		VmaAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.usage = Utils::GetVmaMemoryUsage(memoryFlags);

		FROST_VKCHECK(vmaCreateImage(s_Allocator, &imageCreateInfo, &allocCreateInfo, &image, &memory.allocation, nullptr));
	}

	void VulkanAllocator::DestroyImage(const VkImage& image, const VulkanMemoryInfo& memory)
	{
		vmaDestroyImage(s_Allocator, image, memory.allocation);
	}

	void VulkanAllocator::DeleteBuffer(VkBuffer& buffer, VulkanMemoryInfo& memory)
	{
		vmaDestroyBuffer(s_Allocator, buffer, memory.allocation);
	}

	void VulkanAllocator::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
	{
		VkCommandBuffer cmdBuf = VulkanContext::GetCurrentDevice()->AllocateCommandBuffer(RenderQueueType::Graphics, true);

		VkBufferCopy copyRegion{};
		copyRegion.size = size;

		vkCmdCopyBuffer(cmdBuf, srcBuffer, dstBuffer, 1, &copyRegion);

		VulkanContext::GetCurrentDevice()->FlushCommandBuffer(cmdBuf);
	}

	void VulkanAllocator::BindBuffer(VkBuffer& buffer, VulkanMemoryInfo& memory, void** data)
	{
		FROST_VKCHECK(vmaMapMemory(s_Allocator, memory.allocation, data));
	}

	void VulkanAllocator::UnbindBuffer(VulkanMemoryInfo& memory)
	{
		vmaUnmapMemory(s_Allocator, memory.allocation);
	}

	GPUMemoryStats VulkanAllocator::GetMemoryStats()
	{
		if (s_AllocatorDestroyed) return GPUMemoryStats(UINT64_MAX, UINT64_MAX);

		VmaStats stats;
		vmaCalculateStats(s_Allocator, &stats);

		uint64_t usedMemory = stats.total.usedBytes;
		uint64_t freeMemory = stats.total.unusedBytes;

		return GPUMemoryStats(usedMemory, freeMemory);
	}

	namespace Utils
	{

		static VkBufferUsageFlagBits BufferTypeToVk(BufferUsage usage)
		{
			switch (usage)
			{
			case BufferUsage::Uniform:							return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
			case BufferUsage::Storage:							return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
			case BufferUsage::Vertex:							return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
			case BufferUsage::Index:							return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
			case BufferUsage::AccelerationStructure:			return VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
			case BufferUsage::AccelerationStructureReadOnly:	return VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
			case BufferUsage::TransferSrc:						return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			case BufferUsage::TransferDst:						return VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			case BufferUsage::ShaderAddress:					return VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
			case BufferUsage::ShaderBindingTable:				return VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;
			case BufferUsage::Indirect:							return VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
			}

			FROST_ASSERT_MSG("Couldn't find the buffer usgae flag bits");
			return VkBufferUsageFlagBits();
		}

		static VmaMemoryUsage GetVmaMemoryUsage(MemoryUsage usage)
		{
			switch (usage)
			{
			case MemoryUsage::CPU_ONLY:	   return VMA_MEMORY_USAGE_CPU_ONLY;
			case MemoryUsage::CPU_TO_GPU:  return VMA_MEMORY_USAGE_CPU_TO_GPU;
			case MemoryUsage::GPU_TO_CPU:  return VMA_MEMORY_USAGE_GPU_TO_CPU;
			case MemoryUsage::GPU_ONLY:    return VMA_MEMORY_USAGE_GPU_ONLY;
			}
			return VMA_MEMORY_USAGE_UNKNOWN;
		}

		static uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
		{
			VkPhysicalDevice physicalDevice = VulkanContext::GetCurrentDevice()->GetPhysicalDevice();

			VkPhysicalDeviceMemoryProperties memProperties;
			vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

			for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
				if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
					return i;
				}
			}

			FROST_ASSERT_MSG("Failed to find suitable memory type!");
			return 0;
		}
	}
}