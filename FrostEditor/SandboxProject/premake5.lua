ProjectName = "Sandbox"
FrostRootDirectory = "../../"

workspace "%{ProjectName}"
    architecture "x64"
    targetdir "build"
    startproject "%{ProjectName}"

    configurations
    {
        "Debug",
        "Release",
        "Dist"
    }

group "Frost"
project "FrostScriptCore"
    location "%{FrostRootDirectory}FrostScriptCore"
    kind "SharedLib"
    language "C#"

    targetdir ("%{FrostRootDirectory}FrostEditor/Resources/Scripts")
    objdir ("%{FrostRootDirectory}FrostEditor/Resources/Scripts/Intermediates")

    files
    {
        "%{FrostRootDirectory}FrostScriptCore/Source/**.cs"
    }

group ""
project "Sandbox"
    location "Assets/Scripts"
    kind "SharedLib"
    language "C#"

    targetname "Sandbox"

    targetdir ("%{prj.location}/Binaries")
    objdir ("%{FrostRootDirectory}FrostEditor/Resources/Scripts/Intermediates")

    files
    {
        "Assets/Scripts/Source/**.cs"
    }

    links
    {
        "FrostScriptCore"
    }