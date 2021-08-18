#pragma once

#include "ShaderBindingTable.h"
#include "Frost/Renderer/Pipeline.h"

/* Vulkan Structs */
using VkPipelineLayout = struct VkPipelineLayout_T*;
using VkPipeline = struct VkPipeline_T*;
//using VkDescriptorSetLayout = struct VkDescriptorSetLayout_T*;


namespace Frost
{

	enum class ShaderType;
	class Material;

	// Only works for Vulkan/DX12 APIs
	class RayTracingPipeline
	{
	public:
		struct CreateInfo
		{
			/* Vulkan API specific */
			//Ref<Material> Descriptor;
			Ref<Shader> Shader;
			Pipeline::PushConstantInfo PushConstant;
			Ref<ShaderBindingTable> ShaderBindingTable;
		};

		virtual ~RayTracingPipeline() {}

		virtual void Destroy() = 0;

		virtual void Bind(void* cmdBuf = nullptr) const = 0;
		virtual void Unbind() const = 0;

		/* Vulkan Specific */
		virtual VkPipeline GetVulkanPipeline() const = 0;
		virtual VkPipelineLayout GetVulkanPipelineLayout() const = 0;
		virtual void BindVulkanPushConstant(void* cmdBuf, void* data) const = 0;

		static Ref<RayTracingPipeline> Create(const RayTracingPipeline::CreateInfo& pipelineCreateInfo);

	};


}




#if 0
namespace Frost
{
	struct PushConstantCache
	{
		bool IsEnabled = false;
		VkPushConstantRange PushConstant;
	};

	class VulkanRTPipeline
	{
	public:
		VulkanRTPipeline();
		virtual ~VulkanRTPipeline();

		void SetPushConstant(uint32_t size, std::initializer_list<ShaderType> stages);
		void CreatePipeline(const std::initializer_list<Scope<VulkanDescriptor>>& descriptors, VulkanShaderBindingTable& sbt);

		void Destroy();
		void Bind(VkCommandBuffer cmdBuf);

		VkPipeline GetPipeline() { return m_RtPipeline; }
		VkPipelineLayout GetPipelineLayout() { return m_RtPipelineLayout; }

		VkPushConstantRange GetPushConstantInfo() {
			if (m_PushConstant.IsEnabled)
				return m_PushConstant.PushConstant;

			FROST_ASSERT(false, "Push constant hasn't been initialized!");
			return {};

		}

	private:

		VkPipelineLayout m_RtPipelineLayout;
		VkPipeline m_RtPipeline;
		
		PushConstantCache m_PushConstant;

	};
#endif