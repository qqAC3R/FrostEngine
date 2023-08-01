#pragma once

#include <imgui.h>
#include <imgui_internal.h>
#include "Frost/Core/Application.h"

namespace ImGui
{
	class ScopedStyle
	{
	public:
		ScopedStyle(ImGuiCol style, const ImVec4& vec)
		{
			ImGui::PushStyleColor(style, vec);
			m_Type = ScopedStyleType::StyleColor;
		}

		ScopedStyle(ImGuiCol style, const ImU32& col)
		{
			ImGui::PushStyleColor(style, ImGui::ColorConvertU32ToFloat4(col));
			m_Type = ScopedStyleType::StyleColor;
		}

		ScopedStyle(ImGuiStyleVar style, float value)
		{
			ImGui::PushStyleVar(style, value);
			m_Type = ScopedStyleType::StyleVar;
		}

		ScopedStyle(ImGuiStyleVar style, const ImVec2& value)
		{
			ImGui::PushStyleVar(style, value);
			m_Type = ScopedStyleType::StyleVar;
		}

		~ScopedStyle()
		{
			switch (m_Type)
			{
			case ScopedStyle::ScopedStyleType::StyleColor: ImGui::PopStyleColor(); break;
			case ScopedStyle::ScopedStyleType::StyleVar: ImGui::PopStyleVar(); break;
			}
		}
	private:
		enum class ScopedStyleType
		{
			StyleColor,
			StyleVar,
		};
		ScopedStyleType m_Type;
	};

	class ScopedFontStyle
	{
	public:
		enum class FontType
		{
			Regular, Bold
		};

		ScopedFontStyle(FontType type)
			: m_Type(type)
		{
			Frost::ImGuiLayer* imguiLayer = Frost::Application::Get().GetImGuiLayer();

			if (type == FontType::Bold)
				imguiLayer->SetBoldFont();
		}

		~ScopedFontStyle()
		{
			Frost::ImGuiLayer* imguiLayer = Frost::Application::Get().GetImGuiLayer();

			if (m_Type == FontType::Bold)
				imguiLayer->SetRegularFont();
		}
	private:
		FontType m_Type;
	};

}