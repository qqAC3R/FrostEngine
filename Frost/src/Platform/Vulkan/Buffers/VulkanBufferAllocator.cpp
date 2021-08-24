#include "frostpch.h"
#include "VulkanBufferAllocator.h"

#include <vulkan/vulkan.hpp>
#include "Platform/Vulkan/VulkanCommandBuffer.h"

#include "Platform/Vulkan/VulkanContext.h"

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

		static VmaMemoryUsage vkToVmaMemoryUsage(VkMemoryPropertyFlags flags)
		{
			if ((flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
				return VMA_MEMORY_USAGE_GPU_ONLY;

			else if ((flags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
					(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
				return VMA_MEMORY_USAGE_CPU_ONLY;

			else if ((flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
				return VMA_MEMORY_USAGE_CPU_TO_GPU;

			return VMA_MEMORY_USAGE_UNKNOWN;
		}

	}

	void VulkanBufferAllocator::Init()
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

	void VulkanBufferAllocator::ShutDown()
	{
		vmaDestroyAllocator(s_Allocator);
	}

	void VulkanBufferAllocator::CreateBuffer(VkDeviceSize size, std::vector<BufferType> usage, VkMemoryPropertyFlags memoryFlags,
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
		allocCreateInfo.usage = Utils::vkToVmaMemoryUsage(memoryFlags);
		allocCreateInfo.preferredFlags = memoryFlags;

		VmaAllocation allocation;
		vmaCreateBuffer(s_Allocator, &bufferInfo, &allocCreateInfo, &buffer, &allocation, nullptr);

		bufferMemory.allocation = allocation;

	}

	void VulkanBufferAllocator::CreateBuffer(VkDeviceSize size, std::vector<BufferType> usage, VkMemoryPropertyFlags properties,
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
		CreateBuffer(size,
					{ BufferType::TransferSrc },
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					stagingBuffer, stagingBufferMemory);

		// Binding the data to the staging buffer
		void* stageData;
		vmaMapMemory(s_Allocator, stagingBufferMemory.allocation, &stageData);
		memcpy(stageData, data, (size_t)size);
		vmaUnmapMemory(s_Allocator, stagingBufferMemory.allocation);




		// Creating the buffer allocated on the gpu
		CreateBuffer(size, usages, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, buffer, bufferMemory);


		// Copying the data from the staging buffer to the allocated one on the gpu
		VulkanCommandPool cmdPool;
		auto commandBuffer = VulkanCommandBuffer::BeginSingleTimeCommands(cmdPool);

		VkBufferCopy copyRegion{};
		copyRegion.size = size;
		vkCmdCopyBuffer(commandBuffer.GetCommandBuffer(), stagingBuffer, buffer, 1, &copyRegion);

		VulkanCommandBuffer::EndSingleTimeCommands(commandBuffer);


		cmdPool.Destroy();
		
		vmaDestroyBuffer(s_Allocator, stagingBuffer, stagingBufferMemory.allocation);

	}

	void VulkanBufferAllocator::CreateMemoryForImage(VkImage& image, VulkanMemoryInfo& memory)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		// Getting the memory requirments for the image (size, memory type)
		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(device, image, &memRequirements);

		auto allocationSize = memRequirements.size;
		auto memoryType = VulkanBufferAllocator::FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);


		VmaAllocationCreateInfo allocationCreateInfo{};
		allocationCreateInfo.memoryTypeBits = memoryType;

		VmaAllocationInfo allocationInfo{};
		allocationInfo.memoryType = memoryType;
		allocationInfo.size = allocationSize;

		vmaAllocateMemoryForImage(s_Allocator, image, &allocationCreateInfo, &memory.allocation, &allocationInfo);

		vmaBindImageMemory(s_Allocator, memory.allocation, image);

	}

	void VulkanBufferAllocator::DestroyImage(const VkImage& image, const VulkanMemoryInfo& memory)
	{
		vmaDestroyImage(s_Allocator, image, memory.allocation);
	}


#if 0
	VkAccelerationStructure VulkanBufferAllocator::CreateAccelerationStructure(VkAccelerationStructureCreateInfoKHR& asInfo)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		VkAccelerationStructure accelerationStructure;
		//CreateBuffer(asInfo.size,
		//	{ BufferType::AccelerationStructure, BufferType::ShaderAddress },
		//	VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		//	accelerationStructure.Buffer, accelerationStructure.BufferMemory);

		asInfo.buffer = accelerationStructure.Buffer;

		vkCreateAccelerationStructureKHR(device, &asInfo, nullptr, &accelerationStructure.Structure);

		return accelerationStructure;
	}
#endif

	void VulkanBufferAllocator::DeleteBuffer(VkBuffer& buffer, VulkanMemoryInfo& memory)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		vmaDestroyBuffer(s_Allocator, buffer, memory.allocation);
	}

	void VulkanBufferAllocator::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
	{
		VulkanCommandPool cmdPool;
		auto commandBuffer = VulkanCommandBuffer::BeginSingleTimeCommands(cmdPool);


		VkBufferCopy copyRegion{};
		copyRegion.size = size;

		vkCmdCopyBuffer(commandBuffer.GetCommandBuffer(), srcBuffer, dstBuffer, 1, &copyRegion);

		VulkanCommandBuffer::EndSingleTimeCommands(commandBuffer);


		cmdPool.Destroy();
	}

	void VulkanBufferAllocator::BindBuffer(VkBuffer& buffer, VulkanMemoryInfo& memory, void** data)
	{
		vmaMapMemory(s_Allocator, memory.allocation, data);
	}

	void VulkanBufferAllocator::UnbindBuffer(VulkanMemoryInfo& memory)
	{
		vmaUnmapMemory(s_Allocator, memory.allocation);
	}

	uint32_t VulkanBufferAllocator::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
	{
		VkPhysicalDevice physicalDevice = VulkanContext::GetCurrentDevice()->GetPhysicalDevice();

		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
			if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
				return i;
			}
		}

		FROST_ASSERT(0, "Failed to find suitable memory type!");
		return 0;
	}












