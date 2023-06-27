#pragma once

#include "Frost/Events/Event.h"

namespace Frost
{
	class Panel
	{
	public:
		//Panel() = default;
		virtual ~Panel() = default;

		virtual void Init(void* initArgs) = 0;
		virtual void OnEvent(Event& e) = 0;
		virtual void Render() = 0;
		virtual void SetVisibility(bool show) = 0;
		virtual void Shutdown() = 0;
	};
}