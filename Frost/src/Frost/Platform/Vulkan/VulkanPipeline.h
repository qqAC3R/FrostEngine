#pragma once

#include "Frost/Platform/Vulkan/Vulkan.h"

#include "Frost/Renderer/Pipeline.h"

namespace Frost
{

	class VulkanPipeline : public Pipeline
	{
	public:
		VulkanPipeline(Pipeline::CreateInfo& createInfo);
		virtual ~VulkanPipeline();

		virtual void Bind() override;
		virtual void Unbind() override {}

		/* Vulkan Specific */
		VkPipeline GetVulkanPipeline() const { return m_Pipeline; }
		VkPipelineLayout GetVulkanPipelineLayout() const { return m_PipelineLayout; }
		void BindVulkanPushConstant(std::string name, void* data);

		virtual void Destroy() override;
	private:
		VkPipeline m_Pipeline = VK_NULL_HANDLE;
		VkPipelineLayout m_PipelineLayout;
		std::unordered_map<std::string, VkPushConstantRange> m_PushConstantRangeCache;
	};

	namespace Utils
	{
		Vector<VkDescriptorSetLayout> GetDescriptorSetLayoutFromHashMap(const std::unordered_map<uint32_t, VkDescriptorSetLayout>& hashMapped_descriptorLayouts);
		Vector<VkPushConstantRange> GetPushConstantRangesFromVector(const Vector<PushConstantData>& pushConstantData);
		std::unordered_map<std::string, VkPushConstantRange> GetPushConstantCacheFromVector(const Vector<PushConstantData>& pushConstantData);
	}
}