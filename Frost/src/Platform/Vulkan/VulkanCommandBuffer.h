#pragma once

#include "VulkanCommandPool.h"

namespace Frost
{

	class VulkanCommandBuffer
	{
	public:
		VulkanCommandBuffer();
		virtual ~VulkanCommandBuffer();

		void Allocate(VkCommandPool cmdPool);
		void Destroy();

		void Begin();
		void End();

		VkCommandBuffer GetCommandBuffer() const { return m_CmdBuf; }
		
		static VulkanCommandBuffer BeginSingleTimeCommands(VulkanCommandPool& cmdPool);
		static void EndSingleTimeCommands(VulkanCommandBuffer& cmdBuf);
		

	private:
		VkCommandBuffer m_CmdBuf;
		VkCommandPool m_CachedCmdPool;
	};

}