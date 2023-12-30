outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

VULKAN_SDK = os.getenv("VULKAN_SDK")

project "Compressonator"
    kind "StaticLib"
    language "C"
    staticruntime "off"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "**.h",
        "**.c",
        "**.cxx",
        "**.hpp",
        "**.cpp"
    }

    includedirs
    {
        "%{VULKAN_SDK}/Include",
        "../stb_image",

        "include",
        "include/apc",
        "include/frameworks",
        "include/astc",
        "include/astc/arm",
        "include/atc",
        "include/ati",
        "include/basis",
        "include/bc6h",
        "include/bc7",
        "include/block",
        "include/buffer",
        "include/plugins",

        "include/c3dmodel_viewers/vulkan",
        "include/c3dmodel_viewers/vulkan/util",
        
        "include/cmp_core",
        "include/cmp_core/source",
        "include/cmp_core/shaders",
        "include/cmp_math",
        "include/cmp_meshoptimizer",

        "include/common",
        "include/common/half",
        
        "include/common_plugins",
        "include/common_plugins/d3dx12",
        "include/common_plugins/gltf",
        "include/common_plugins/json",
        "include/common_plugins/model_viewer",
        
        "include/dxt",
        "include/dxtc",
        "include/etc",
        "include/etc/etcpack",
        "include/glm",
        "include/gpu_decode",
        "include/gt",
        "include/stb_image",
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