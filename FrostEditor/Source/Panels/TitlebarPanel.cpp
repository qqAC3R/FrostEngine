#include "frostpch.h"
#include "TitlebarPanel.h"

#include "Frost/Core/Application.h"
#include "Frost/Renderer/Renderer.h"

#include <imgui.h>
#include <imgui_internal.h>

#include "UserInterface/ImGuiUtilities.h"

namespace Frost
{
	void TitlebarPanel::Init(void* initArgs)
	{
		m_LogoTexture = Texture2D::Create("Resources/Editor/Logo_Frost.png");
		m_MinimizeButtonTex = Texture2D::Create("Resources/Editor/window_minimize.png");
		m_MaximizeButtonTex = Texture2D::Create("Resources/Editor/window_maximize.png");
		m_CloseButtonTex = Texture2D::Create("Resources/Editor/window_close.png");
	}

	void TitlebarPanel::OnEvent(Event& e)
	{

	}

	void TitlebarPanel::Render()
	{
		UIManualResize();
		RenderTitleBar();
		ImGui::SetCursorPosY(m_TitlebarHeight + ImGui::GetCurrentWindow()->WindowPadding.y);
	}

	void TitlebarPanel::Shutdown()
	{

	}

	void TitlebarPanel::RenderTitleBar()
	{
		ImGuiLayer* imguiLayer = Application::Get().GetImGuiLayer();
		const ImVec2 windowPadding = ImGui::GetCurrentWindow()->WindowPadding;

		

		// Render Titlebar Box
		ImGui::SetCursorPos(ImVec2(windowPadding.x, windowPadding.y));
		const ImVec2 titlebarMin = ImGui::GetCursorScreenPos();
		const ImVec2 titlebarMax = { ImGui::GetCursorScreenPos().x + ImGui::GetWindowWidth() - windowPadding.y * 2.0f, ImGui::GetCursorScreenPos().y + m_TitlebarHeight };

		auto* drawList = ImGui::GetWindowDrawList();
		drawList->AddRectFilled(titlebarMin, titlebarMax, IM_COL32(31, 36, 43, 255));

		// Render Logo
		RenderEngineLogo();

		// Handle Window Dragging
		HandleTitleBarDragZone();

		// Draw Menubar
		ImGui::SetItemAllowOverlap();
		RenderMenuBar();

		// Draw Minimze/Maximize/Close buttons
		RenderButtons();

		m_MenuBarFunction = {};
	}

	void TitlebarPanel::RenderButtons()
	{
		ImGuiLayer* imguiLayer = Application::Get().GetImGuiLayer();
		const ImVec2 windowPadding = ImGui::GetCurrentWindow()->WindowPadding;

		// Button transparent Style
		ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
		ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(31, 36, 43, 255));


		// Minimize button
		ImGui::SetCursorPos(ImVec2(ImGui::GetWindowSize().x - 110.0f - windowPadding.x, windowPadding.y + 20.0f));
		ImTextureID minimizeButtonTexID = imguiLayer->GetImGuiTextureID(m_MinimizeButtonTex->GetImage2D());
		ImGui::Image(minimizeButtonTexID, { 14.0f, 2.0f });

