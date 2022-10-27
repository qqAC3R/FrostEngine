#include "frostpch.h"
#include <Frost.h>
#include <Frost/Core/EntryPoint.h>

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

	Application* CreateApplication()
	{
		return new EditorApp();
	}
}