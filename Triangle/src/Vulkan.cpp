#include "Globals.h"

namespace Vulkan
{
	const std::vector<const char*> VALIDATION_LAYERS =
	{
		"VK_LAYER_KHRONOS_validation"
	};

#ifdef NDEBUG
	const bool ENABLE_VALIDATION_LAYERS = false;
#else
	const bool ENABLE_VALIDATION_LAYERS = true;
#endif

	std::vector<const char*> GetRequiredExtentions()
	{
		std::vector<const char*> extentions;

		if (ENABLE_VALIDATION_LAYERS)
		{
			extentions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME); // VK_EXT_debug_utils
		}

		return extentions;
	}

	bool CheckValidationLayerSupport()
	{
		// Get all available validation layers
		std::vector<vk::LayerProperties> validationLayers = vk::enumerateInstanceLayerProperties();

		// Check if requested validation layers are available
		for (const auto& layerName : VALIDATION_LAYERS)
		{
			bool layerFound = false;

			for (const auto& layer : validationLayers)
			{
				if (strcmp(layerName, layer.layerName) == 0)
				{
					layerFound = true;
					break;
				}
			}

			return layerFound;
		}

		return true;
	}

	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData
	)
	{
		switch (messageSeverity)
		{
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
			logger->info(pCallbackData->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
			logger->warn(pCallbackData->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
			logger->error(pCallbackData->pMessage);
			break;
		default:
			break;
		}

		return VK_FALSE; // Always return false unless testing validation layer itself, triggers exception
	}

	vk::DebugUtilsMessengerCreateInfoEXT GetDebugMessengerCreateInfo()
	{
		return vk::DebugUtilsMessengerCreateInfoEXT(
			vk::DebugUtilsMessengerCreateFlagsEXT(),
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
			vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
			vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
			vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
			DebugCallback,
			nullptr
		);
	}

	void SetupDebugMessenger()
	{
		if (!ENABLE_VALIDATION_LAYERS) return;

		vk::DebugUtilsMessengerCreateInfoEXT createInfo = GetDebugMessengerCreateInfo();

		vk::DispatchLoaderDynamic dldi(instance, vkGetInstanceProcAddr);
		debugMessenger = instance.createDebugUtilsMessengerEXT(createInfo, nullptr, dldi);

	}

	void CreateInstance()
	{
		if (ENABLE_VALIDATION_LAYERS && !CheckValidationLayerSupport())
			throw std::runtime_error("Validation layers requested, but not available!");

		vk::ApplicationInfo appInfo
		{
			"Hello Vulkan",				// Application name
			VK_MAKE_VERSION(1, 0, 0),	// Application version
			"No Engine",				// Engine name (none in this case)
			VK_MAKE_VERSION(0, 0, 0),	// Engine version (0.0.0 for no engine)
			VK_API_VERSION_1_3			// Vulkan API version
		};

		vk::InstanceCreateInfo createInfo
		{
			vk::InstanceCreateFlags(),	// Sets flags for instance creation
			&appInfo,					// Application info
			0, nullptr					// No enabled layers by default

		};

		// Debug create info to pass to pNext, so we can get diagnostics on instance creation and destruction
		vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo{};

		// Include validation layer names, if enabled
		if (ENABLE_VALIDATION_LAYERS)
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(VALIDATION_LAYERS.size());
			createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();
			debugCreateInfo = GetDebugMessengerCreateInfo();
			createInfo.pNext = &debugCreateInfo; // Pass callback only if validation layers are enabled
		}
		else
		{
			createInfo.enabledLayerCount = 0;
		}

		// Enumerates available extensions
		std::vector<vk::ExtensionProperties> instanceExtensions = vk::enumerateInstanceExtensionProperties();

		// Enable the requested extentions
		auto extentions = GetRequiredExtentions();
		createInfo.enabledExtensionCount = extentions.size();
		createInfo.ppEnabledExtensionNames = extentions.data();

		instance = vk::createInstance(createInfo);
	}

	void DestroyInstance()
	{
		vk::DispatchLoaderDynamic dldi(instance, vkGetInstanceProcAddr);
		instance.destroyDebugUtilsMessengerEXT(debugMessenger, nullptr, dldi);
		instance.destroy();
	}
}