#pragma once

#include "Frost/Renderer/RendererAPI.h"

namespace Frost
{


	class VulkanRenderer : public RendererAPI
	{
	public:
		virtual void Init() override;
		virtual void ShutDown() override;

		virtual void BeginRenderPass(const EditorCamera& camera) override;
		virtual void EndRenderPass() override;

		// VulkanPipeline
		virtual void Submit(const Ref<Mesh>& mesh, const glm::mat4& transform) override;
		virtual void Submit(const Ref<Mesh>& mesh, Ref<Material> material, const glm::mat4& transform) override;


		void BeginVulkanRenderPass(Ref<RenderPass> renderPass);
		void EndVulkanRenderPass();

		void Resize();

	};

}