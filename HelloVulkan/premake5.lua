project "HelloVulkan"
        kind "ConsoleApp"
        language "C++"
        cppdialect "C++17"
        targetdir ( "%{wks.location}/bin/" .. outputdir .. "/%{prj.name}" )
        objdir ( "%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}" )

        files 
        { 
            "src/**.h",
            "src/**.cpp",
        }

        includedirs 
        {
            "%{IncludeDir.VulkanSDK}",
            "%{IncludeDir.spdlog}"
        }

        libdirs 
        { 
            "%{LibraryDir.VulkanSDK}" 
        }

        filter "system:windows"
            links { "vulkan-1" } -- Vulkan lib for Windows

        filter "system:linux"
            links { "vulkan" }  -- Vulkan library for Linux (`libvulkan.so`)

        filter "configurations:Debug"
            defines { "DEBUG", "_DEBUG" }
            symbols "On"

        filter "configurations:Release"
            defines { "NDEBUG" }
            optimize "On"

        filter "action:gmake"
            buildoptions { "-Wall", "-Wextra", "-Werror", "-std=c++17" }