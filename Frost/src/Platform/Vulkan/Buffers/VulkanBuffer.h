#pragma once

#include "Frost/Renderer/Buffers/Buffer.h"

#include "VulkanBufferAllocator.h"

namespace Frost
{
	enum class ShaderType;
	struct VulkanMemoryInfo;

	class VulkanBuffer : public Buffer
	{
	public:
		VulkanBuffer(uint32_t size, std::initializer_list<BufferType> types);
		VulkanBuffer(void* data, uint32_t size, std::initializer_list<BufferType> types);
		virtual ~VulkanBuffer();


		virtual uint32_t GetBufferSize() const override { return m_BufferSize; }
		virtual void SetData(uint32_t size, void* data) override;
		virtual void SetData(void* data) override;

		VkBuffer GetVulkanBuffer() const override { return m_Buffer; }

		virtual void Destroy() override;

	private:
		VkBuffer m_Buffer;
		VulkanMemoryInfo m_BufferMemory;

		uint32_t m_BufferSize;

		std::vector<BufferType> m_Types;


	public:
		// Functions that are useless
		virtual void Bind() const override {}
		virtual void Unbind() const override {}

	};

}