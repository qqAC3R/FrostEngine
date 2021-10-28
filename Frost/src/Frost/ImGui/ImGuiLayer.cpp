#include "frostpch.h"
#include "ImGuiLayer.h"

#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include "backends/imgui_impl_glfw.h"

#include "Frost/Core/Application.h"
#include "Frost/Platform/Vulkan/VulkanContext.h"

#include "Frost/Renderer/Renderer.h"
#include "Frost/Platform/Vulkan/VulkanTexture.h"
#include "Frost/Platform/Vulkan/VulkanImage.h"

namespace Frost
{

	namespace Utils
	{
		static void CheckVkResult(VkResult err)
		{
			if (err == 0) return;
			FROST_ERROR("[ImGui] Error: {1}", err);

			if (err < 0) abort();
		}
	}


	ImGuiLayer::ImGuiLayer()
	{

	}

	ImGuiLayer::~ImGuiLayer()
	{

	}

	void ImGuiLayer::OnAttach()
	{
	}

	void ImGuiLayer::OnInit(VkRenderPass renderPass)
	{
		VkInstance instance = VulkanContext::GetInstance();
		VkPhysicalDevice physicalDevice = VulkanContext::GetCurrentDevice()->GetPhysicalDevice();
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		VkQueue graphicsQueue = VulkanContext::GetCurrentDevice()->GetQueueFamilies().GraphicsFamily.Queue;
		GLFWwindow* window = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();


		VkDescriptorPool imguiPool;
		VkDescriptorPoolSize pool_sizes[] =
		{
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
		};
		VkDescriptorPoolCreateInfo pool_info = {};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
		pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
		pool_info.pPoolSizes = pool_sizes;
		FROST_VKCHECK(vkCreateDescriptorPool(device, &pool_info, nullptr, &imguiPool));
		
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
		io.FontDefault = io.Fonts->AddFontFromFileTTF("Resources/Fonts/san-francisco/SF-Regular.otf", 18.0f);

		ImGui::StyleColorsDark();
		SetDarkThemeColors();

		ImGuiStyle& style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		ImGui_ImplGlfw_InitForVulkan(window, true);

		//this initializes imgui for Vulkan
		ImGui_ImplVulkan_InitInfo init_info = {};
		init_info.Instance = instance;
		init_info.PhysicalDevice = physicalDevice;
		init_info.Device = device;
		init_info.Queue = graphicsQueue;
		init_info.DescriptorPool = imguiPool;
		init_info.Allocator = nullptr;
		init_info.PipelineCache = VK_NULL_HANDLE;
		//init_info.MinImageCount = Renderer::GetRendererConfig().FramesInFlight;
		init_info.MinImageCount = 3;
		init_info.ImageCount = (uint32_t)VulkanContext::GetSwapChain()->GetImageCount();
		init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
		init_info.CheckVkResultFn = Utils::CheckVkResult;
		ImGui_ImplVulkan_Init(&init_info, renderPass);

		// Upload Fonts
		VkCommandBuffer cmdBuf = VulkanContext::GetCurrentDevice()->AllocateCommandBuffer(RenderQueueType::Graphics, true);
		ImGui_ImplVulkan_CreateFontsTexture(cmdBuf);
		VulkanContext::GetCurrentDevice()->FlushCommandBuffer(cmdBuf);
			
		ImGui_ImplVulkan_DestroyFontUploadObjects();
		m_DescriptorPool = imguiPool;

		vkGetPhysicalDeviceProperties(physicalDevice, &m_RendererSpecs);
	}
	
