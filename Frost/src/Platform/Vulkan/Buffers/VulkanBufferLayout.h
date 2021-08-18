#pragma once

#include <vulkan/vulkan.hpp>

#include "frostpch.h"
#include "Frost/Renderer/Buffers/BufferLayout.h"

namespace Frost
{
	//enum class ShaderDataType
	//{
	//	None = 0, Float, Float2, Float3, Float4, Mat3, Mat4, Int, Int2, Int3, Int4, Bool
	//};

	//static uint32_t ShaderDataTypeSize(ShaderDataType type)
	//{
	//	switch (type)
	//	{
	//		case ShaderDataType::Float:     return 4;
	//		case ShaderDataType::Float2:    return 4 * 2;
	//		case ShaderDataType::Float3:    return 4 * 3;
	//		case ShaderDataType::Float4:    return 4 * 4;
	//		case ShaderDataType::Mat3:      return 4 * 3 * 3;
	//		case ShaderDataType::Mat4:      return 4 * 4 * 4;
	//		case ShaderDataType::Int:       return 4;
	//		case ShaderDataType::Int2:      return 4 * 2;
	//		case ShaderDataType::Int3:      return 4 * 3;
	//		case ShaderDataType::Int4:      return 4 * 4;
	//		case ShaderDataType::Bool:      return 1;
	//	}
	//
	//	FROST_ASSERT(false, "Unknown ShaderDataType!");
	//	return 0;
	//}

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

	//struct BufferElement
	//{
	//	std::string Name;
	//	ShaderDataType Type;
	//	uint32_t Offset;
	//
	//	BufferElement(const std::string& name, ShaderDataType type)
	//		: Name(name), Type(type)
	//	{
	//	}
	//
	//};

	namespace VulkanUtils
	{

		VkVertexInputBindingDescription GetVertexBindingDescription(const BufferLayout& bufferLayout, VkVertexInputRate inputStage)
		{
			VkVertexInputBindingDescription bidingDescription{};
			bidingDescription.binding = 0;
			bidingDescription.stride = bufferLayout.m_Size;
			bidingDescription.inputRate = inputStage;

			return bidingDescription;
		}

		std::vector<VkVertexInputAttributeDescription> GetVertexAttributeDescriptions(const BufferLayout& bufferLayout)
		{
			std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};

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

	/*
	class VulkanBufferLayout
	{
	public:
		VulkanBufferLayout() = default;
		VulkanBufferLayout(const std::initializer_list<BufferElement>& elements, VkVertexInputRate inputStage)
			: m_BufferElements(elements), m_InputStage(inputStage)
		{
			CalculateOffset();
		}

	public:

		VkVertexInputBindingDescription GetBindingDescription()
		{
			VkVertexInputBindingDescription bidingDescription{};
			bidingDescription.binding = 0;
			bidingDescription.stride = m_Size;
			bidingDescription.inputRate = m_InputStage;

			return bidingDescription;
		}

		std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions()
		{
			std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};

			for (uint32_t i = 0; i < m_BufferElements.size(); i++)
			{
				VkVertexInputAttributeDescription attributeDescription;
				attributeDescription.binding = 0;
				attributeDescription.location = i;
				attributeDescription.format = ShaderDataTypeFormat(m_BufferElements[i].Type);
				attributeDescription.offset = m_BufferElements[i].Offset;

				attributeDescriptions.push_back(attributeDescription);
			}

			return attributeDescriptions;
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

	private:
		std::vector<BufferElement> m_BufferElements;
		uint32_t m_Size;
		VkVertexInputRate m_InputStage;

	};
	*/
}