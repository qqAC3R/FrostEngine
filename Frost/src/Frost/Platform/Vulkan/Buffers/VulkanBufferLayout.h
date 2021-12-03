#pragma once

#include "Frost/Platform/Vulkan/Vulkan.h"

#include "frostpch.h"
#include "Frost/Renderer/Buffers/BufferLayout.h"

namespace Frost
{

	static VkFormat ShaderDataTypeFormat(ShaderDataType type)
	{
		switch (type)
		{
			case ShaderDataType::Float:     return VK_FORMAT_R32_SFLOAT;
			case ShaderDataType::Float2:    return VK_FORMAT_R32G32_SFLOAT;
			case ShaderDataType::Float3:    return VK_FORMAT_R32G32B32_SFLOAT;
			case ShaderDataType::Float4:    return VK_FORMAT_R32G32B32A32_SFLOAT;
			case ShaderDataType::Mat3:      return VK_FORMAT_R32G32B32_SFLOAT;
			case ShaderDataType::Mat4:      return VK_FORMAT_R32G32B32A32_SFLOAT;
			case ShaderDataType::Int:       return VK_FORMAT_R32_SINT;
			case ShaderDataType::Int2:      return VK_FORMAT_R32G32_SINT;
			case ShaderDataType::Int3:      return VK_FORMAT_R32G32B32_SINT;
			case ShaderDataType::Int4:      return VK_FORMAT_R32G32B32A32_SINT;
			case ShaderDataType::Bool:      return VK_FORMAT_R32_UINT;
		}

		FROST_ASSERT(false, "Unknown ShaderDataType!");
		return VK_FORMAT_R32G32B32A32_SFLOAT;
	}

	namespace Utils
	{

		VkVertexInputBindingDescription GetVertexBindingDescription(const BufferLayout& bufferLayout)
		{
			VkVertexInputBindingDescription bidingDescription{};
			bidingDescription.binding = 0;
			bidingDescription.stride = bufferLayout.m_Size;
			switch (bufferLayout.m_InputType)
			{
				case InputType::Vertex:     bidingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;   break;
				case InputType::Instanced:  bidingDescription.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE; break;
				case InputType::None:       bidingDescription.inputRate = VK_VERTEX_INPUT_RATE_MAX_ENUM; break;
			}

			return bidingDescription;
		}

		Vector<VkVertexInputAttributeDescription> GetVertexAttributeDescriptions(const BufferLayout& bufferLayout)
		{
			Vector<VkVertexInputAttributeDescription> attributeDescriptions{};

			for (uint32_t i = 0; i < bufferLayout.m_BufferElements.size(); i++)
			{
				VkVertexInputAttributeDescription attributeDescription;
				attributeDescription.binding = 0;
				attributeDescription.location = i;
				attributeDescription.format = ShaderDataTypeFormat(bufferLayout.m_BufferElements[i].Type);
				attributeDescription.offset = bufferLayout.m_BufferElements[i].Offset;

				attributeDescriptions.push_back(attributeDescription);
			}

			return attributeDescriptions;
		}

	}
}