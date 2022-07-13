#include "frostpch.h"
#include "ViewportPanel.h"

#include "Frost/ImGui/ImGuiLayer.h"

#include "Frost/Core/Application.h"
#include "Frost/Core/Input.h"
#include "Frost/InputCodes/MouseButtonCodes.h"
#include <imgui.h>

namespace Frost
{
	void ViewportPanel::Init(void* initArgs)
	{
	}

	void ViewportPanel::OnEvent(Event& e)
	{

	}

	void ViewportPanel::Render()
	{
	}

	void ViewportPanel::BeginRender()
	{
		// Begin the viewport panel
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
		ImGui::Begin("Viewport");

		// Check if the viewport is focused
		bool IsViewPortFocused = ImGui::IsWindowFocused();
		Application::Get().GetImGuiLayer()->BlockEvents(!IsViewPortFocused);

		// Check if the viewport is resized
		ImVec2 viewPortSizePanel = ImGui::GetContentRegionAvail();
		if (m_ViewportSize.x != viewPortSizePanel.x || m_ViewportSize.y != viewPortSizePanel.y)
		{
			//m_IsResized = true;

			m_IsResized = false;
			m_IsResized_Internal = true;

			m_ViewportSize = *(glm::vec2*)&viewPortSizePanel;
		}
		else
		{
			//if (m_IsResized)
			//	m_WasResized = true;
			//else
			//	m_WasResized = false;

			//if (!Input::IsMouseButtonPressed(Mouse::Button1))
			if(m_IsResized_Internal && !Input::IsMouseButtonPressed(Mouse::Button0))
			{
				m_IsResized = true;
				m_IsResized_Internal = false;
			}
			else
			{
				m_IsResized = false;
			}

		}
	}

	void ViewportPanel::RenderTexture(Ref<Image2D> texture)
	{
		ImGuiLayer* imguiLayer = Application::Get().GetImGuiLayer();
		imguiLayer->RenderTexture(texture, m_ViewportSize.x, m_ViewportSize.y);
	}

	void ViewportPanel::EndRender()
	{
		// End the viewport panel rendering
		ImGui::End();
		ImGui::PopStyleVar();
	}

	void ViewportPanel::Shutdown()
	{
	}
}