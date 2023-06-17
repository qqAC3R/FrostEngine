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

		virtual void SubmitCmdsToRender() override;

		virtual void BeginScene(Ref<EditorCamera> camera) override;
		virtual void BeginScene(Ref<RuntimeCamera> camera) override;
		virtual void EndScene() override;

		virtual void Submit(const Ref<Mesh>& mesh, const glm::mat4& transform, uint32_t entityID) override;
		virtual void Submit(const PointLightComponent& pointLight, const glm::vec3& position) override;
		virtual void Submit(const DirectionalLightComponent& directionalLight, const glm::vec3& direction) override;
		virtual void Submit(const RectangularLightComponent& rectLight, const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& scale) override;
		virtual void Submit(const FogBoxVolumeComponent& fogVolume, const glm::mat4& transform) override;
		virtual void Submit(const CloudVolumeComponent& cloudVolume, const glm::vec3& position, const glm::vec3& scale) override;
		virtual void SubmitBillboards(const glm::vec3& positon, const glm::vec2& size, const glm::vec4& color) override;
		virtual void SubmitBillboards(const glm::vec3& positon, const glm::vec2& size, Ref<Texture2D> texture) override;
		virtual void SubmitLines(const glm::vec3& positon0, const glm::vec3& positon1, const glm::vec4& color) override;
		virtual void SubmitWireframeMesh(Ref<Mesh> mesh, const glm::mat4& transform, const glm::vec4& color, float lineWidth) override;

		virtual uint32_t ReadPixelFromFramebufferEntityID(uint32_t x, uint32_t y) override;
		virtual uint32_t GetCurrentFrameIndex() override;
		virtual void SetEditorActiveEntity(uint32_t selectedEntityId) override;

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