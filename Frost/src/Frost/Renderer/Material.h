#pragma once

#include "Frost/Renderer/Shader.h"

#include "Frost/Renderer/Buffers/BufferDevice.h"
#include "Frost/Renderer/Buffers/UniformBuffer.h"
#include "Frost/Renderer/RayTracing/AccelerationStructures.h"
#include "Frost/Renderer/Texture.h"

#include "Frost/Renderer/Pipeline.h"
#include "Frost/Renderer/PipelineCompute.h"
#include "Frost/Renderer/RayTracing/RayTracingPipeline.h"

#include <glm/glm.hpp>

namespace Frost
{
	enum class GraphicsType;

	class Material
	{
	public:
		virtual ~Material() {}

		virtual void Bind(Ref<Pipeline> pipeline) = 0;
		virtual void Bind(Ref<ComputePipeline> computePipeline) = 0;
		virtual void Bind(Ref<RayTracingPipeline> rayTracingPipeline) = 0;

		virtual void Set(const std::string& name, float value) = 0;
		virtual void Set(const std::string& name, uint32_t value) = 0;
		virtual void Set(const std::string& name, const glm::vec3& value) = 0;
		
		virtual void Set(const std::string& name, const Ref<Texture2D>& texture) = 0;
		virtual void Set(const std::string& name, const Ref<Texture2D>& texture, uint32_t arrayIndex) = 0;
		virtual void Set(const std::string& name, const Ref<TextureCubeMap>& cubeMap) = 0;
		virtual void Set(const std::string& name, const Ref<Image2D>& image) = 0;
		virtual void Set(const std::string& name, const Ref<BufferDevice>& storageBuffer) = 0;
		virtual void Set(const std::string& name, const Ref<UniformBuffer>& uniformBuffer) = 0;
		virtual void Set(const std::string& name, const Ref<TopLevelAccelertionStructure>& accelerationStructure) = 0;

		virtual Ref<BufferDevice> GetBuffer(const std::string& name) = 0;
		virtual Ref<UniformBuffer> GetUniformBuffer(const std::string& name) = 0;
		virtual Ref<Texture2D> GetTexture2D(const std::string& name) = 0;
		virtual Ref<Image2D> GetImage2D(const std::string& name) = 0;
		virtual Ref<TopLevelAccelertionStructure> GetAccelerationStructure(const std::string& name) = 0;

		virtual float& GetFloat(const std::string& name) = 0;
		virtual uint32_t& GetUint(const std::string& name) = 0;
		virtual glm::vec3& GetVector3(const std::string& name) = 0;

		virtual void Destroy() = 0;

		static Ref<Material> Create(const Ref<Shader>& shader, const std::string& name = "");
	};

	class DataStorage
	{
	public:
		DataStorage() {}

		void Allocate(uint32_t size)
		{
			m_Buffer.Allocate(size);
		}

		template <typename T>
		void Add(const std::string& name, const T& data)
		{
			BufferSpecification bufferSpecs{};
			bufferSpecs.Size = sizeof(T);
			bufferSpecs.Offset = m_LastOffset;

			m_Buffer.Write((void*)&data, sizeof(T), m_LastOffset);

			m_LastOffset += sizeof(T);

			m_HashMapData[name] = bufferSpecs;
		}

		template <typename T>
		T& Get(const std::string& name)
		{
			// Removed assertion beucase it hurts the performance quite a bit (Maybe add it later)
			//if (m_HashMapData.find(name) == m_HashMapData.end()) FROST_ASSERT_MSG("The value hasn't been found");

			BufferSpecification& bufferSpecs = m_HashMapData[name];
			return m_Buffer.Read<T>(bufferSpecs.Offset);
		}

		template <typename T>
		void Set(const std::string& name, const T& data)
		{
			BufferSpecification& bufferSpecs = m_HashMapData[name];
			m_Buffer.Write((void*)&data, sizeof(T), bufferSpecs.Offset);
		}

	public:
		struct BufferSpecification
		{
			uint32_t Size = 0;
			uint32_t Offset = 0;
		};

		uint32_t m_LastOffset = 0;

		std::unordered_map<std::string, BufferSpecification> m_HashMapData;
		Buffer m_Buffer;
	};

}