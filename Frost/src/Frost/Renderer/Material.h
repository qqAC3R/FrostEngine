#pragma once

#include "Frost/Renderer/Shader.h"

#include "Frost/Renderer/Buffers/Buffer.h"
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
		virtual void Set(const std::string& name, const Ref<TextureCubeMap>& cubeMap) = 0;
		virtual void Set(const std::string& name, const Ref<Image2D>& image) = 0;
		virtual void Set(const std::string& name, const Ref<Buffer>& storageBuffer) = 0;
		virtual void Set(const std::string& name, const Ref<UniformBuffer>& uniformBuffer) = 0;
		virtual void Set(const std::string& name, const Ref<TopLevelAccelertionStructure>& accelerationStructure) = 0;

		virtual Ref<Buffer> GetBuffer(const std::string& name) = 0;
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

}