// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
// Hello Vulkan
// 
// Initializes the Vulkan API via creation of an
// instance, enumerates instance layers and extensions,
// enables certain validation layers in debug mode for
// safety, then cleans up Vulkan and quits.
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

#include <vulkan/vulkan.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <iostream>
#include <exception>
#include <vector>

auto logger = spdlog::stdout_color_mt("logger");

// Logs error message and quits application
void error(std::string message)
{
	logger->error(message);
	exit(EXIT_FAILURE);
}

// Main class representing the application
class HelloVulkan
{
public:
	std::string applicationName = "Hello Vulkan";

public:
	// Initialize application objects
	void init()
	{
		create_instance();
		setup_debug_messenger();
	}

	// Destroy application objects
	void destroy()
	{
		vk::DispatchLoaderDynamic dldi(instance, vkGetInstanceProcAddr);
		instance.destroyDebugUtilsMessengerEXT(debugMessenger, nullptr, dldi);
		instance.destroy();
	}

private:
	// Core application objects
	vk::Instance instance;
	vk::DebugUtilsMessengerEXT debugMessenger;

	// Enable validation layers if compiled as debug build
#ifdef NDEBUG
	const bool ENABLE_VALIDATION_LAYERS = false;
#else
	const bool ENABLE_VALIDATION_LAYERS = true;
#endif

	const std::vector<const char*> VALIDATION_LAYERS =
	{
		"VK_LAYER_KHRONOS_validation"
	};

private:
	void create_instance()
	{
		if (ENABLE_VALIDATION_LAYERS && !check_validation_layer_support())
			error("Validation layers requested, but not available!");

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
			debugCreateInfo = get_debug_messenger_create_info();
			createInfo.pNext = &debugCreateInfo; // Pass callback only if validation layers are enabled
		}
		else
		{
			createInfo.enabledLayerCount = 0;
		}

		// Enumerates available extensions
		std::vector<vk::ExtensionProperties> instanceExtensions = vk::enumerateInstanceExtensionProperties();

		// Enable the requested extensions
		auto extensions = get_required_extensions();
		createInfo.enabledExtensionCount = extensions.size();
		createInfo.ppEnabledExtensionNames = extensions.data();

		instance = vk::createInstance(createInfo);
	}

	// Check if requested validation layers are available
	bool check_validation_layer_support()
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

	// Debug callback for validation layer messages
	static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
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

	// Helper to return debug messenger create info
	vk::DebugUtilsMessengerCreateInfoEXT get_debug_messenger_create_info()
	{
		return vk::DebugUtilsMessengerCreateInfoEXT(
			vk::DebugUtilsMessengerCreateFlagsEXT(),
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
			vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
			vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
			vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
			debug_callback,
			nullptr
		);
	}

	// Setup debug messenger if debug build
	void setup_debug_messenger()
	{
		if (!ENABLE_VALIDATION_LAYERS) return;

		vk::DebugUtilsMessengerCreateInfoEXT createInfo = get_debug_messenger_create_info();

		vk::DispatchLoaderDynamic dldi(instance, vkGetInstanceProcAddr);
		debugMessenger = instance.createDebugUtilsMessengerEXT(createInfo, nullptr, dldi);

	}

	// Get required extensions for Vulkan instance
	std::vector<const char*> get_required_extensions()
	{
		std::vector<const char*> extensions;

		if (ENABLE_VALIDATION_LAYERS)
		{
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME); // VK_EXT_debug_utils
		}

		return extensions;
	}
};

// Application main function
int main()
{
	// Set logger output pattern
	logger->set_pattern("[%H:%M:%S %^%l%$]: %v");

	try
	{
		HelloVulkan app;

		app.init();
		app.destroy();
	}
	catch (const std::exception& e)
	{
		error(e.what());
	}

	return EXIT_SUCCESS;
}