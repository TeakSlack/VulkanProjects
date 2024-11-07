include "Dependencies.lua"

workspace "VulkanProjects"
    configurations {"Debug", "Release"}
    platforms {"x86_64"}
    startproject "HelloVulkan"

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

group "Projects"
    include "HelloVulkan"
    include "Triangle"
