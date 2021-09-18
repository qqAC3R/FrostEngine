#include "frostpch.h"
#include "VulkanPipeline.h"

#include "Frost/Platform/Vulkan/VulkanShader.h"
#include "Frost/Platform/Vulkan/VulkanMaterial.h"
#include "Frost/Platform/Vulkan/VulkanRenderPass.h"

#include "Frost/Platform/Vulkan/VulkanContext.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanBufferLayout.h"

namespace Frost
{

	// TODO: Add functions for the pipeline
	namespace Utils
	{
		Vector<VkDescriptorSetLayout> GetDescriptorSetLayoutFromHashMap(const std::unordered_map<uint32_t, VkDescriptorSetLayout>& hashMapped_descriptorLayouts)
		{
			Vector<VkDescriptorSetLayout> descriptorLayouts;
			for (auto& descriptorLayout : hashMapped_descriptorLayouts)
			{
				descriptorLayouts.push_back(descriptorLayout.second);
			}
			return descriptorLayouts;
		}

		Vector<VkPushConstantRange> GetPushConstantRangesFromVector(const Vector<PushConstantData>& pushConstantData)
		{
			Vector<VkPushConstantRange> pushConstants;
			for (auto& pushConstant : pushConstantData)
			{
				VkShaderStageFlags shaderStageBits{};
				for (auto& shaderStange : pushConstant.ShaderStage)
					shaderStageBits |= VulkanShader::GetShaderStageBits(shaderStange);

				VkPushConstantRange& pushConstantRange = pushConstants.emplace_back();
				pushConstantRange.offset = 0;
				pushConstantRange.size = pushConstant.Size;
				pushConstantRange.stageFlags = shaderStageBits;
			}
			return pushConstants;
		}

		std::unordered_map<std::string, VkPushConstantRange> GetPushConstantCacheFromVector(const Vector<PushConstantData>& pushConstantData)
		{
			std::unordered_map<std::string, VkPushConstantRange> pushConstants;
			for (auto& pushConstant : pushConstantData)
			{
				VkShaderStageFlags shaderStageBits{};
				for (auto& shaderStange : pushConstant.ShaderStage)
					shaderStageBits |= VulkanShader::GetShaderStageBits(shaderStange);

				VkPushConstantRange& pushConstantRange = pushConstants[pushConstant.Name];
				pushConstantRange.offset = 0;
				pushConstantRange.size = pushConstant.Size;
				pushConstantRange.stageFlags = shaderStageBits;
			}
			return pushConstants;
		}

		VkPrimitiveTopology GetVulkanPrimitiveTopology(PrimitiveTopology primitive)
		{
			switch (primitive)
			{
				case PrimitiveTopology::Points:         return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
				case PrimitiveTopology::Lines:          return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
				case PrimitiveTopology::LineStrip:      return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
				case PrimitiveTopology::Triangles:      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
				case PrimitiveTopology::TriangleStrip:  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
				case PrimitiveTopology::None:           FROST_ASSERT(false, "PrimitiveType::None is invalid!");
			}
			return VkPrimitiveTopology();
		}

		VkCullModeFlags GetVulkanCullMode(CullMode cullMode)
		{
			switch (cullMode)
			{
				case CullMode::None:          return VK_CULL_MODE_NONE;
				case CullMode::Front:         return VK_CULL_MODE_FRONT_BIT;
				case CullMode::Back:          return VK_CULL_MODE_BACK_BIT;
				case CullMode::FrontAndBack:  return VK_CULL_MODE_FRONT_AND_BACK;
			}

			FROST_ASSERT(false, "CullMode is invalid!");
			return VkCullModeFlags();
		}
	}

	VulkanPipeline::VulkanPipeline(Pipeline::CreateInfo& createInfo)
		: m_Specification(createInfo)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		auto shaders = createInfo.Shader.As<VulkanShader>();
		auto bufferLayout = createInfo.VertexBufferLayout;


