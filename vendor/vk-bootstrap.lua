project "vk-bootstrap"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"

    targetdir ( "%{wks.location}/vendor/vk-bootstrap/lib/" )
    objdir ( "%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}" )

    files
    {
        "vk-bootstrap/src/VkBootstrap.cpp",
        "vk-bootstrap/src/VkBootstrap.h",
        "vk-bootstrap/src/VkBootstrapDispatch.h"
    }

    includedirs 
    {
        "%{IncludeDir.VulkanSDK}"
    }

    filter "configurations:Debug"
        defines { "DEBUG", "_DEBUG" }
        symbols "On"

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"