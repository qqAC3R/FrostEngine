#include "frostpch.h"
#include <Frost.h>

#include "EditorLayer.h"

namespace Frost
{
	class EditorApp : public Application
	{
	public:
		EditorApp()
		{
			PushLayer(new EditorLayer());
		}

		~EditorApp()
		{
		}
	};
}

Frost::Application* Frost::CreateApplication()
{
	return new EditorApp();
}