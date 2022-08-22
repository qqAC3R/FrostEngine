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
IncludeDir["assimp"] = "Frost/vendor/assimp/include"
IncludeDir["assimpLib"] = "Frost/vendor/assimp/lib"
IncludeDir["vma"] = "Frost/vendor/VulkanMemoryAllocator/include"
IncludeDir["ImGui"] = "Frost/vendor/ImGui"
IncludeDir["ImGuizmo"] = "Frost/vendor/ImGuizmo"
IncludeDir["OZZ_Animation"] = "Frost/vendor/ozz-animation/include"
IncludeDir["SPIRV_Cross"] = "Frost/vendor/SPIRV-Cross/include"
IncludeDir["FontAwesome"] = "Frost/vendor/FontAwesome"
IncludeDir["json"] = "Frost/vendor/json/include"

IncludeDir["shaderc"] = "Frost/vendor/shaderc/Include"
IncludeDir["shadercLib"] = "Frost/vendor/shaderc/Lib"

IncludeDir["entt"] = "Frost/vendor/entt/include"

LibraryDir = {}

LibraryDir["VulkanSDK"] = "%{VULKAN_SDK}/Lib"

Library = {}
Library["Vulkan"] = "%{LibraryDir.VulkanSDK}/vulkan-1.lib"
Library["shaderc"] = "Frost/vendor/shaderc/Lib/shaderc_shared.lib"


group "Dependencies"
	include "Frost/vendor/GLFW"
	include "Frost/vendor/SPIRV-Cross"
	include "Frost/vendor/ozz-animation"
	include "Frost/vendor/ImGui"
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
	}

	--filter "files:vendor/ImGuizmo/**.cpp"
	--flags { "NoPCH" }

	defines
	{
		"_CRT_SECURE_NO_WARNINGS",
		"VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1",
		"VK_USE_PLATFORM_WIN32_KHR",
		"GLM_FORCE_DEPTH_ZERO_TO_ONE" -- For voxelization
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
		"%{IncludeDir.entt}",
		"%{IncludeDir.SPIRV_Cross}",
		"%{IncludeDir.OZZ_Animation}",
		"%{IncludeDir.json}",
		"%{IncludeDir.FontAwesome}"
	}

	flags
	{
		"MultiProcessorCompile"
	}

	links
	{
		"GLFW",
		"ImGui",
		"SPIR-V_Cross",
		"OZZ-Animation",

		"shaderc_shared.lib",
		"vulkan-1.lib"
	}

	libdirs
	{
		"%{VULKAN_SDK}/Lib",
		"%{IncludeDir.assimpLib}",
		"%{IncludeDir.shadercLib}"
	}

	libdirs
	{
		"%{IncludeDir.assimpLib}/Release"
	}
	links
	{
		"assimp-vc143-mt.lib"
	}



	filter "system:windows"
		systemversion "latest"

	filter "configurations:Debug"
		defines "FROST_DEBUG"
		runtime "Debug"
		symbols "on"
--		libdirs
--		{
--			"%{IncludeDir.assimpLib}/Debug"
--		}
--		links
--		{
--			"assimp-vc143-mtd.lib"
--		}

	filter "configurations:Release"
		defines "FROST_RELEASE"
		runtime "Release"
		optimize "on"
--		libdirs
--		{
--			"%{IncludeDir.assimpLib}/Release"
--		}
--		links
--		{
--			"assimp-vc143-mt.lib"
--		}


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
		"%{IncludeDir.OZZ_Animation}",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.ImGuizmo}",
		"%{IncludeDir.spdlog}",
		"%{IncludeDir.assimp}",
		"%{IncludeDir.entt}",
		"%{IncludeDir.FontAwesome}",
		"%{IncludeDir.json}",
		
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