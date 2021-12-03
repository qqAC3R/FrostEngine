#pragma once

namespace Frost
{
	class BufferDevice;

	class IndexBuffer
	{
	public:
		virtual ~IndexBuffer() {}

		virtual uint64_t GetCount() const = 0;
		virtual uint64_t GetBufferSize() const = 0;
		virtual Ref<BufferDevice> GetBuffer() const = 0;

		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		virtual void Destroy() = 0;

		static Ref<IndexBuffer> Create(void* indicies, uint64_t count);
	};

}