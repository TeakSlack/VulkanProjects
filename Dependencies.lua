VULKAN_SDK = os.getenv("VULKAN_SDK")

IncludeDir = {}
IncludeDir["VulkanSDK"] = "%{VULKAN_SDK}/Include"
IncludeDir["spdlog"] = "%{wks.location}/vendor/spdlog/include"
IncludeDir["glfw"] = "%{wks.location}/vendor/glfw/include"

LibraryDir = {}
LibraryDir["VulkanSDK"] = "%{VULKAN_SDK}/Lib"
LibraryDir["glfw"] = "%{wks.location}/vendor/glfw/lib"