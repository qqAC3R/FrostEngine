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
IncludeDir["vma"] = "Frost/vendor/VulkanMemoryAllocator/include"
IncludeDir["ImGui"] = "Frost/vendor/ImGui"
IncludeDir["ImGuizmo"] = "Frost/vendor/ImGuizmo"
IncludeDir["PhysX"] = "Frost/vendor/PhysX/include"
IncludeDir["OZZ_Animation"] = "Frost/vendor/ozz-animation/include"
IncludeDir["SPIRV_Cross"] = "Frost/vendor/SPIRV-Cross/include"
IncludeDir["FontAwesome"] = "Frost/vendor/FontAwesome"
IncludeDir["json"] = "Frost/vendor/json/include"
IncludeDir["shaderc"] = "Frost/vendor/shaderc/Include"
IncludeDir["entt"] = "Frost/vendor/entt/include"
IncludeDir["Mono"] = "Frost/vendor/mono/include"

LibraryDir = {}

LibraryDir["VulkanSDK"] = "%{VULKAN_SDK}/Lib"
LibraryDir["PhysX"] = "Frost/vendor/PhysX/lib/%{cfg.buildcfg}"
LibraryDir["shaderc"] = "Frost/vendor/shaderc/Lib"
LibraryDir["assimp"] = "Frost/vendor/assimp/lib"
LibraryDir["Mono"] = "Frost/vendor/mono/lib/%{cfg.buildcfg}"

Library = {}

Library["Mono"] = "%{LibraryDir.Mono}/mono-2.0-sgen.lib"

Library["Vulkan"] = "%{LibraryDir.VulkanSDK}/vulkan-1.lib"
Library["shaderc"] = "Frost/vendor/shaderc/Lib/shaderc_shared.lib"

Library["PhysX"] =                   "%{LibraryDir.PhysX}/PhysX_static_64.lib"
Library["PhysXCharacterKinematic"] = "%{LibraryDir.PhysX}/PhysXCharacterKinematic_static_64.lib"
Library["PhysXCommon"] =             "%{LibraryDir.PhysX}/PhysXCommon_static_64.lib"
Library["PhysXCooking"] =            "%{LibraryDir.PhysX}/PhysXCooking_static_64.lib"
Library["PhysXExtensions"] =         "%{LibraryDir.PhysX}/PhysXExtensions_static_64.lib"
Library["PhysXFoundation"] =         "%{LibraryDir.PhysX}/PhysXFoundation_static_64.lib"
Library["PhysXPvd"] =                "%{LibraryDir.PhysX}/PhysXPvdSDK_static_64.lib"

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
		"GLM_FORCE_DEPTH_ZERO_TO_ONE", -- For voxelization
		"PX_PHYSX_STATIC_LIB" -- To use PhysX static libraries
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
		"%{IncludeDir.json}/json",
		"%{IncludeDir.PhysX}",
		"%{IncludeDir.FontAwesome}",
		"%{IncludeDir.Mono}"
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

		"%{Library.Vulkan}", -- vulkan-1.lib
		"shaderc_shared.lib",

		"assimp-vc143-mt.lib",

		--"CMP_Framework_MD.lib",

		"mono-2.0-sgen.lib",

		-- PhysX
		"PhysX_static_64.lib",
		"PhysXCharacterKinematic_static_64.lib",
		"PhysXCommon_static_64.lib",
		"PhysXCooking_static_64.lib",
		"PhysXExtensions_static_64.lib",
		"PhysXFoundation_static_64.lib",
		"PhysXPvdSDK_static_64.lib"
	}

	libdirs
	{
		"%{VULKAN_SDK}/Lib",

		--"%{LibraryDir.assimp}",
		"%{LibraryDir.assimp}/Release",

		"%{LibraryDir.Mono}",

		--"%{LibraryDir.Compressonator}",

		"%{LibraryDir.shaderc}",
		"%{LibraryDir.PhysX}"
	}

	filter "system:windows"
		systemversion "latest"

	filter "configurations:Debug"
		defines "FROST_DEBUG"
		runtime "Debug"
		symbols "on"

		defines
		{
			"_DEBUG"
		}

	filter "configurations:Release"
		defines "FROST_RELEASE"
		runtime "Release"
		optimize "on"

		defines
		{
			"NDEBUG"
		}


	filter "configurations:Dist"
		defines "FROST_DIST"
		runtime "Release"
		optimize "on"

		defines
		{
			"NDEBUG"
		}

project "FrostScriptCore"
	location "FrostScriptCore"
	kind "SharedLib"
	language "C#"
	dotnetframework "4.7.2"

	targetdir("%{wks.location}/FrostEditor/Resources/Scripts")
	objdir("%{wks.location}/FrostEditor/Resources/Scripts/Intermediates")

	files
	{
		"%{prj.name}/Source/**.cs",
		"%{prj.name}/Properties/**.cs",
	}

	filter "configurations:Debug"
		optimize "Off"
		symbols "Default"

	filter "configurations:Release"
		optimize "On"
		symbols "Default"

	filter "configurations:Dist"
		optimize "Full"
		symbols "Off"

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
		"%{IncludeDir.json}/json",
		
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

		defines
		{
			"_DEBUG"
		}

	filter "configurations:Release"
		defines "FROST_RELEASE"
		runtime "Release"
		optimize "on"

		defines
		{
			"NDEBUG"
		}

	filter "configurations:Dist"
		defines "FROST_DIST"
		runtime "Release"
		optimize "on"

		defines
		{
			"NDEBUG"
		}