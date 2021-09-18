#pragma once

namespace Frost
{
	class Buffer;

	class IndexBuffer
	{
	public:
		virtual ~IndexBuffer() {}

		virtual uint32_t GetCount() const = 0;
		virtual uint32_t GetBufferSize() const = 0;
		virtual Ref<Buffer> GetBuffer() const = 0;

		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		virtual void Destroy() = 0;

		static Ref<IndexBuffer> Create(void* indicies, uint32_t count);
	};

}