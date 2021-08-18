#pragma once

#include "frostpch.h"

namespace Frost
{
	enum class ShaderDataType
	{
		None = 0, Float, Float2, Float3, Float4, Mat3, Mat4, Int, Int2, Int3, Int4, Bool
	};

	static uint32_t ShaderDataTypeSize(ShaderDataType type)
	{
		switch (type)
		{
		case ShaderDataType::Float:     return 4;
		case ShaderDataType::Float2:    return 4 * 2;
		case ShaderDataType::Float3:    return 4 * 3;
		case ShaderDataType::Float4:    return 4 * 4;
		case ShaderDataType::Mat3:      return 4 * 3 * 3;
		case ShaderDataType::Mat4:      return 4 * 4 * 4;
		case ShaderDataType::Int:       return 4;
		case ShaderDataType::Int2:      return 4 * 2;
		case ShaderDataType::Int3:      return 4 * 3;
		case ShaderDataType::Int4:      return 4 * 4;
		case ShaderDataType::Bool:      return 1;
		}

		FROST_ASSERT(false, "Unknown ShaderDataType!");
		return 0;
	}

	struct BufferElement
	{
		std::string Name;
		ShaderDataType Type;
		uint32_t Offset;

		BufferElement(const std::string& name, ShaderDataType type)
			: Name(name), Type(type)
		{
		}

	};

	class BufferLayout
	{
	public:
		BufferLayout() = default;
		BufferLayout(const std::initializer_list<BufferElement>& elements)
			: m_BufferElements(elements)
		{
			CalculateOffset();
		}


		void CalculateOffset()
		{
			uint32_t offset = 0;
			for (auto& element : m_BufferElements)
			{
				element.Offset = offset;
				offset += ShaderDataTypeSize(element.Type);
			}
			m_Size = offset;
		}

		std::vector<BufferElement> m_BufferElements;
		uint32_t m_Size;

	};

}