#include "frostpch.h"
#include "RenderCommandQueue.h"

namespace Frost
{

#if 0
	RenderCommandQueue::RenderCommandQueue()
	{
		//m_CommandBuffer.reserve(1024);
	}

	void RenderCommandQueue::Allocate(std::function<void()> func)
	{
		m_CommandBuffer.push(func);
	}

	void RenderCommandQueue::Execute()
	{
		uint32_t commandBufferSize = (uint32_t)m_CommandBuffer.size();
		for (uint32_t i = 0; i < commandBufferSize; i++)
		{
			auto func = m_CommandBuffer.front();
			func();
			m_CommandBuffer.pop();
		}
		//m_CommandBuffer.clear();
	}

#else
	RenderCommandQueue::RenderCommandQueue()
	{
		m_CommandBuffer = new uint8_t[10 * 1024 * 1024]; // 10mb buffer
		m_CommandBufferPtr = m_CommandBuffer;
		memset(m_CommandBuffer, 0, 10 * 1024 * 1024);
	}

	RenderCommandQueue::~RenderCommandQueue()
	{
		delete[] m_CommandBuffer;
	}

	void* RenderCommandQueue::Allocate(RenderCommandFn fn, uint32_t size)
	{
		// TODO: alignment
		*(RenderCommandFn*)m_CommandBufferPtr = fn;
		m_CommandBufferPtr += sizeof(RenderCommandFn);

		*(uint32_t*)m_CommandBufferPtr = size;
		m_CommandBufferPtr += sizeof(uint32_t);

		void* memory = m_CommandBufferPtr;
		m_CommandBufferPtr += size;

		m_CommandCount++;
		return memory;
	}

	void RenderCommandQueue::Execute()
	{
		//HZ_RENDER_TRACE("RenderCommandQueue::Execute -- {0} commands, {1} bytes", m_CommandCount, (m_CommandBufferPtr - m_CommandBuffer));

		Byte* buffer = m_CommandBuffer;

		for (uint32_t i = 0; i < m_CommandCount; i++)
		{
			RenderCommandFn function = *(RenderCommandFn*)buffer;
			buffer += sizeof(RenderCommandFn);

			uint32_t size = *(uint32_t*)buffer;
			buffer += sizeof(uint32_t);
			function(buffer);
			buffer += size;
		}

		m_CommandBufferPtr = m_CommandBuffer;
		m_CommandCount = 0;
	}
#endif

}