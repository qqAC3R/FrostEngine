#pragma once

namespace Frost
{
	enum class ShaderType;
	
	class UniformBuffer
	{
	public:
		virtual ~UniformBuffer() {}

		virtual void SetData(void* data) = 0;
		virtual void SetData(void* data, uint32_t size) = 0;
		virtual uint32_t GetBufferSize() const = 0;

		virtual void Destroy() = 0;

		static Ref<UniformBuffer> Create(uint32_t size);
	};

}