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
		virtual void RenderDebugger() override;
		virtual void ShutDown() override;

		virtual void BeginFrame() override;
		virtual void EndFrame() override;

		virtual void BeginScene(const EditorCamera& camera) override;
		virtual void EndScene() override;

		virtual void Submit(const Ref<Mesh>& mesh, const glm::mat4& transform) override;
		virtual void Submit(const Ref<Mesh>& mesh, Ref<Material> material, const glm::mat4& transform) override;
		virtual void Submit(const PointLightComponent& pointLight, const glm::vec3& position) override;
		virtual void Submit(const DirectionalLightComponent& directionalLight, const glm::vec3& direction) override;
		virtual void Submit(const FogBoxVolumeComponent& fogVolume, const glm::mat4& transform) override;
		virtual void Submit(const CloudVolumeComponent& cloudVolume, const glm::vec3& position, const glm::vec3& scale) override;

		virtual void Resize(uint32_t width, uint32_t height) override;
		virtual Ref<Image2D> GetFinalImage(uint32_t id) const override;

		static VkDescriptorSet AllocateDescriptorSet(VkDescriptorSetAllocateInfo allocInfo);

		static void BeginTimeStampPass(const std::string& passName);
		static void EndTimeStampPass(const std::string& passName);

	private:
		static uint64_t BeginTimestampQuery();
		static void EndTimestampQuery(uint64_t queryId);
		static void GetTimestampResults(uint32_t currentFrameIndex);
		static const Vector<float>& GetFrameExecutionTimings();

	private:
		friend class VulkanRendererDebugger; // To access the internal time stamps functions
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