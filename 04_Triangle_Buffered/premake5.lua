project "04_Triangle_Buffered"
        kind "ConsoleApp"
        language "C++"
        cppdialect "C++20"
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
            "%{IncludeDir.spdlog}",
            "%{IncludeDir.glfw}",
            "%{IncludeDir.vk_bootstrap}"
        }

        libdirs 
        { 
            "%{LibraryDir.VulkanSDK}" ,
            "%{LibraryDir.glfw}",
            "%{LibraryDir.vk_bootstrap}"
        }

        links
        {
            "glfw",
            "vk-bootstrap"
        }

        filter "system:windows"
            links 
            { 
                "vulkan-1", -- Vulkan lib for Windows ('vulkan-1.lib')
            }

        filter "system:linux"
            links 
            { 
                "vulkan", -- Vulkan library for Linux (`libvulkan.so`)
            } 

        filter "configurations:Debug"
            defines { "DEBUG", "_DEBUG" }
            symbols "On"

        filter "configurations:Release"
            defines { "NDEBUG" }
            optimize "On"

        filter "action:gmake"
            buildoptions { "-Wall", "-Wextra", "-Werror", "-std=c++20" }