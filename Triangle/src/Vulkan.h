#pragma once

#include <vector>
#include <vulkan/vulkan.hpp>

namespace Vulkan
{
	std::vector<const char*> GetRequiredExtensions();
	bool CheckValidationLayerSupport();
	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData
	);
	vk::DebugUtilsMessengerCreateInfoEXT GetDebugMessengerCreateInfo();
	void SetupDebugMessenger();
	void CreateInstance();
	void DestroyInstance();
}