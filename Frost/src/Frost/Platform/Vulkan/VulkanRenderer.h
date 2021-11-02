#pragma once

#include "Frost/Renderer/RendererAPI.h"
#include "Frost/Platform/Vulkan/Vulkan.h"

namespace Frost
{
	class VulkanRenderer : public RendererAPI
	{
	public:
		virtual void Init() override;
		virtual void InitRenderPasses() override;
		virtual void Render() override;
		virtual void ShutDown() override;

		virtual void BeginFrame() override;
		virtual void EndFrame() override;

		virtual void BeginScene(const EditorCamera& camera) override;
		virtual void EndScene() override;

		virtual void Submit(const Ref<Mesh>& mesh, const glm::mat4& transform) override;
		virtual void Submit(const Ref<Mesh>& mesh, Ref<Material> material, const glm::mat4& transform) override;

		virtual void Resize(uint32_t width, uint32_t height) override;
		virtual Ref<Image2D> GetFinalImage(uint32_t id) const override;

		static VkDescriptorSet AllocateDescriptorSet(VkDescriptorSetAllocateInfo allocInfo);
	};

	namespace Utils
	{
		void InsertImageMemoryBarrier(
			VkCommandBuffer cmdbuffer,
			VkImage image,
			VkAccessFlags srcAccessMask,
			VkAccessFlags dstAccessMask,
			VkImageLayout oldImageLayout,
			VkImageLayout newImageLayout,
			VkPipelineStageFlags srcStageMask,
			VkPipelineStageFlags dstStageMask,
			VkImageSubresourceRange subresourceRange
		);

		void SetImageLayout(
			VkCommandBuffer cmdbuffer,
			VkImage image,
			VkImageLayout oldImageLayout,
			VkImageLayout newImageLayout,
			VkImageSubresourceRange subresourceRange,
			VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
		);

		void SetImageLayout(
			VkCommandBuffer cmdbuffer,
			VkImage image,
			VkImageAspectFlags aspectMask,
			VkImageLayout oldImageLayout,
			VkImageLayout newImageLayout,
			VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
		);
	}
}