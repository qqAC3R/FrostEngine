#pragma once

namespace Frost
{
	class BufferLayout;
	class BufferDevice;

	class VertexBuffer
	{
	public:
		virtual ~VertexBuffer() {}

		virtual BufferLayout GetLayout() const = 0;
		virtual void SetLayout(const BufferLayout& layout) = 0;

		virtual uint64_t GetBufferSize() const = 0;
		virtual Ref<BufferDevice> GetBuffer() const = 0;

		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		virtual void Destroy() = 0;

		static Ref<VertexBuffer> Create(void* verticies, uint64_t size);
	};

}