	void ImGuiLayer::OnDetach()
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr);
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}

	void ImGuiLayer::OnImGuiRender()
	{
		VkPhysicalDevice physicalDevice = VulkanContext::GetCurrentDevice()->GetPhysicalDevice();

		ImGui::Begin("Renderer:");
		{
			const char* deviceName = (const char*)m_RendererSpecs.deviceName;

			const char* vendor;
			switch (m_RendererSpecs.vendorID)
			{
				case 0x1002: vendor = "AMD "; break;
				case 0x10DE: vendor = "NVIDIA Corporation"; break;
				case 0x8086: vendor = "Intel"; break;
			}
			ImGui::Text(vendor);
			ImGui::Text(deviceName);
			ImGui::Text("Frametime: %.1f ms", 1000.0f / ImGui::GetIO().Framerate);
		}
		ImGui::End();
	}

	void ImGuiLayer::OnResize(uint32_t width, uint32_t height)
	{
		if (ImGui::GetCurrentContext() != nullptr)
		{
			auto& imgui_io = ImGui::GetIO();
			imgui_io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
		}
	}

	void ImGuiLayer::OnEvent(Event& event)
	{
		ImGuiIO& io = ImGui::GetIO();
		event.Handled |= event.IsInCategory(EventCategoryMouse) & io.WantCaptureMouse;
		event.Handled |= event.IsInCategory(EventCategoryKeyboard) & io.WantCaptureKeyboard;
	}

	void ImGuiLayer::Begin()
	{
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
	}

	void ImGuiLayer::Render()
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		ImGui::Render();
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuf);

		ImGuiIO& io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}
	}

	void* ImGuiLayer::GetTextureIDFromVulkanTexture(Ref<Image2D> texture)
	{
		Ref<VulkanImage2D> vulkanTexture = texture.As<VulkanImage2D>();
		return ImGui_ImplVulkan_AddTexture(vulkanTexture->GetVulkanSampler(), vulkanTexture->GetVulkanImageView(), vulkanTexture->GetVulkanImageLayout());
	}

	void ImGuiLayer::SetDarkThemeColors()
	{
		auto& colors = ImGui::GetStyle().Colors;
		colors[ImGuiCol_WindowBg] = ImVec4{ 0.1f, 0.105f, 0.11f, 1.0f };

		// Headers
		colors[ImGuiCol_Header] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
		colors[ImGuiCol_HeaderHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
		colors[ImGuiCol_HeaderActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

		// Buttons
		colors[ImGuiCol_Button] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
		colors[ImGuiCol_ButtonHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
		colors[ImGuiCol_ButtonActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

		// Frame BG
		colors[ImGuiCol_FrameBg] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
		colors[ImGuiCol_FrameBgHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
		colors[ImGuiCol_FrameBgActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

		// Tabs
		colors[ImGuiCol_Tab] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		colors[ImGuiCol_TabHovered] = ImVec4{ 0.38f, 0.3805f, 0.381f, 1.0f };
		colors[ImGuiCol_TabActive] = ImVec4{ 0.28f, 0.2805f, 0.281f, 1.0f };
		colors[ImGuiCol_TabUnfocused] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		colors[ImGuiCol_TabUnfocusedActive] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };

		// Title
		colors[ImGuiCol_TitleBg] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		colors[ImGuiCol_TitleBgActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

		// Resize Grip
		colors[ImGuiCol_ResizeGrip] = ImVec4(0.91f, 0.91f, 0.91f, 0.25f);
		colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.81f, 0.81f, 0.81f, 0.67f);
		colors[ImGuiCol_ResizeGripActive] = ImVec4(0.46f, 0.46f, 0.46f, 0.95f);

		// Scrollbar
		colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
		colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.0f);
		colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.0f);
		colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.0f);

		// Check Mark
		colors[ImGuiCol_CheckMark] = ImVec4(0.94f, 0.94f, 0.94f, 1.0f);

		// Slider
		colors[ImGuiCol_SliderGrab] = ImVec4(0.51f, 0.51f, 0.51f, 0.7f);
		colors[ImGuiCol_SliderGrabActive] = ImVec4(0.66f, 0.66f, 0.66f, 1.0f);
	}

	void UI::Begin(const std::string& title)
	{
		ImGui::Begin(title.c_str());
	}

	void UI::End()
	{
		ImGui::End();
	}

	void UI::ColorEdit(const std::string& name, glm::vec3& value)
	{
		ImGui::ColorEdit3(name.c_str(), reinterpret_cast<float*>(&value));
	}

	void UI::ColorEdit(const std::string& name, glm::vec4& value)
	{
		ImGui::ColorEdit4(name.c_str(), reinterpret_cast<float*>(&value));
	}

	void UI::Slider(const std::string& name, glm::vec3& value, float min, float max)
	{
		ImGui::SliderFloat3(name.c_str(), reinterpret_cast<float*>(&value), min, max);
	}

	void UI::Slider(const std::string& name, float& value, float min, float max)
	{
		ImGui::SliderFloat(name.c_str(), reinterpret_cast<float*>(&value), min, max);
	}

	void UI::Slider(const std::string& name, int& value, int min, int max)
	{
		ImGui::SliderInt(name.c_str(), reinterpret_cast<int*>(&value), min, max);
	}

	void UI::CheckMark(const std::string& name, bool& value)
	{
		ImGui::Checkbox(name.c_str(), &value);
	}

}