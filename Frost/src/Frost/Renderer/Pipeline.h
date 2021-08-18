#pragma once

//#include <vulkan/vulkan.hpp>

#include "Frost/Renderer/Buffers/BufferLayout.h"

#include "Frost/Renderer/Shader.h"
#include "Frost/Renderer/RenderPass.h"

/* Vulkan Structs */
using VkPipelineLayout = struct VkPipelineLayout_T*;
using VkPipeline = struct VkPipeline_T*;
using VkDescriptorSetLayout = struct VkDescriptorSetLayout_T*;

namespace Frost
{


	class Material;

	class Pipeline
	{
	public:
		virtual ~Pipeline() {}

		struct PushConstantInfo
		{
			uint32_t PushConstantRange = 0;
			Vector<ShaderType> PushConstantShaderStages;
		};

		struct CreateInfo
		{
			/* Vulkan API specific */
			//Ref<Material> Descriptor;
			Ref<Shader> Shader;

			PushConstantInfo PushConstant;

			BufferLayout VertexBufferLayout;
			Ref<RenderPass> RenderPass;
			FramebufferSpecification FramebufferSpecification;

			//Ref<Shader> Shaders;
		};

		virtual void Bind(void* cmdBuf = nullptr) = 0;
		virtual void Unbind() = 0;

		/* Vulkan Specific */
		virtual VkPipeline GetVulkanPipeline() const = 0;
		virtual VkPipelineLayout GetVulkanPipelineLayout() const = 0;
		virtual void BindVulkanPushConstant(void* cmdBuf, void* data) = 0;


		virtual void Destroy() = 0;

		static Ref<Pipeline> Create(Pipeline::CreateInfo& createInfo);

	};

}