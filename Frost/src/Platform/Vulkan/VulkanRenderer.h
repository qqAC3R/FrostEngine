#pragma once

#include "Frost/Renderer/RendererAPI.h"

namespace Frost
{


	class VulkanRenderer : public RendererAPI
	{
	public:
		virtual void Init() override;
		virtual void ShutDown() override;

		virtual void BeginScene(const EditorCamera& camera) override;
		virtual void EndScene() override;

		virtual void Submit(const Ref<Mesh>& mesh, const glm::mat4& transform) override;
		virtual void Submit(const Ref<Mesh>& mesh, Ref<Material> material, const glm::mat4& transform) override;

		//void BeginVulkanRenderPass(Ref<RenderPass> renderPass);
		//void EndVulkanRenderPass();

	protected:
		virtual void Update() override;
		virtual void Resize(uint32_t width, uint32_t height) override;
	};

}