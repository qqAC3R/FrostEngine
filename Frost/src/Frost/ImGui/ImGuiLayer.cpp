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
			ImGui::Text("Frametime: %.2f ms", 1000.0f / ImGui::GetIO().Framerate);

			ImGui::Separator();

			GPUMemoryStats memoryStats = Application::Get().GetWindow().GetGraphicsContext()->GetGPUMemoryStats();
			float used = memoryStats.UsedMemory / 1000000.0f;
			float free = memoryStats.FreeMemory / 1000000.0f;
			ImGui::Text("Used Memory: %.2f MB", used);
			ImGui::Text("Free Memory: %.2f MB", free);
		}
		ImGui::End();


#if 0
		ImGui::Begin("ImGuiStyle");

		auto& colors = ImGui::GetStyle().Colors;

		// Window
		ImGui::Text("Window");
		ImGui::SliderFloat4("ImGuiCol_WindowBg", &colors[ImGuiCol_WindowBg].x, 0.0f, 1.0f);


		// Headers
		ImGui::Text("Headers");
		ImGui::SliderFloat4("ImGuiCol_Header", &colors[ImGuiCol_Header].x, 0.0f, 1.0f);
		ImGui::SliderFloat4("ImGuiCol_HeaderHovered", &colors[ImGuiCol_HeaderHovered].x, 0.0f, 1.0f);
		ImGui::SliderFloat4("ImGuiCol_HeaderActive", &colors[ImGuiCol_HeaderActive].x, 0.0f, 1.0f);

		// Buttons
		ImGui::Text("Buttons");
		ImGui::SliderFloat4("ImGuiCol_Button", &colors[ImGuiCol_Button].x, 0.0f, 1.0f);
		ImGui::SliderFloat4("ImGuiCol_ButtonHovered", &colors[ImGuiCol_ButtonHovered].x, 0.0f, 1.0f);
		ImGui::SliderFloat4("ImGuiCol_ButtonActive", &colors[ImGuiCol_ButtonActive].x, 0.0f, 1.0f);

		// Frame BG
		ImGui::Text("Frame BG");
		ImGui::SliderFloat4("ImGuiCol_FrameBg", &colors[ImGuiCol_FrameBg].x, 0.0f, 1.0f);
		ImGui::SliderFloat4("ImGuiCol_FrameBgHovered", &colors[ImGuiCol_FrameBgHovered].x, 0.0f, 1.0f);
		ImGui::SliderFloat4("ImGuiCol_FrameBgActive", &colors[ImGuiCol_FrameBgActive].x, 0.0f, 1.0f);

		// Tabs
		ImGui::Text("Tabs");
		ImGui::SliderFloat4("ImGuiCol_Tab", &colors[ImGuiCol_Tab].x, 0.0f, 1.0f);
		ImGui::SliderFloat4("ImGuiCol_TabHovered", &colors[ImGuiCol_TabHovered].x, 0.0f, 1.0f);
		ImGui::SliderFloat4("ImGuiCol_TabActive", &colors[ImGuiCol_TabActive].x, 0.0f, 1.0f);
		ImGui::SliderFloat4("ImGuiCol_TabUnfocused", &colors[ImGuiCol_TabUnfocused].x, 0.0f, 1.0f);
		ImGui::SliderFloat4("ImGuiCol_TabUnfocusedActive", &colors[ImGuiCol_TabUnfocusedActive].x, 0.0f, 1.0f);

		// Title
		ImGui::Text("Title");
		ImGui::SliderFloat4("ImGuiCol_TitleBg", &colors[ImGuiCol_TitleBg].x, 0.0f, 1.0f);
		ImGui::SliderFloat4("ImGuiCol_TitleBgActive", &colors[ImGuiCol_TitleBgActive].x, 0.0f, 1.0f);
		ImGui::SliderFloat4("ImGuiCol_TitleBgCollapsed", &colors[ImGuiCol_TitleBgCollapsed].x, 0.0f, 1.0f);
		ImGui::SliderFloat4("ImGuiCol_TitleBgCollapsed", &colors[ImGuiCol_TitleBgCollapsed].x, 0.0f, 1.0f);

		// Resize Grip
		ImGui::Text("Resize Grip");
		ImGui::SliderFloat4("ImGuiCol_ResizeGrip", &colors[ImGuiCol_ResizeGrip].x, 0.0f, 1.0f);
		ImGui::SliderFloat4("ImGuiCol_ResizeGripHovered", &colors[ImGuiCol_ResizeGripHovered].x, 0.0f, 1.0f);
		ImGui::SliderFloat4("ImGuiCol_ResizeGripActive", &colors[ImGuiCol_ResizeGripActive].x, 0.0f, 1.0f);

		// Scrollbar
		ImGui::Text("Scrollbar");
		ImGui::SliderFloat4("ImGuiCol_ScrollbarBg", &colors[ImGuiCol_ScrollbarBg].x, 0.0f, 1.0f);
		ImGui::SliderFloat4("ImGuiCol_ScrollbarGrab", &colors[ImGuiCol_ScrollbarGrab].x, 0.0f, 1.0f);
		ImGui::SliderFloat4("ImGuiCol_ScrollbarGrabHovered", &colors[ImGuiCol_ScrollbarGrabHovered].x, 0.0f, 1.0f);
		ImGui::SliderFloat4("ImGuiCol_ScrollbarGrabActive", &colors[ImGuiCol_ScrollbarGrabActive].x, 0.0f, 1.0f);
		ImGui::SliderFloat4("ImGuiCol_ScrollbarGrabActive", &colors[ImGuiCol_ScrollbarGrabActive].x, 0.0f, 1.0f);

		// Check Mark
		ImGui::Text("Check Mark");
		ImGui::SliderFloat4("ImGuiCol_CheckMark", &colors[ImGuiCol_CheckMark].x, 0.0f, 1.0f);

		// Slider
		ImGui::Text("Slider");
		ImGui::SliderFloat4("ImGuiCol_SliderGrab", &colors[ImGuiCol_SliderGrab].x, 0.0f, 1.0f);
		ImGui::SliderFloat4("ImGuiCol_SliderGrabActive", &colors[ImGuiCol_SliderGrabActive].x, 0.0f, 1.0f);


		ImGui::Text("ImGuiStyle:");
		ImGuiStyle& style = ImGui::GetStyle();
		ImGui::SliderFloat2("style.WindowPadding", &style.WindowPadding.x, 0.0f, 10.0f);
		ImGui::SliderFloat2("style.FramePadding", &style.FramePadding.x, 0.0f, 10.0f);
		ImGui::SliderFloat2("style.CellPadding", &style.CellPadding.x, 0.0f, 10.0f);
		ImGui::SliderFloat2("style.ItemSpacing", &style.ItemSpacing.x, 0.0f, 10.0f);
		ImGui::SliderFloat2("style.ItemInnerSpacing", &style.ItemInnerSpacing.x, 0.0f, 10.0f);
		ImGui::SliderFloat2("style.TouchExtraPadding", &style.TouchExtraPadding.x, 0.0f, 10.0f);
		ImGui::SliderFloat("style.IndentSpacing", &style.IndentSpacing, 0.0f, 50.0f);
		ImGui::SliderFloat("style.ScrollbarSize", &style.ScrollbarSize, 0.0f, 50.0f);
		ImGui::SliderFloat("style.GrabMinSize", &style.GrabMinSize, 0.0f, 50.0f);
		ImGui::SliderFloat("style.WindowBorderSize", &style.WindowBorderSize, 0.0f, 2.0f);
		ImGui::SliderFloat("style.ChildBorderSize", &style.ChildBorderSize, 0.0f, 2.0f);
		ImGui::SliderFloat("style.PopupBorderSize", &style.PopupBorderSize, 0.0f, 2.0f);
		ImGui::SliderFloat("style.FrameBorderSize", &style.FrameBorderSize, 0.0f, 2.0f);
		ImGui::SliderFloat("style.TabBorderSize", &style.TabBorderSize, 0.0f, 2.0f);
		ImGui::SliderFloat("style.WindowRounding", &style.WindowRounding, 0.0f, 15.0f);
		ImGui::SliderFloat("style.ChildRounding", &style.ChildRounding, 0.0f, 15.0f);
		ImGui::SliderFloat("style.FrameRounding", &style.FrameRounding, 0.0f, 15.0f);
		ImGui::SliderFloat("style.PopupRounding", &style.PopupRounding, 0.0f, 15.0f);
		ImGui::SliderFloat("style.TabRounding", &style.TabRounding, 0.0f, 15.0f);

		ImGui::End();
