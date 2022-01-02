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
		ImGuiLayer();
		~ImGuiLayer();

		void OnInit(VkRenderPass renderPass);

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnImGuiRender() override;
		virtual void OnResize(uint32_t width, uint32_t height) override;

		virtual void OnEvent(Event& event) override;

		void Begin();
		void Render();

		static void* GetTextureIDFromVulkanTexture(Ref<Image2D> texture);
	private:
		void SetDarkThemeColors();

	private:
		float m_Time = 0.0f;

		VkRenderPass m_ImGuiRenderPass;
		VkDescriptorPool m_DescriptorPool;
		VkPhysicalDeviceProperties m_RendererSpecs;
	};
}