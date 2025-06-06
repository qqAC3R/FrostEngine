#pragma once

#include "Frost/Core/Buffer.h"

namespace Frost
{
	enum class ShaderType;
	class BufferDevice;

	enum class BufferUsage
	{
		None = 0,
		Vertex, Index,

		Storage, Uniform,

		// RT Specific
		AccelerationStructure, AccelerationStructureReadOnly, ShaderBindingTable,

		// Transfer
		TransferSrc, TransferDst, ShaderAddress,

		// Indirect Drawing
		Indirect
	};

	struct BufferData
	{
		void* Data = nullptr;
		uint64_t Size = 0;
	};

	// A block of memory both on cpu/gpu memory
	struct HeapBlock
	{
		Ref<BufferDevice> DeviceBuffer; // GPU Memory
		Buffer HostBuffer;              // CPU Memory
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