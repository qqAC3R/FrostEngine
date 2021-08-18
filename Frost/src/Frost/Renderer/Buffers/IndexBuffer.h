#pragma once

using VkBuffer = struct VkBuffer_T*;

namespace Frost
{
	class BufferLayout;

	class IndexBuffer
	{
	public:
		virtual ~IndexBuffer() {}

		virtual uint32_t GetCount() const = 0;
		
		virtual uint32_t GetBufferSize() const = 0;

		/* OpenGL Specific API */
		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		/* Vulkan Specific API */
		virtual VkBuffer GetVulkanBuffer() const = 0;
		virtual void Bind(void* cmdBuf) const = 0;

		virtual void Destroy() = 0;

		static Ref<IndexBuffer> Create(void* indicies, uint32_t count);
	};

}