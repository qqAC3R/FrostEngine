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

	class UI
	{
	public:
		static void Begin(const std::string& title);
		static void End();

		static void ColorEdit(const std::string& name, glm::vec3& value);
		static void ColorEdit(const std::string& name, glm::vec4& value);
		static void Slider(const std::string& name, glm::vec3& value, float min, float max);
		static void Slider(const std::string& name, float& value, float min, float max);
		static void Slider(const std::string& name, int& value, int min, int max);
		static void CheckMark(const std::string& name, bool& value);

		static void SetMouseEnabled(bool enable);
	};


}