		ImGui::SetCursorPos(ImVec2(ImGui::GetWindowSize().x - 110.0f - windowPadding.x, windowPadding.y + 10.0f));
		if (ImGui::InvisibleButton("##MinimizeButton", { 14.0f, 14.0f }))
		{
			// TODO: move this stuff to a better place, like Window class
			if (auto* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow()))
			{
				Renderer::SubmitDeletion([]()
				{
					Application::Get().GetWindow().MinimizeWindow();
				});
			}
		}


		// Maximize button
		ImGui::SetCursorPos(ImVec2(ImGui::GetWindowSize().x - (110 - 40) - windowPadding.x, windowPadding.y + 10.0f));
		ImTextureID maximizeButtonTexID = imguiLayer->GetImGuiTextureID(m_MaximizeButtonTex->GetImage2D());
		ImGui::Image(maximizeButtonTexID, { 14.0f, 14.0f });

		ImGui::SetCursorPos(ImVec2(ImGui::GetWindowSize().x - (110 - 40) - windowPadding.x, windowPadding.y + 10.0f));
		if (ImGui::InvisibleButton("##MaximizeButton", { 14.0f, 14.0f }))
		{
			auto* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
			bool isMaximized = Application::Get().GetWindow().IsMaximized();


			Renderer::SubmitDeletion([isMaximized]()
			{
				if (isMaximized)
				{
					Application::Get().GetWindow().RestoreWindow();
				}
				else
				{
					Application::Get().GetWindow().MaximizeWindow();
				}
			});
		}


		// Close button
		ImGui::SetCursorPos(ImVec2(ImGui::GetWindowSize().x - (110 - 80) - windowPadding.x, windowPadding.y + 10.0f));
		ImTextureID closeButtonTexID = imguiLayer->GetImGuiTextureID(m_CloseButtonTex->GetImage2D());
		ImGui::Image(closeButtonTexID, { 14.0f, 14.0f });

		ImGui::SetCursorPos(ImVec2(ImGui::GetWindowSize().x - (110 - 80) - windowPadding.x, windowPadding.y + 10.0f));
		if (ImGui::InvisibleButton("##CloseButton", { 14.0f, 14.0f }))
		{
			auto* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
			bool isMaximized = Application::Get().GetWindow().IsMaximized();

			Application::Get().Close();
		}

		// Button transparent Style (close)
		ImGui::PopStyleVar(3);
		ImGui::PopStyleColor();
	}

	void TitlebarPanel::RenderMenuBar()
	{
		const ImRect menuBarRect = { ImGui::GetCursorPos(), {ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeightWithSpacing()} };

		ImGui::SetCursorPos(ImVec2(70.0f, 3.0f));

		ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
		ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(31, 36, 43, 255));

		m_MenuBarFunction();

		ImGui::PopStyleVar(3);
		ImGui::PopStyleColor();
	}

	void TitlebarPanel::RenderEngineLogo()
	{
		ImGuiLayer* imguiLayer = Application::Get().GetImGuiLayer();
		const ImVec2 windowPadding = ImGui::GetCurrentWindow()->WindowPadding;
		auto* drawList = ImGui::GetWindowDrawList();


		const int logoWidth = 50;
		const int logoHeight = 50;

		const ImVec2 logoOffset(2.0f + windowPadding.x, 2.0f + windowPadding.y);
		const ImVec2 logoRectStart = {
			ImGui::GetItemRectMin().x + logoOffset.x,
			ImGui::GetItemRectMin().y + logoOffset.y
		};
		const ImVec2 logoRectMax = { logoRectStart.x + logoWidth, logoRectStart.y + logoHeight };

		ImGui::SetCursorPos(ImVec2(windowPadding.x + 10.0f, windowPadding.y + 10.0f));

		drawList->AddImage(imguiLayer->GetImGuiTextureID(m_LogoTexture->GetImage2D()), logoRectStart, logoRectMax);
	}

	void TitlebarPanel::HandleTitleBarDragZone()
	{
		const ImVec2 windowPadding = ImGui::GetCurrentWindow()->WindowPadding;

		static float moveOffsetX = 0.0f;
		static float moveOffsetY = 0.0f;
		const float w = ImGui::GetContentRegionAvail().x;


		auto* rootWindow = ImGui::GetCurrentWindow()->RootWindow;
		const float windowWidth = (int)rootWindow->RootWindow->Size.x;


		ImGui::SetCursorPos(ImVec2(windowPadding.x, windowPadding.y));
		if (ImGui::InvisibleButton("##titleBarDragZone", ImVec2(w, m_TitlebarHeight), ImGuiButtonFlags_PressedOnClick))
		{
			ImVec2 point = ImGui::GetMousePos();
			ImRect rect = rootWindow->Rect();
			// Calculate the difference between the cursor pos and window pos
			moveOffsetX = point.x - rect.Min.x;
			moveOffsetY = point.y - rect.Min.y;
		}

		if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered())
		{
			auto* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
			bool maximized = Application::Get().GetWindow().IsMaximized();

			Renderer::SubmitDeletion([maximized]()
			{
				if (maximized)
					Application::Get().GetWindow().RestoreWindow();
				else
					Application::Get().GetWindow().MaximizeWindow();
			});
		}
		if (ImGui::IsItemActive())
		{
			if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
			{
				Renderer::SubmitDeletion([windowWidth]()
				{
					auto* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
					int maximized = Application::Get().GetWindow().IsMaximized();
					if (maximized)
					{
						//glfwRestoreWindow(window);
						Application::Get().GetWindow().RestoreWindow();

						int newWidth, newHeight;
						newWidth = Application::Get().GetWindow().GetWidth();
						newHeight = Application::Get().GetWindow().GetHeight();

						// Offset position proportionally to mouse position on titlebar
						// This ensures we dragging window relatively to cursor position on titlebar
						// correctly when window size changes
						if (windowWidth - (float)newWidth > 0.0f)
							moveOffsetX *= (float)newWidth / windowWidth;
					}

					ImVec2 point = ImGui::GetMousePos();
					Application::Get().GetWindow().SetWindowPosition(point.x - moveOffsetX, point.y - moveOffsetY);
				});
			}
		}
	}

}