#endif
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
		ImVec4* colors = ImGui::GetStyle().Colors;
		colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
		colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
		colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
		colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_PopupBg] = ImVec4(0.19f, 0.19f, 0.19f, 0.92f);
		colors[ImGuiCol_Border] = ImVec4(0.19f, 0.19f, 0.19f, 0.29f);
		colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.24f);
		colors[ImGuiCol_FrameBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
		colors[ImGuiCol_FrameBgHovered] = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
		colors[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
		colors[ImGuiCol_TitleBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
		colors[ImGuiCol_TitleBgActive] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
		colors[ImGuiCol_MenuBarBg] = ImVec4(0.0f, 0.0f, 0.0f, 1.00f);
		colors[ImGuiCol_ScrollbarBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
		colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
		colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.40f, 0.54f);
		colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
		colors[ImGuiCol_CheckMark] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
		colors[ImGuiCol_SliderGrab] = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
		colors[ImGuiCol_SliderGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
		colors[ImGuiCol_Button] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
		colors[ImGuiCol_ButtonHovered] = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
		colors[ImGuiCol_ButtonActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
		colors[ImGuiCol_Header] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
		colors[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 0.00f, 0.00f, 0.36f);
		colors[ImGuiCol_HeaderActive] = ImVec4(0.20f, 0.22f, 0.23f, 0.33f);
		colors[ImGuiCol_Separator] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
		colors[ImGuiCol_SeparatorHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
		colors[ImGuiCol_SeparatorActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
		colors[ImGuiCol_ResizeGrip] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
		colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
		colors[ImGuiCol_ResizeGripActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
		colors[ImGuiCol_Tab] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
		colors[ImGuiCol_TabHovered] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
		colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.20f, 0.20f, 0.36f);
		colors[ImGuiCol_TabUnfocused] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
		colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
		colors[ImGuiCol_DockingPreview] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
		colors[ImGuiCol_DockingEmptyBg] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
		colors[ImGuiCol_PlotLines] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
		colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
		colors[ImGuiCol_PlotHistogram] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
		colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
		colors[ImGuiCol_TableHeaderBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
		colors[ImGuiCol_TableBorderStrong] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
		colors[ImGuiCol_TableBorderLight] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
		colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
		colors[ImGuiCol_TextSelectedBg] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
		colors[ImGuiCol_DragDropTarget] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
		colors[ImGuiCol_NavHighlight] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
		colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 0.00f, 0.00f, 0.70f);
		colors[ImGuiCol_NavWindowingDimBg] = ImVec4(1.00f, 0.00f, 0.00f, 0.20f);
		colors[ImGuiCol_ModalWindowDimBg] = ImVec4(1.00f, 0.00f, 0.00f, 0.35f);

		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowPadding = ImVec2(8.00f, 8.00f);
		style.FramePadding = ImVec2(10.00f, 3.00f);
		style.CellPadding = ImVec2(6.00f, 6.00f);
		style.ItemSpacing = ImVec2(6.00f, 6.00f);
		style.ItemInnerSpacing = ImVec2(6.00f, 6.00f);
		style.TouchExtraPadding = ImVec2(0.00f, 0.00f);
		style.IndentSpacing = 25;
		style.ScrollbarSize = 15;
		style.GrabMinSize = 10;
		style.WindowBorderSize = 1;
		style.ChildBorderSize = 1;
		style.PopupBorderSize = 1;
		style.FrameBorderSize = 1;
		style.TabBorderSize = 1;
		style.WindowRounding = 7;
		style.ChildRounding = 4;
		style.FrameRounding = 3;
		style.PopupRounding = 4;
		style.ScrollbarRounding = 9;
		style.GrabRounding = 3;
		style.LogSliderDeadzone = 4;
		style.TabRounding = 4;

#if 0
		auto& colors = ImGui::GetStyle().Colors;
		colors[ImGuiCol_WindowBg] = ImVec4{ 0.06f, 0.06f, 0.09f, 1.0f };

		// Headers
		colors[ImGuiCol_Header] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
		colors[ImGuiCol_HeaderHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
		colors[ImGuiCol_HeaderActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

		// Buttons
		colors[ImGuiCol_Button] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
		colors[ImGuiCol_ButtonHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
		colors[ImGuiCol_ButtonActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

		// Frame BG
		colors[ImGuiCol_FrameBg] = ImVec4{ 0.2f, 0.205f, 0.25f, 1.0f };
		colors[ImGuiCol_FrameBgHovered] = ImVec4{ 0.3f, 0.305f, 0.32f, 1.0f };
		colors[ImGuiCol_FrameBgActive] = ImVec4{ 0.31f, 0.31f, 0.53f, 1.0f };

		// Tabs
		colors[ImGuiCol_Tab] = ImVec4{ 0.22f, 0.22f, 0.36f, 0.631f };
		colors[ImGuiCol_TabHovered] = ImVec4{ 0.24f, 0.24f, 0.52f, 0.66f };
		colors[ImGuiCol_TabActive] = ImVec4{ 0.28f, 0.28f, 0.53f, 1.0f };
		colors[ImGuiCol_TabUnfocused] = ImVec4{ 0.3f, 0.3f, 0.58f, 0.583f };
		colors[ImGuiCol_TabUnfocusedActive] = ImVec4{ 0.24f, 0.24f, 0.47f, 0.534f };

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
#endif
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

	void UI::SetMouseEnabled(bool enable)
	{
		if (enable)
			ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
		else
			ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
	}

}