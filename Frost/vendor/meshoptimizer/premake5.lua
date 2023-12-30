outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

VULKAN_SDK = os.getenv("VULKAN_SDK")

project "MeshOptimizer"
    kind "StaticLib"
    language "C"
    staticruntime "off"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "**.h",
        "**.cpp",
    }

    includedirs
    {
        "include/meshoptimizer",
    }

    defines 
	{ 
		"_CRT_SECURE_NO_WARNINGS",
        "OPTION_BUILD_ASTC",
        'CMAKE_INTDIR="Debug"'
	}

    filter "configurations:Debug"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		runtime "Release"
		optimize "on"