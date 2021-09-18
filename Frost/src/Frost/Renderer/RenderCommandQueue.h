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
		Vector<std::function<void()>> m_CommandBuffer;
	};
}