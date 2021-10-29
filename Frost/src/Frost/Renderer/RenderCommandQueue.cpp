#include "frostpch.h"
#include "RenderCommandQueue.h"

namespace Frost
{
	RenderCommandQueue::RenderCommandQueue()
	{
		// Allocate 10Mb buffer
		m_CommandBuffer = new uint8_t[10 * 1024 * 1024];
		m_CommandBufferPtr = m_CommandBuffer;

		// Setting all the memory to `0`
		memset(m_CommandBuffer, 0, 10 * 1024 * 1024);
	}

	RenderCommandQueue::~RenderCommandQueue()
	{
		delete[] m_CommandBuffer;
	}

	void* RenderCommandQueue::Allocate(RenderCommandFn fn, uint32_t size)
	{
		///////////////////////////////////// ALIGNMENT /////////////////////////////////////////////
		//                 ||----------------------------------------------------------------------||
		// What we store:  ||         FUNCTION         |  SIZE OF THE FUNCTION   |   VOID POINTER  ||
		// Memory size:    ||  sizeof(RenderCommandFn) |    sizeof(uint32_t)	 |   sizeof(void*) ||
		// Memory size:    ||          N bytes         |         4 bytes      	 |      1 byte     ||
		//                 ||----------------------------------------------------------------------||
		/////////////////////////////////////////////////////////////////////////////////////////////

		// We move the pointer n bytes (n = sizeof(RenderCommandFn)) - for later allocating the std::function<void()>
		*(RenderCommandFn*)m_CommandBufferPtr = fn;
		m_CommandBufferPtr += sizeof(RenderCommandFn);

		// We add at the end the size of the RenderCommandFn
		*(uint32_t*)m_CommandBufferPtr = size;
		m_CommandBufferPtr += sizeof(uint32_t);

		// We allocate another byte for knowing the address
		// (which we later give to the std::function constructor to store the information of the function)
		void* memory = m_CommandBufferPtr;
		m_CommandBufferPtr += size;

		// Increase the command count and return the address for the specific function
		m_CommandCount++;
		return memory;
	}

	void RenderCommandQueue::Execute()
	{
		// Pointer to the first function
		Byte* buffer = m_CommandBuffer;

		for (uint32_t i = 0; i < m_CommandCount; i++)
		{
			// Move the pointer to n bytes (n = sizeof(RenderCommandFn))
			RenderCommandFn function = *(RenderCommandFn*)buffer;
			buffer += sizeof(RenderCommandFn);

			// Move the pointer 4 bytes
			uint32_t size = *(uint32_t*)buffer;
			buffer += sizeof(uint32_t);

			// Execute the function
			function(buffer);

			// Move 1 byte (because we allocated a void* for the address)
			buffer += size;
		}

		// After finishing executing the functions, change pointer's address to the beginning of the buffer / And set the commandCount to 0
		m_CommandBufferPtr = m_CommandBuffer;
		m_CommandCount = 0;
	}
}