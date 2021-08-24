#include "frostpch.h"
#include "VulkanPipeline.h"

#include "Platform/Vulkan/VulkanShader.h"
#include "Platform/Vulkan/VulkanMaterial.h"

#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/Buffers/VulkanBufferLayout.h"

namespace Frost
{

	// TODO: Add functions for the pipeline
	namespace VulkanUtils
	{

	}


	VulkanPipeline::VulkanPipeline(Pipeline::CreateInfo& createInfo)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		auto shaders = createInfo.Shader.As<VulkanShader>();
		auto bufferLayout = createInfo.VertexBufferLayout;
		auto framebufferSpecification = createInfo.FramebufferSpecification;
		

		std::vector<VkDescriptorSetLayout> descriptorLayouts;
		auto hashMapped_descriptorLayouts = createInfo.Shader->GetVulkanDescriptorSetLayout();
		for (auto& descriptorLayout : hashMapped_descriptorLayouts)
			descriptorLayouts.push_back(descriptorLayout.second);



		std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
		for (const auto& shader : shaders->m_ShaderModules)
		{
			VkPipelineShaderStageCreateInfo shaderStageInfo{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };

			shaderStageInfo.stage = shader.first;;
			shaderStageInfo.module = shader.second;
			shaderStageInfo.pName = "main";
			shaderStageInfo.pSpecializationInfo = nullptr;

			shaderStages.push_back(shaderStageInfo);

		}




		auto bindingDescriptions = VulkanUtils::GetVertexBindingDescription(bufferLayout, VK_VERTEX_INPUT_RATE_VERTEX);
		auto attributeDescriptions = VulkanUtils::GetVertexAttributeDescriptions(bufferLayout);

		VkPipelineVertexInputStateCreateInfo vertexInputInfo{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescriptions;
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();;





		// Define how vulkan draws (triangles, lines, etc.)
		VkPipelineInputAssemblyStateCreateInfo inputAssembly{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)framebufferSpecification.Width;
		viewport.height = (float)framebufferSpecification.Height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = { framebufferSpecification.Width, framebufferSpecification.Height };

		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;




		VkPipelineRasterizationStateCreateInfo rasterizer{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = VK_CULL_MODE_NONE;
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
		depthStencil.depthTestEnable = true;
		depthStencil.depthWriteEnable = true;
		depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
		depthStencil.depthBoundsTestEnable = VK_FALSE;
		depthStencil.stencilTestEnable = false;

		

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

			VkPipelineLayoutCreateInfo pipelineLayoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
			pipelineLayoutInfo.setLayoutCount = (uint32_t)descriptorLayouts.size();
			pipelineLayoutInfo.pSetLayouts = descriptorLayouts.data();
			pipelineLayoutInfo.pushConstantRangeCount = createInfo.PushConstant.PushConstantRange > 0 ? 1 : 0;
														// If the range is higher than 0, then the count is 1

			pipelineLayoutInfo.pPushConstantRanges = nullptr;

			if (createInfo.PushConstant.PushConstantRange > 0)
			{
				VkShaderStageFlags pushConstantStages{};
				for (auto stage : createInfo.PushConstant.PushConstantShaderStages)
				{
					pushConstantStages |= VulkanShader::GetShaderStageBits(stage);
				}

				VkPushConstantRange pushConstantRange{};
				pushConstantRange.offset = 0;
				pushConstantRange.size = createInfo.PushConstant.PushConstantRange;
				pushConstantRange.stageFlags = pushConstantStages;

				pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

				m_PushConstantRangeCache = pushConstantRange;
			}


			if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS)
			{
				FROST_ASSERT(0, "Failed to create pipeline layout!");
			}

			VulkanContext::SetStructDebugName("PipelineLayout", VK_OBJECT_TYPE_PIPELINE_LAYOUT, m_PipelineLayout);

		}

		std::array<VkDynamicState, 2> dynamicStates;
		dynamicStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
		dynamicStates[1] = VK_DYNAMIC_STATE_SCISSOR;


		VkPipelineDynamicStateCreateInfo dynamicStatesCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
		dynamicStatesCreateInfo.dynamicStateCount = 2;
		dynamicStatesCreateInfo.pDynamicStates = dynamicStates.data();

		VkGraphicsPipelineCreateInfo pipelineInfo{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
		pipelineInfo.pDepthStencilState = &depthStencil;
		pipelineInfo.stageCount = (uint32_t)shaderStages.size();
		pipelineInfo.pStages = shaderStages.data();
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		//pipelineInfo.pViewportState = nullptr;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pColorBlendState = &colorBlending;
		//pipelineInfo.pDynamicState = nullptr;
		pipelineInfo.pDynamicState = &dynamicStatesCreateInfo;
		pipelineInfo.layout = m_PipelineLayout;

		//auto renderPass = createInfo.PipelineRenderPass->GetVulkanRenderPass();
		pipelineInfo.renderPass = createInfo.RenderPass->GetVulkanRenderPass();
		pipelineInfo.subpass = 0;

		FROST_VKCHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline), "");

		VulkanContext::SetStructDebugName("Pipeline", VK_OBJECT_TYPE_PIPELINE, m_Pipeline);


	}

	VulkanPipeline::~VulkanPipeline()
	{
	}

	void VulkanPipeline::Bind(void* cmdBuf)
	{
		auto vulkanCmdBuf = (VkCommandBuffer)cmdBuf;

		vkCmdBindPipeline(vulkanCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);
	}

	void VulkanPipeline::BindVulkanPushConstant(void* cmdBuf, void* data)
	{
		auto vulkanCmdBuf = (VkCommandBuffer)cmdBuf;

		vkCmdPushConstants(vulkanCmdBuf, m_PipelineLayout,
						   m_PushConstantRangeCache.stageFlags,
						   m_PushConstantRangeCache.offset,
						   m_PushConstantRangeCache.size,
						   data);

	}

	void VulkanPipeline::Destroy()
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		vkDestroyPipeline(device, m_Pipeline, nullptr);
		vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr);
	}

}