#if 0
	void VulkanBufferAllocator::CreateBuffer(VkDeviceSize size, std::vector<BufferType> usage, VkMemoryPropertyFlags memoryFlags,
											 VkBuffer& buffer, VkDeviceMemory& bufferMemory)
	{

		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();


		// Transform from Frost::BufferType to Vk
		VkBufferUsageFlags bufferUsage{};
		for (auto& type : usage)
		{
			bufferUsage |= Utils::BufferTypeToVk(type);
		}

		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;
		bufferInfo.usage = bufferUsage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
		{
			FROST_ASSERT(0, "Failed to create buffer!");
		}

		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, memoryFlags);

		// Device Address
		VkMemoryAllocateFlagsInfo memFlagInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO };
		if (bufferUsage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) memFlagInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

		allocInfo.pNext = &memFlagInfo;

		FROST_VKCHECK(vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory), "Failed to allocate memory for the buffer!");

		FROST_CORE_INFO("Buffer created! Memory: {0}", (void*)&bufferMemory);


		vkBindBufferMemory(device, (VkBuffer)buffer, bufferMemory, 0);



	}

	void VulkanBufferAllocator::CreateBuffer(VkDeviceSize size, std::vector<BufferType> usage, VkMemoryPropertyFlags properties,
		VkBuffer& buffer, VkDeviceMemory& bufferMemory, void* data)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		std::vector<BufferType> usages;
		for (auto& type : usage)
		{
			usages.push_back(type);
		}
		usages.push_back(BufferType::TransferDst);


		// Initializating the staging buffer
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		CreateBuffer(size,
					{ BufferType::TransferSrc },
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					stagingBuffer, stagingBufferMemory);

		// Binding the data to the staging buffer
		//void* stageData;
		//vkMapMemory(device, stagingBufferMemory, 0, size, 0, &stageData);
		//memcpy(stageData, data, (size_t)size);
		//vkUnmapMemory(device, stagingBufferMemory);





		// Creating the buffer allocated on the gpu
		//VkBufferUsageFlags usageFlagBits = VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage;
		CreateBuffer(size,
					 usages,
					 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					 buffer, bufferMemory);


		// Copying the data from the staging buffer to the allocated one on the gpu
		VulkanCommandPool cmdPool;
		auto commandBuffer = VulkanCommandBuffer::BeginSingleTimeCommands(cmdPool);

		VkBufferCopy copyRegion{};
		copyRegion.size = size;
		vkCmdCopyBuffer(commandBuffer.GetCommandBuffer(), stagingBuffer, buffer, 1, &copyRegion);

		VulkanCommandBuffer::EndSingleTimeCommands(commandBuffer);


		cmdPool.Destroy();
		//vmaDestroyBuffer(s_Allocator, stagingBuffer, allo)
		//vkDestroyBuffer(device, stagingBuffer, nullptr);
		//vkFreeMemory(device, stagingBufferMemory, nullptr);


	}

	VkAccelerationStructure VulkanBufferAllocator::CreateAccelerationStructure(VkAccelerationStructureCreateInfoKHR& asInfo)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		VkAccelerationStructure accelerationStructure;
		CreateBuffer(asInfo.size,
					{ BufferType::AccelerationStructure, BufferType::ShaderAddress },
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					accelerationStructure.Buffer, accelerationStructure.BufferMemory);

		asInfo.buffer = accelerationStructure.Buffer;

		vkCreateAccelerationStructureKHR(device, &asInfo, nullptr, &accelerationStructure.Structure);

		return accelerationStructure;
	}

	void VulkanBufferAllocator::DeleteBuffer(VkBuffer& buffer, VkDeviceMemory& memory)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		vkDestroyBuffer(device, buffer, nullptr);
		vkFreeMemory(device, memory, nullptr);
	}

#if 0
	void VulkanBufferAllocator::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
	{
		VulkanCommandPool cmdPool;
		auto commandBuffer = VulkanCommandBuffer::BeginSingleTimeCommands(cmdPool);


		VkBufferCopy copyRegion{};
		copyRegion.size = size;

		vkCmdCopyBuffer(commandBuffer.GetCommandBuffer(), srcBuffer, dstBuffer, 1, &copyRegion);

		VulkanCommandBuffer::EndSingleTimeCommands(commandBuffer);


		cmdPool.Destroy();
	}

	uint32_t VulkanBufferAllocator::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
	{
		VkPhysicalDevice physicalDevice = VulkanContext::GetCurrentDevice()->GetPhysicalDevice();

		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
			if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
				return i;
			}
		}

		FROST_ASSERT(0, "Failed to find suitable memory type!");
		return 0;
	}
#endif

#endif 
}