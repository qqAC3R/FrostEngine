#pragma once

namespace Frost
{
	class FunctionQueue
	{
	public:
		FunctionQueue() = default;
		~FunctionQueue() { m_Queue.clear(); }

		template <typename Func>
		void AddFunction(Func&& function)
		{
			m_Queue.push_back(function);
		}

		void Execute()
		{
			for (auto&& func : m_Queue)
			{
				func();
			}
			m_Queue.clear();
		}
	private:
		std::vector<std::function<void()>> m_Queue; // Using std::vector instead of std::queue due to the performance
	};
}