#pragma once

namespace Frost
{
#if 0
	class RenderCommandQueue
	{
	public:
		RenderCommandQueue();
		virtual ~RenderCommandQueue() {}

		void Allocate(std::function<void()> func);
		void Execute();
	private:
		std::queue<std::function<void()>> m_CommandBuffer;
	};
#else

	class RenderCommandQueue
	{
	public:
		typedef void(*RenderCommandFn)(void*);

		RenderCommandQueue();
		~RenderCommandQueue();

		void* Allocate(RenderCommandFn func, uint32_t size);

		void Execute();
	private:
		uint8_t* m_CommandBuffer;
		uint8_t* m_CommandBufferPtr;
		uint32_t m_CommandCount = 0;
	};
#endif

}