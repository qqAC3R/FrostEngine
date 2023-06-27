#pragma once


#include <glm/glm.hpp>

#include "Frost/Core/Application.h"
#include "Frost/Renderer/Texture.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

//typedef int ImGuiCol;
//typedef int ImGuiStyleVar;
//typedef unsigned int ImU32;
//struct ImVec4;
//struct ImVec2;

namespace Frost
{
	class Entity;

	class UserInterface
	{
	public:
		static void Text(const std::string& text);

		static void SliderFloat(const std::string& name, float& value, float min, float max);
		static void DragFloat(const std::string& name, float& value, float speed, float minValue = FLT_MIN, float maxValue = FLT_MAX);

		static void DrawVec3ColorEdit(const std::string& name, glm::vec3& values);
		static void DrawVec4ColorEdit(const std::string& name, glm::vec4& values);

		static void DrawVec3CoordsEdit(const std::string& name, glm::vec3& values, float resetValue = 0.0f, float columnWidth = 100.0f);

		static std::string DrawFilePath(const std::string& label, const std::string& textBox, const std::string& format, float columnWidth = 100.0f);

		static void CheckBox(const std::string& label, bool& value);

		static bool InputText(const std::string& label, const char* buffer);
		static bool InputTextWithDragDrop(const std::string& label, const char* buffer);

		static bool PropertyPrefabReference(const char* label, const char* prefabName);
		static bool PropertyEntityReference(const char* label, Entity& entity);

		static bool PropertyMultiline(const char* label, std::string& value);
		
		static void InvisibleButtonWithNoBehaivour(const char* str_id, const ImVec2& size_arg);

		class ScopedStyle
		{
		public:
			ScopedStyle(ImGuiCol style, const ImVec4& vec);
			ScopedStyle(ImGuiCol style, const ImU32& col);
			ScopedStyle(ImGuiStyleVar style, float value);
			ScopedStyle(ImGuiStyleVar style, const ImVec2& value);
			~ScopedStyle();
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

			ScopedFontStyle(FontType type);
			~ScopedFontStyle();

		private:
			FontType m_Type;
		};

		static void ShiftCursorX(float x);
		static void ShiftCursorY(float y);

		//=========================================================================================
		/// Button Image

		static void DrawButtonImage(Ref<Texture2D> imageNormal, Ref<Texture2D> imageHovered, Ref<Texture2D> imagePressed,
			ImU32 tintNormal, ImU32 tintHovered, ImU32 tintPressed,
			ImVec2 rectMin, ImVec2 rectMax);

		static void DrawButtonImage(Ref<Texture2D> imageNormal, Ref<Texture2D> imageHovered, Ref<Texture2D> imagePressed,
			ImU32 tintNormal, ImU32 tintHovered, ImU32 tintPressed,
			ImRect rectangle)
		{
			DrawButtonImage(imageNormal, imageHovered, imagePressed, tintNormal, tintHovered, tintPressed, rectangle.Min, rectangle.Max);
		};

		static void DrawButtonImage(Ref<Texture2D> image,
			ImU32 tintNormal, ImU32 tintHovered, ImU32 tintPressed,
			ImVec2 rectMin, ImVec2 rectMax)
		{
			DrawButtonImage(image, image, image, tintNormal, tintHovered, tintPressed, rectMin, rectMax);
		};

		static void DrawButtonImage(Ref<Texture2D> image,
			ImU32 tintNormal, ImU32 tintHovered, ImU32 tintPressed,
			ImRect rectangle)
		{
			DrawButtonImage(image, image, image, tintNormal, tintHovered, tintPressed, rectangle.Min, rectangle.Max);
		};


		static void DrawButtonImage(Ref<Texture2D> imageNormal, Ref<Texture2D> imageHovered, Ref<Texture2D> imagePressed,
			ImU32 tintNormal, ImU32 tintHovered, ImU32 tintPressed)
		{
			DrawButtonImage(imageNormal, imageHovered, imagePressed, tintNormal, tintHovered, tintPressed, ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
		};

		static void DrawButtonImage(Ref<Texture2D> image,
			ImU32 tintNormal, ImU32 tintHovered, ImU32 tintPressed)
		{
			DrawButtonImage(image, image, image, tintNormal, tintHovered, tintPressed, ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
		};

		/// Inner Shadow Draw
		static void DrawShadowInner(const Ref<Texture2D>& shadowImage, int radius, ImVec2 rectMin, ImVec2 rectMax, float alpha = 1.0f, float lengthStretch = 10.0f,
			bool drawLeft = true, bool drawRight = true, bool drawTop = true, bool drawBottom = true);

		static void DrawShadowInner(const Ref<Texture2D>& shadowImage, int radius, ImRect rectangle, float alpha = 1.0f, float lengthStretch = 10.0f,
			bool drawLeft = true, bool drawRight = true, bool drawTop = true, bool drawBottom = true)
		{
			DrawShadowInner(shadowImage, radius, rectangle.Min, rectangle.Max, alpha, lengthStretch, drawLeft, drawRight, drawTop, drawBottom);
		};


		static void DrawShadowInner(const Ref<Texture2D>& shadowImage, int radius, float alpha = 1.0f, float lengthStretch = 10.0f,
			bool drawLeft = true, bool drawRight = true, bool drawTop = true, bool drawBottom = true)
		{
			DrawShadowInner(shadowImage, radius, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), alpha, lengthStretch, drawLeft, drawRight, drawTop, drawBottom);
		};
	};



}