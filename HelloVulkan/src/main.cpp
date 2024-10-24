#include <vulkan/vulkan.hpp>
#include <iostream>
#include <exception>
#include <vector>

// The connection between the application and Vulkan API
vk::Instance instance;

const std::vector<const char*> VALIDATION_LAYERS =
{
	"VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
const bool ENABLE_VALIDATION_LAYERS = false;
#else
const bool ENABLE_VALIDATION_LAYERS = true;
#endif


bool checkValidationLayerSupport()
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

void CreateInstance()
{
	if (ENABLE_VALIDATION_LAYERS && !checkValidationLayerSupport())
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

	// Include validation layer names, if enabled
	if (ENABLE_VALIDATION_LAYERS)
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>(VALIDATION_LAYERS.size());
		createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();
	}
	else
	{
		createInfo.enabledLayerCount = 0;
	}

	// Enumerates and displays available extensions
	std::vector<vk::ExtensionProperties> instanceExtensions = vk::enumerateInstanceExtensionProperties();

	std::cout << "Avaliable instance extensions:" << std::endl;
	for (auto& ext : instanceExtensions)
	{
		std::cout << "\t" << ext.extensionName << std::endl;
	}

	instance = vk::createInstance(createInfo);
}

void Initialize()
{
	CreateInstance();
}

void Cleanup()
{
	instance.destroy();
}

void Run()
{
	Initialize();
	Cleanup();
}

int main()
{
	try
	{
		Run();
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}