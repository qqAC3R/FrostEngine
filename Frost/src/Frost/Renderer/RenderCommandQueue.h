#pragma once

namespace Frost
{
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
}