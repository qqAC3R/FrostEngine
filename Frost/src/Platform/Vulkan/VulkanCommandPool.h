#pragma once

#include <vulkan/vulkan.hpp>

namespace Frost
{
	class VulkanCommandBuffer;

	class VulkanCommandPool
	{

	public:
		VulkanCommandPool();
		virtual ~VulkanCommandPool();

		void Destroy();

		VkCommandPool GetCommandPool() { return m_CommandPool; }

		VulkanCommandBuffer CreateCommandBuffer(bool beginRecording = false);

	private:
		VkCommandPool m_CommandPool;

	};

}