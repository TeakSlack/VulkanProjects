include "Dependencies.lua"

workspace "VulkanProjects"
    configurations {"Debug", "Release"}
    platforms {"x86_64"}
    startproject "HelloVulkan"

    outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

group "Projects"
    include "vendor/glfw"
    include "vendor/vk-bootstrap"
    include "01_HelloVulkan"
    include "02_Clear"
    include "03_TriangleUnbuffered"
    include "04_TriangleBuffered"