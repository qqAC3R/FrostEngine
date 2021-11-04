#pragma once

namespace Frost
{
	enum class ShaderType;

	enum class BufferUsage
	{
		None = 0,
		Vertex, Index,

		Storage, Uniform,


		/* Vulkan Specific */
		AccelerationStructure, AccelerationStructureReadOnly,

		// Transfer
		TransferSrc, TransferDst, ShaderAddress, ShaderBindingTable
	};

	struct BufferData
	{
		void* Data = nullptr;
		uint64_t Size = 0;
	};

	class BufferDevice
	{
	public:
		virtual ~BufferDevice() {}

		virtual uint64_t GetBufferSize() const = 0;
		virtual BufferData GetBufferData() const = 0;
		virtual void SetData(uint64_t size, void* data) = 0;
		virtual void SetData(void* data) = 0;

		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;
		
		virtual void Destroy() = 0;

		static Ref<BufferDevice> Create(uint64_t size, std::initializer_list<BufferUsage> types = {});
		static Ref<BufferDevice> Create(uint64_t size, void* data, std::initializer_list<BufferUsage> types = {});
	};

}