#pragma once

//#include "Frost/Renderer/Texture.h"
#include <vulkan/vulkan.hpp>
#include "ImGuiLayer.h"

typedef void* ImTextureID;

namespace Frost
{
	class Texture2D;
	class Image2D;

	class VulkanImGuiLayer : public ImGuiLayer
	{
	public:
		VulkanImGuiLayer();
		virtual ~VulkanImGuiLayer();

		void OnInit(VkRenderPass renderPass);

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnImGuiRender() override;
		virtual void OnResize(uint32_t width, uint32_t height) override;

		virtual void OnEvent(Event& event) override;

		virtual void Begin() override;
		virtual void Render() override;

		static void* GetTextureIDFromVulkanTexture(Ref<Image2D> texture);
		static void* GetTextureIDFromVulkanTexture_MipLevel(Ref<Image2D> texture, uint32_t mipLevel);

		static void ResetStyleSettings();

		virtual void* GetImGuiTextureID(Ref<Image2D> texture, uint32_t mip = UINT32_MAX) override;
		virtual void RenderTexture(Ref<Image2D> texture, uint32_t width, uint32_t height, uint32_t mip = UINT32_MAX) override;

	private:
		void Set_FrostStyle_Theme();
	private:
		VkRenderPass m_ImGuiRenderPass;
		VkDescriptorPool m_DescriptorPool;
		VkPhysicalDeviceProperties m_RendererSpecs;
	};
}