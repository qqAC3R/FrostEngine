#pragma once

#include "Frost/Events/Event.h"
#include "Frost/Core/Timestep.h"

namespace Frost
{

	class Layer
	{
	public:
		Layer(const std::string& name = "Layer");
		virtual ~Layer();

		virtual void OnAttach() {}
		virtual void OnDetach() {}
		virtual void OnUpdate(Timestep ts) {}
		virtual void OnEvent(Event& event) {}
		virtual void OnResize(uint32_t width, uint32_t height) {}
		virtual void OnImGuiRender() {}

		inline const std::string& GetName() const { return m_DebugName; }
		inline void BlockEvents(bool boolBlock) { m_BlockEvents = boolBlock; }
		inline bool IsBlocking() const { return m_BlockEvents; }

	private:
		std::string m_DebugName;
		bool m_BlockEvents = true;
	};

}