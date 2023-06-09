#include "frostpch.h"
#include "PhysicsMaterialEditor.h"

#include "UserInterface/UIWidgets.h"

#include <imgui.h>

namespace Frost
{
	void PhysicsMaterialEditor::Init(void* initArgs)
	{

	}

	void PhysicsMaterialEditor::OnEvent(Event& e)
	{

	}

	void PhysicsMaterialEditor::Render()
	{
		if (m_Visibility && m_ActivePhysicsMaterial)
		{
			ImGui::Begin("Physics Material Editor", &m_Visibility);

			std::string physicsMaterialName = "Physics Material Name: " + m_ActivePhysicsMaterial->m_MaterialName;
			UserInterface::Text("Physics Material Name: ");

			UserInterface::DragFloat("Static Friction", m_ActivePhysicsMaterial->StaticFriction, 0.01f, 0.0f, 100.0f);
			UserInterface::DragFloat("Dynamic Friction", m_ActivePhysicsMaterial->DynamicFriction, 0.01f, 0.0f, 100.0f);
			UserInterface::DragFloat("Bounciness", m_ActivePhysicsMaterial->Bounciness, 0.01f, 0.0f, 100.0f);

			ImGui::End();
		}
	}

	void PhysicsMaterialEditor::Shutdown()
	{

	}
}