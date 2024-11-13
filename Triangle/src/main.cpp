#include <exception>
#include <iostream>
#include <vector>

#include <vulkan/vulkan.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <GLFW/glfw3.h>

#include "VkBootstrap.h"

auto logger = spdlog::stdout_color_mt("logger");

vkb::Instance instance;
vkb::PhysicalDevice physicalDevice;
vkb::Device device;
VkSurfaceKHR surface;
VkQueue presentQueue;
GLFWwindow* window;

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

void CreateGLFWWindow()
{
	if (!glfwInit()) exit(EXIT_FAILURE);

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	window = glfwCreateWindow(1280, 720, "Triangle", NULL, NULL);
	VkResult err = glfwCreateWindowSurface(instance.instance, window, NULL, &surface);

	if (err != VK_SUCCESS) exit(EXIT_FAILURE);
}

bool CreateInstance()
{
	vkb::InstanceBuilder instanceBuilder;
	
	uint32_t glfwCount = 0;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwCount);

	std::vector<const char*> requiredExtensions(glfwExtensions, glfwExtensions + glfwCount);

#ifdef DEBUG
	instanceBuilder.request_validation_layers();
	instanceBuilder.set_debug_callback(DebugCallback);
#endif
	
	instanceBuilder.set_app_name("Triangle");
	instanceBuilder.set_app_version(VK_MAKE_VERSION(1, 0, 0));
	instanceBuilder.enable_extensions(requiredExtensions);
	auto instanceReturn = instanceBuilder.build();

	if (!instanceReturn)
	{
		logger->error("Failed to create Vulkan instance: " + instanceReturn.error().message());
		return false;
	}

	instance = instanceReturn.value();

	return true;
}

void CreateDevice()
{
	vkb::PhysicalDeviceSelector physSelector(instance);

	VkPhysicalDeviceFeatures features{};
	features.geometryShader = true;

	auto physReturn = physSelector
		.set_surface(surface)
		.set_required_features(features)
		.select();
	if (!physReturn)
	{
		logger->error("Failed to create physical device: " + physReturn.error().message());
		exit(EXIT_FAILURE);
	}

	physicalDevice = physReturn.value();

	vkb::DeviceBuilder deviceBuilder{ physicalDevice };
	auto deviceReturn = deviceBuilder.build();
	if (!deviceReturn)
	{
		logger->error("Failed to create device: " + deviceReturn.error().message());
		exit(EXIT_FAILURE);
	}

	device = deviceReturn.value();
}

void GetQueues()
{
	auto presentQueueRet = device.get_queue(vkb::QueueType::present);
	if (!presentQueueRet)
	{
		logger->error("Failed to get presentation queue: " + presentQueueRet.error().message());
		exit(EXIT_FAILURE);
	}

	presentQueue = presentQueueRet.value();
}

void DestroyGLFWWindow()
{
	glfwDestroyWindow(window);
	glfwTerminate();
}

void DestroyDevice()
{
	vkb::destroy_device(device);
}

void DestroyInstance()
{
	vkDestroySurfaceKHR(instance.instance, surface, NULL);
	vkb::destroy_instance(instance);
}

void Run()
{

	logger->set_pattern("[%H:%M:%S %^%l%$]: %v"); // [Hour:Minute:Second Level]: Message

	if (!CreateInstance()) exit(EXIT_FAILURE);
	CreateGLFWWindow();
	CreateDevice();

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
	}

	DestroyDevice();
	DestroyInstance();
	DestroyGLFWWindow();
}

int main()
{
	try
	{
		Run();
	}
	catch (const std::exception& e)
	{
		
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}