#include "frostpch.h"
#include <Frost.h>

#include "EditorLayer.h"

namespace Frost
{

	class Sandbox : public Application
	{
	public:
		Sandbox()
		{
			PushLayer(new EditorLayer());
		}

		~Sandbox()
		{

		}
	};
}

Frost::Application* Frost::CreateApplication()
{
	return new Sandbox();
}