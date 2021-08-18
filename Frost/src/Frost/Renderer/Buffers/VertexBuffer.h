#pragma once

using VkBuffer = struct VkBuffer_T*;

namespace Frost
{

	class BufferLayout;

	class VertexBuffer
	{
	public:
		virtual ~VertexBuffer() {}

		virtual BufferLayout GetLayout() const = 0;
		virtual void SetLayout(const BufferLayout& layout) = 0;

		virtual uint32_t GetBufferSize() const = 0;

		/* OpenGL Specific API */
		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		/* Vulkan Specific API */
		virtual VkBuffer GetVulkanBuffer() const = 0;
		virtual void Bind(void* cmdBuf) const = 0;

		virtual void Destroy() = 0;

		static Ref<VertexBuffer> Create(void* verticies, uint32_t size);
	};

}