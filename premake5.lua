workspace "VulkanProjects"
    configurations {"Debug", "Release"}
    platforms {"x64"}
    startproject "HelloWorld"

function setupProject(name)
    project (name)
        kind "ConsoleApp"
        language "C++"
        cppdialect "C++17"
        targetdir ( "bin/%{cfg.buildcfg}/" )
        objdir ( "bin-int/" )

        location("src/" .. name)

        files { name .. "/src/**.h", name .. "/src/**.hpp", name .. "/src/**.cpp", "include/**.h", "include/**.hpp" }

        includedirs {
            "include",
            "$(VULKAN_SDK)/Include"
        }

        filter "system:windows"
            libdirs { "$(VULKAN_SDK)/Lib" }
            links { "vulkan-1" } -- Vulkan lib for Windows

        filter "system:linux"
            libdirs { "$(VULKAN_SDK)/lib" }
            links { "vulkan" }  -- Vulkan library for Linux (`libvulkan.so`)

        filter "configurations:Debug"
            defines { "DEBUG", "_DEBUG" }
            symbols "On"

        filter "configurations:Release"
            defines { "NDEBUG" }
            optimize "On"

        filter "action:gmake"
            buildoptions { "-Wall", "-Wextra", "-Werror", "-std=c++17" }
    end

setupProject("HelloVulkan")