#include "frostpch.h"
#include "RenderCommandQueue.h"

namespace Frost
{

	RenderCommandQueue::RenderCommandQueue()
	{
		m_CommandBuffer.reserve(1024);
	}

	void RenderCommandQueue::Allocate(std::function<void()> func)
	{
		m_CommandBuffer.push_back(func);
	}

	void RenderCommandQueue::Execute()
	{
		uint32_t commandBufferSize = (uint32_t)m_CommandBuffer.size();
		for (uint32_t i = 0; i < commandBufferSize; i++)
		{
			auto func = m_CommandBuffer[i];
			func();
		}
		m_CommandBuffer.clear();
	}

}