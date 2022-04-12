#pragma once

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>

#include "Frost/Core/Layer.h"

namespace Frost
{
	class Texture2D;
	class Image2D;

	class ImGuiLayer : public Layer
	{
	public:
		ImGuiLayer() {}
		virtual ~ImGuiLayer() {}

		virtual void Begin() = 0;
		virtual void Render() = 0;

		virtual void* GetImGuiTextureID(Ref<Image2D> texture, uint32_t mip = UINT32_MAX) = 0;
		virtual void RenderTexture(Ref<Image2D> texture, uint32_t width, uint32_t height, uint32_t mip = UINT32_MAX) = 0;

		static ImGuiLayer* Create();

#if 0
		void OnInit(VkRenderPass renderPass);

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnImGuiRender() override;
		virtual void OnResize(uint32_t width, uint32_t height) override;

		virtual void OnEvent(Event& event) override;

		void Begin();
		void Render();
		
		static void* GetTextureIDFromVulkanTexture(Ref<Image2D> texture);
		static void* GetTextureIDFromVulkanTexture_MipLevel(Ref<Image2D> texture, uint32_t mipLevel);
	private:
		void SetDarkThemeColors();

	private:
		float m_Time = 0.0f;

		VkRenderPass m_ImGuiRenderPass;
		VkDescriptorPool m_DescriptorPool;
		VkPhysicalDeviceProperties m_RendererSpecs;
#endif
	};
}