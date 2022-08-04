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

project "SPIR-V_Cross"
    kind "StaticLib"
    language "C"
    staticruntime "off"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "include/**.h",
        "include/**.hpp",
        "include/**.cpp"
    }

    includedirs
    {
        "%{VULKAN_SDK}/Include",
        "include",
    }

    defines 
	{ 
		"_CRT_SECURE_NO_WARNINGS",
        "USEVULKANSDK",
        "_CRT_SECURE_NO_WARNINGS",
        'CMAKE_INTDIR="Debug"'
	}



    filter "configurations:Debug"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		runtime "Release"
		optimize "on"