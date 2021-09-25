#include "frostpch.h"
#include "VulkanBufferAllocator.h"

#include "Frost/Platform/Vulkan/VulkanContext.h"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

namespace Frost
{
	static VmaAllocator s_Allocator;

	namespace Utils
	{

		static VkBufferUsageFlagBits BufferTypeToVk(BufferType usage)
		{
			switch (usage)
			{
				case BufferType::Uniform:						return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
				case BufferType::Storage:						return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
				case BufferType::Vertex:						return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
				case BufferType::Index:							return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
				case BufferType::AccelerationStructure:			return VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
				case BufferType::AccelerationStructureReadOnly:	return VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
				case BufferType::TransferSrc:					return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
				case BufferType::TransferDst:					return VK_BUFFER_USAGE_TRANSFER_DST_BIT;
				case BufferType::ShaderAddress:					return VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
				case BufferType::ShaderBindingTable:			return VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;
			}

			FROST_ASSERT(0, "Couldn't find the buffer usgae flag bits");
			return VkBufferUsageFlagBits();
		}

		static VmaMemoryUsage GetVmaMemoryUsage(MemoryUsage usage)
		{
			switch (usage)
			{
				case MemoryUsage::CPU_ONLY:	   return VMA_MEMORY_USAGE_CPU_ONLY;
				case MemoryUsage::CPU_AND_GPU: return VMA_MEMORY_USAGE_CPU_TO_GPU;
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

			FROST_ASSERT(false, "Failed to find suitable memory type!");
			return 0;
		}
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

		vmaCreateAllocator(&allocatorInfo, &s_Allocator);
	}

	void VulkanAllocator::ShutDown()
	{
		vmaDestroyAllocator(s_Allocator);
	}

	void VulkanAllocator::AllocateBuffer(VkDeviceSize size, std::vector<BufferType> usage, MemoryUsage memoryFlags,
										 VkBuffer& buffer, VulkanMemoryInfo& bufferMemory)
	{

		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		VkBufferUsageFlags bufferUsage{};
		for (auto& type : usage)
			bufferUsage |= Utils::BufferTypeToVk(type);


		VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bufferInfo.size = size;
		bufferInfo.usage = bufferUsage;

		VmaAllocationCreateInfo allocCreateInfo{};
		allocCreateInfo.usage = Utils::GetVmaMemoryUsage(memoryFlags);
		//allocCreateInfo.preferredFlags = memoryFlags;

		VmaAllocation allocation;
		vmaCreateBuffer(s_Allocator, &bufferInfo, &allocCreateInfo, &buffer, &allocation, nullptr);

		bufferMemory.allocation = allocation;

	}

	void VulkanAllocator::AllocateBuffer(VkDeviceSize size, std::vector<BufferType> usage,
										 VkBuffer& buffer, VulkanMemoryInfo& bufferMemory, void* data)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		std::vector<BufferType> usages;
		for (auto& type : usage)
			usages.push_back(type);
		usages.push_back(BufferType::TransferDst);


		// Initializating the staging buffer
		VkBuffer stagingBuffer;
		VulkanMemoryInfo stagingBufferMemory;
		AllocateBuffer(size, { BufferType::TransferSrc }, MemoryUsage::CPU_AND_GPU, stagingBuffer, stagingBufferMemory);

		// Binding the data to the staging buffer
		void* stageData;
		vmaMapMemory(s_Allocator, stagingBufferMemory.allocation, &stageData);
		memcpy(stageData, data, (size_t)size);
		vmaUnmapMemory(s_Allocator, stagingBufferMemory.allocation);

		// Creating the buffer allocated on the gpu
		AllocateBuffer(size, usages, MemoryUsage::GPU_ONLY, buffer, bufferMemory);


		// Copying the data from the staging buffer to the allocated one on the gpu
		VkCommandBuffer cmdBuf = VulkanContext::GetCurrentDevice()->AllocateCommandBuffer(true);
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

		vmaAllocateMemoryForImage(s_Allocator, image, &allocationCreateInfo, &memory.allocation, &allocationInfo);
		vmaBindImageMemory(s_Allocator, memory.allocation, image);
	}

	void VulkanAllocator::DestroyImage(const VkImage& image, const VulkanMemoryInfo& memory)
	{
		vmaDestroyImage(s_Allocator, image, memory.allocation);
	}

	void VulkanAllocator::DeleteBuffer(VkBuffer& buffer, VulkanMemoryInfo& memory)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		vmaDestroyBuffer(s_Allocator, buffer, memory.allocation);
	}

	void VulkanAllocator::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
	{
		VkCommandBuffer cmdBuf = VulkanContext::GetCurrentDevice()->AllocateCommandBuffer(true);

		VkBufferCopy copyRegion{};
		copyRegion.size = size;

		vkCmdCopyBuffer(cmdBuf, srcBuffer, dstBuffer, 1, &copyRegion);

		VulkanContext::GetCurrentDevice()->FlushCommandBuffer(cmdBuf);
	}

	void VulkanAllocator::BindBuffer(VkBuffer& buffer, VulkanMemoryInfo& memory, void** data)
	{
		vmaMapMemory(s_Allocator, memory.allocation, data);
	}

	void VulkanAllocator::UnbindBuffer(VulkanMemoryInfo& memory)
	{
		vmaUnmapMemory(s_Allocator, memory.allocation);
	}

}