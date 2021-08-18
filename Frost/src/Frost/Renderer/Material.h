#pragma once

#include "Frost/Renderer/Shader.h"

#include "Frost/Renderer/Buffers/Buffer.h"
#include "Frost/Renderer/Buffers/UniformBuffer.h"
#include "Frost/Renderer/RayTracing/AccelerationStructures.h"
#include "Frost/Renderer/Texture.h"

#include <glm/glm.hpp>

using VkDescriptorSetLayout = struct VkDescriptorSetLayout_T*;
using VkPipelineLayout = struct VkPipelineLayout_T*;

namespace Frost
{
	enum class GraphicsType;

	class Material
	{
	public:

		virtual void UpdateVulkanDescriptor() = 0;
		virtual void Bind(void* cmdBuf, VkPipelineLayout pipelineLayout, GraphicsType graphicsType) const = 0;;

		virtual Vector<VkDescriptorSetLayout> GetVulkanDescriptorLayout() const = 0;

		virtual void Invalidate() = 0;

		virtual void Set(const std::string& name, float value) = 0;
		virtual void Set(const std::string& name, int value) = 0;
		virtual void Set(const std::string& name, glm::vec2 value) = 0;
		virtual void Set(const std::string& name, glm::vec3 value) = 0;
		virtual void Set(const std::string& name, glm::vec4 value) = 0;
		virtual void Set(const std::string& name, glm::mat3 value) = 0;
		virtual void Set(const std::string& name, glm::mat4 value) = 0;

		virtual void Set(const std::string& name, const Ref<Texture2D>& texture) = 0;
		virtual void Set(const std::string& name, const Ref<Image2D>& image) = 0;

		virtual void Set(const std::string& name, const Ref<Buffer>& storageBuffer) = 0;
		virtual void Set(const std::string& name, const Ref<UniformBuffer>& uniformBuffer) = 0;

		virtual void Set(const std::string& name, const Ref<TopLevelAccelertionStructure>& accelerationStructure) = 0;


		virtual float GetFloat(const std::string& name) = 0;
		virtual int GetInt(const std::string& name) = 0;
		virtual glm::vec2 GetVector2(const std::string& name) = 0;
		virtual glm::vec3 GetVector3(const std::string& name) = 0;
		virtual glm::vec4 GetVector4(const std::string& name) = 0;
		virtual glm::mat3 GetMatrix3(const std::string& name) = 0;
		virtual glm::mat4 GetMatrix4(const std::string& name) = 0;
		
		virtual Ref<Buffer> GetBuffer(const std::string& name) = 0;
		virtual Ref<UniformBuffer> GetUniformBuffer(const std::string& name) = 0;
		virtual Ref<Texture2D> GetTexture2D(const std::string& name) = 0;
		virtual Ref<Image2D> GetImage2D(const std::string& name) = 0;
		virtual Ref<TopLevelAccelertionStructure> GetAccelerationStructure(const std::string& name) = 0;


		virtual void Destroy() = 0;

		static Ref<Material> Create(const Ref<Shader>& shader, const std::string& name = "");

	};

}