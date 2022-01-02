workspace "Frost"
	architecture "x64"
	startproject "FrostEditor"		
	
	flags
	{
		"MultiProcessorCompile"
	}

	configurations 
	{
		"Debug",
		"Release",
		"Dist"
	}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

VULKAN_SDK = os.getenv("VULKAN_SDK")

IncludeDir = {}
IncludeDir["GLFW"] = "Frost/vendor/GLFW/include"
IncludeDir["glm"] = "Frost/vendor/glm"
IncludeDir["stb_image"] = "Frost/vendor/stb_image"
IncludeDir["spdlog"] = "Frost/vendor/spdlog/include"
IncludeDir["assimp"] = "Frost/vendor/assimp/assimp/include"
IncludeDir["assimpLib"] = "Frost/vendor/assimp/lib"
IncludeDir["nvvk"] = "Frost/vendor/nvvk/shared_sources"
IncludeDir["vma"] = "Frost/vendor/VulkanMemoryAllocator/include"
IncludeDir["ImGui"] = "Frost/vendor/ImGui"
IncludeDir["ImGuizmo"] = "Frost/vendor/ImGuizmo"
IncludeDir["SPIRV_Cross"] = "Frost/vendor/VulkanSDK/SPIRV-Cross/include"
IncludeDir["yaml_cpp"] = "Frost/vendor/yaml-cpp/include"
IncludeDir["VkBootstrap"] = "Frost/vendor/VkBootstrap/src"
IncludeDir["entt"] = "Frost/vendor/entt/include"

LibraryDir = {}

LibraryDir["VulkanSDK"] = "%{VULKAN_SDK}/Lib"
LibraryDir["VulkanSDK_Debug"] = "%{wks.location}/Frost/vendor/VulkanSDK/Lib"

Library = {}
Library["Vulkan"] = "%{LibraryDir.VulkanSDK}/vulkan-1.lib"
Library["VulkanUtils"] = "%{LibraryDir.VulkanSDK}/VkLayer_utils.lib"

Library["ShaderC_Debug"] = "%{LibraryDir.VulkanSDK_Debug}/shaderc_sharedd.lib"
Library["SPIRV_Cross_Debug"] = "%{LibraryDir.VulkanSDK_Debug}/spirv-cross-cored.lib"
Library["SPIRV_Cross_GLSL_Debug"] = "%{LibraryDir.VulkanSDK_Debug}/spirv-cross-glsld.lib"
Library["SPIRV_Tools_Debug"] = "%{LibraryDir.VulkanSDK_Debug}/SPIRV-Toolsd.lib"

Library["ShaderC_Release"] = "%{LibraryDir.VulkanSDK}/shaderc_shared.lib"
Library["SPIRV_Cross_Release"] = "%{LibraryDir.VulkanSDK}/spirv-cross-core.lib"
Library["SPIRV_Cross_GLSL_Release"] = "%{LibraryDir.VulkanSDK}/spirv-cross-glsl.lib"

group "Dependencies"
	include "Frost/vendor/GLFW"
	include "Frost/vendor/nvvk/shared_sources"
	include "Frost/vendor/ImGui"
	include "Frost/vendor/yaml-cpp"
	include "Frost/vendor/VkBootstrap"
group ""

group "Core"
project "Frost"
	location "Frost"
	kind "StaticLib"
	language "C++"
	cppdialect "C++17"
	staticruntime "off"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	pchsource "Frost/src/Frost/frostpch.cpp"
	pchheader "frostpch.h"

	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp",
		"%{prj.name}/vendor/stb_image/**.h",
		"%{prj.name}/vendor/stb_image/**.cpp",
		"%{prj.name}/vendor/glm/glm/**.hpp",
		"%{prj.name}/vendor/glm/glm/**.inl",

		"%{prj.name}/vendor/ImGuizmo/ImGuizmo.h",
		"%{prj.name}/vendor/ImGuizmo/ImGuizmo.cpp",
		
		"%{prj.name}vendor/nvvk/shared_sources/nvvk/**.hpp",
		"%{prj.name}vendor/nvvk/shared_sources/nvvk/**.cpp",
	}

	--filter "files:vendor/ImGuizmo/**.cpp"
	--flags { "NoPCH" }

	defines
	{
		"_CRT_SECURE_NO_WARNINGS",
		"VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1",
		"VK_USE_PLATFORM_WIN32_KHR"
	}

	includedirs 
	{
		"%{VULKAN_SDK}/Include",
		"%{prj.name}/src",
		"%{prj.name}/src/Frost",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.glm}",
		"%{IncludeDir.stb_image}",
		"%{IncludeDir.assimp}",
		"%{IncludeDir.spdlog}",
		"%{IncludeDir.vma}",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.ImGuizmo}",
		"%{IncludeDir.SPIRV_Cross}",
		"%{IncludeDir.yaml_cpp}",
		"%{IncludeDir.VkBootstrap}",
		"%{IncludeDir.entt}",
		"%{IncludeDir.nvvk}"
	}

	flags
	{
		"MultiProcessorCompile"
	}

	links
	{
		"GLFW",
		"ImGui",
		"VkBootstrap",
		"nvvk",
		"yaml-cpp",

		"assimp-vc142-mt.lib",
		"vulkan-1.lib",
	}

	

	libdirs
	{
		"%{VULKAN_SDK}/Lib",
		"%{IncludeDir.assimpLib}",
	}

	filter "system:windows"
		systemversion "latest"

		links
		{
			"%{Library.ShaderC_Debug}",
			"%{Library.SPIRV_Cross_Debug}",
			"%{Library.SPIRV_Cross_GLSL_Debug}"
		}

	filter "configurations:Debug"
		defines "FROST_DEBUG"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		defines "FROST_RELEASE"
		runtime "Release"
		optimize "on"

	filter "configurations:Dist"
		defines "FROST_DIST"
		runtime "Release"
		optimize "on"

project "FrostScript-Core"
	location "FrostScriptCore"
	kind "ConsoleApp"
	language "C#"
	staticruntime "off"
group ""

project "FrostEditor"
	location "FrostEditor"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++17"
	staticruntime "off"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	
	files
	{
		"%{prj.name}/Source/**.h",
		"%{prj.name}/Source/**.cpp",
		"%{prj.name}/Resources/Shaders/**.glsl"
	}

	includedirs
	{
		"%{VULKAN_SDK}/Include",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.glm}",
		"%{IncludeDir.vma}",
		"%{IncludeDir.nvvk}",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.ImGuizmo}",
		"%{IncludeDir.spdlog}",
		"%{IncludeDir.assimp}",
		"%{IncludeDir.entt}",
		
		"Frost/vendor",
		"Frost/src/Frost",
		"Frost/src",

		"FrostEditor/Source"
	}

	links 
	{
		"Frost"
	}

	filter "system:windows"
		systemversion "latest"

	filter "configurations:Debug"
		defines "FROST_DEBUG"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		defines "FROST_RELEASE"
		runtime "Release"
		optimize "on"

	filter "configurations:Dist"
		defines "FROST_DIST"
		runtime "Release"
		optimize "on"