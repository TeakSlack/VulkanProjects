VULKAN_SDK = os.getenv("VULKAN_SDK")

IncludeDir = {}
IncludeDir["VulkanSDK"] = "%{VULKAN_SDK}/Include"
IncludeDir["spdlog"] = "%{wks.location}/vendor/spdlog/include"
IncludeDir["glfw"] = "%{wks.location}/vendor/glfw/include"
IncludeDir["vk_bootstrap"] = "%{wks.location}/vendor/vk-bootstrap/src"
IncludeDir["glm"] = "%{wks.location}/vendor/glm"

LibraryDir = {}
LibraryDir["VulkanSDK"] = "%{VULKAN_SDK}/Lib"
LibraryDir["glfw"] = "%{wks.location}/vendor/glfw/lib"
LibraryDir["vk_bootstrap"] = "%{wks.location}/vendor/vk-bootstrap/lib"
