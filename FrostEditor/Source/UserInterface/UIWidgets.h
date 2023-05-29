#pragma once

#include <glm/glm.hpp>

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

		static bool PropertyEntityReference(const char* label, Entity& entity);
	};

}