		Vector<VkPipelineShaderStageCreateInfo> shaderStages;
		for (const auto& shader : shaders->m_ShaderModules)
		{
			VkPipelineShaderStageCreateInfo shaderStageInfo{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
			shaderStageInfo.stage = shader.first;;
			shaderStageInfo.module = shader.second;
			shaderStageInfo.pName = "main";
			shaderStageInfo.pSpecializationInfo = nullptr;
			shaderStages.push_back(shaderStageInfo);
		}

		auto bindingDescriptions = Utils::GetVertexBindingDescription(bufferLayout, VK_VERTEX_INPUT_RATE_VERTEX);
		auto attributeDescriptions = Utils::GetVertexAttributeDescriptions(bufferLayout);

		VkPipelineVertexInputStateCreateInfo vertexInputInfo{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescriptions;
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();;


		// Define how vulkan draws (triangles, lines, etc.)
		VkPipelineInputAssemblyStateCreateInfo inputAssembly{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
		inputAssembly.topology = Utils::GetVulkanPrimitiveTopology(createInfo.Topology);
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		VkPipelineViewportStateCreateInfo viewportState{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
		viewportState.viewportCount = 1;
		viewportState.pViewports = nullptr;
		viewportState.scissorCount = 1;
		viewportState.pScissors = nullptr;

		VkPipelineRasterizationStateCreateInfo rasterizer{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth = createInfo.LineWidth;
		rasterizer.cullMode = Utils::GetVulkanCullMode(createInfo.Cull);
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizer.depthBiasEnable = VK_FALSE;
		rasterizer.depthBiasConstantFactor = 0.0f;
		rasterizer.depthBiasClamp = 0.0f;
		rasterizer.depthBiasSlopeFactor = 0.0f;


		VkPipelineMultisampleStateCreateInfo multisampling{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		// TODO: DepthStencil Option
		VkPipelineDepthStencilStateCreateInfo depthStencil{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
		depthStencil.depthTestEnable = createInfo.UseDepth ? true : false;
		depthStencil.depthWriteEnable = createInfo.UseDepth ? true : false;;
		depthStencil.stencilTestEnable = createInfo.UseStencil ? true : false;;
		depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
		depthStencil.depthBoundsTestEnable = VK_FALSE;

		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE;

		VkPipelineColorBlendStateCreateInfo colorBlending{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;
		colorBlending.blendConstants[0] = 0.0f;
		colorBlending.blendConstants[1] = 0.0f;
		colorBlending.blendConstants[2] = 0.0f;
		colorBlending.blendConstants[3] = 0.0f;

		{
			// Setting up the pipeline layout
			auto descriptorLayouts = Utils::GetDescriptorSetLayoutFromHashMap(createInfo.Shader->GetVulkanDescriptorSetLayout());
			auto pushConstants = Utils::GetPushConstantRangesFromVector(createInfo.Shader->GetShaderReflectionData().GetPushConstantData());
			m_PushConstantRangeCache = Utils::GetPushConstantCacheFromVector(createInfo.Shader->GetShaderReflectionData().GetPushConstantData());

			VkPipelineLayoutCreateInfo pipelineLayoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
			pipelineLayoutInfo.setLayoutCount = (uint32_t)descriptorLayouts.size();
			pipelineLayoutInfo.pSetLayouts = descriptorLayouts.data();
			pipelineLayoutInfo.pushConstantRangeCount = (uint32_t)pushConstants.size();
			pipelineLayoutInfo.pPushConstantRanges = pushConstants.data();

			FROST_VKCHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_PipelineLayout));
			VulkanContext::SetStructDebugName("PipelineLayout", VK_OBJECT_TYPE_PIPELINE_LAYOUT, m_PipelineLayout);
		}

		std::array<VkDynamicState, 2> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

		VkPipelineDynamicStateCreateInfo dynamicStatesCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
		dynamicStatesCreateInfo.dynamicStateCount = dynamicStates.size();
		dynamicStatesCreateInfo.pDynamicStates = dynamicStates.data();

		VkGraphicsPipelineCreateInfo pipelineInfo{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
		pipelineInfo.pDepthStencilState = &depthStencil;
		pipelineInfo.stageCount = (uint32_t)shaderStages.size();
		pipelineInfo.pStages = shaderStages.data();
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDynamicState = &dynamicStatesCreateInfo;
		pipelineInfo.layout = m_PipelineLayout;
		pipelineInfo.renderPass = createInfo.RenderPass.As<VulkanRenderPass>()->GetVulkanRenderPass();
		pipelineInfo.subpass = 0;

		FROST_VKCHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline));
		VulkanContext::SetStructDebugName("Pipeline", VK_OBJECT_TYPE_PIPELINE, m_Pipeline);
	}

	VulkanPipeline::~VulkanPipeline()
	{
	}

	void VulkanPipeline::Bind()
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);
	}

	void VulkanPipeline::BindVulkanPushConstant(std::string name, void* data)
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		vkCmdPushConstants(cmdBuf, m_PipelineLayout,
							m_PushConstantRangeCache[name].stageFlags,
							m_PushConstantRangeCache[name].offset,
							m_PushConstantRangeCache[name].size,
						   data);
	}

	void VulkanPipeline::Destroy()
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		vkDestroyPipeline(device, m_Pipeline, nullptr);
		vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr);
	}

}