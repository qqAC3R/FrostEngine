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
			case ShaderDataType::UInt:      return VK_FORMAT_R32_UINT;
			case ShaderDataType::Bool:      return VK_FORMAT_R32_UINT;
		}

		FROST_ASSERT(false, "Unknown ShaderDataType!");
		return VK_FORMAT_R32G32B32A32_SFLOAT;
	}

	static uint32_t GetLocationOffset(const BufferElement& element)
	{
		switch (element.Type)
		{
			case ShaderDataType::Mat3: return 3;
			case ShaderDataType::Mat4: return 4;
			default:				   return 0;
		}
		return 0;
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

			uint32_t index = 0;
			//for (uint32_t i = 0; i < bufferLayout.m_BufferElements.size(); i++)
			for (auto& element : bufferLayout.m_BufferElements)
			{
				/* We use an offset because the vulkan spec says:

				if we are using a n*m matrix, then it will be assigned multiple locations starting with the location specified.
				The number of locations assigned for each matrix will be the same as for an n-element array of m-component vectors. */
				if (element.Type == ShaderDataType::Mat3 || element.Type == ShaderDataType::Mat4)
				{
					uint32_t matrixSize = GetLocationOffset(element);
					uint32_t matrixOffset = 0;

					for (uint32_t i = 0; i < matrixSize; i++)
					{
						VkVertexInputAttributeDescription attributeDescription;
						attributeDescription.binding = 0;
						attributeDescription.location = index;
						attributeDescription.format = ShaderDataTypeFormat(element.Type);
						attributeDescription.offset = element.Offset + matrixOffset;

						attributeDescriptions.push_back(attributeDescription);
						
						index++;
						matrixOffset += matrixSize * sizeof(float);
					}

					continue;
				}

				VkVertexInputAttributeDescription attributeDescription;
				attributeDescription.binding = 0;
				attributeDescription.location = index;
				attributeDescription.format = ShaderDataTypeFormat(element.Type);
				attributeDescription.offset = element.Offset;

				attributeDescriptions.push_back(attributeDescription);

				index++;
			}

			return attributeDescriptions;
		}

	}
}