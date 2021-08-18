--workspace "NVPro"
--	architecture "x64"
--	startproject "Sandbox"		
--		
--	configurations 
--	{
--		"Debug",
--		"Release",
--		"Dist"
--	}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

VULKAN_SDK = os.getenv("VULKAN_SDK")

project "nvvk"
    kind "StaticLib"
    language "C"
    staticruntime "off"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "**.h",
        "**.hpp",
        "**.cpp"
    }

    includedirs
    {
        "%{VULKAN_SDK}/Include",
        
        "imgui",
        "../shared_external/zlib/include",
        "../shared_external/glfw3/include",
        "../shared_external",
        "../shared_sources"
        
    }

    defines 
	{ 
		"_CRT_SECURE_NO_WARNINGS",
        "WIN32",
        "_WINDOWS",
        'RESOURCE_DIRECTORY="D:/Visual Studio/Vulkan/NVVK/shared_sources/resources/"',
        "NOMINMAX",
        "USEVULKANSDK",
        "GLFW_INCLUDE_VULKAN",
        "VK_ENABLE_BETA_EXTENSIONS",
        "USEOPENGL",
        "USEIMGUI",
        "CSF_ZIP_SUPPORT=1",
        "VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1",
        "VK_USE_PLATFORM_WIN32_KHR",
        "_CRT_SECURE_NO_WARNINGS",
        'CMAKE_INTDIR="Debug"'
	}



    filter "configurations:Debug"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		runtime "Release"
		optimize "on"