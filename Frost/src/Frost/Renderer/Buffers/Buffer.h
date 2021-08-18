#pragma once

using VkBuffer = struct VkBuffer_T*;

namespace Frost
{
	enum class ShaderType;

	enum class BufferType
	{
		None = 0,
		Vertex, Index,

		Storage, Uniform,


		/* Vulkan Specific */
		AccelerationStructure, AccelerationStructureReadOnly,

		// Transfer
		TransferSrc, TransferDst, ShaderAddress, ShaderBindingTable
	};

	class Buffer
	{
	public:
		virtual ~Buffer() {}


		virtual uint32_t GetBufferSize() const = 0;
		virtual void SetData(uint32_t size, void* data) = 0;
		virtual void SetData(void* data) = 0;

		/* OpenGL Specific API */
		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;
		
		/* Vulkan Specific API */
		virtual VkBuffer GetVulkanBuffer() const = 0;

		virtual void Destroy() = 0;


		static Ref<Buffer> Create(uint32_t size, std::initializer_list<BufferType> types = {});
		static Ref<Buffer> Create(void* data, uint32_t size, std::initializer_list<BufferType> types = {});

	};

}