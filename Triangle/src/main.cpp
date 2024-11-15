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
vkb::Swapchain swapchain;
VkSurfaceKHR surface;
VkQueue presentQueue;
VkQueue graphicsQueue;
GLFWwindow* window;

std::vector<VkImage> swapImages;
std::vector<VkImageView> swapImageViews;
VkFormat swapFormat;
VkExtent2D swapExtent;

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

	std::vector<const char*> extensions
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	auto physReturn = physSelector
		.set_surface(surface)
		.add_required_extensions(extensions)
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

	auto graphicsQueueRet = device.get_queue(vkb::QueueType::graphics);
	if (!graphicsQueueRet)
	{
		logger->error("Failed to get graphics queue: " + graphicsQueueRet.error().message());
		exit(EXIT_FAILURE);
	}

	presentQueue = presentQueueRet.value();
	graphicsQueue = graphicsQueueRet.value();
}

void CreateSwapchain()
{
	vkb::SwapchainBuilder swapBuilder{ device };

	int width, height;
	glfwGetFramebufferSize(window, &width, &height);

	swapBuilder.use_default_format_selection();
	swapBuilder.use_default_present_mode_selection();
	swapBuilder.use_default_image_usage_flags();
	swapBuilder.set_desired_extent(width, height);
	swapBuilder.set_image_array_layer_count(1);

	auto swapRet = swapBuilder.build();

	if (!swapRet)
	{
		logger->error("Failed to create swapchain: " + swapRet.error().message());
		exit(EXIT_FAILURE);
	}

	swapchain = swapRet.value();

	swapImages = swapchain.get_images().value();
	swapFormat = swapchain.image_format;
	swapExtent = swapchain.extent;
}

void CreateImageViews()
{
	swapImageViews.resize(swapImages.size());

	for (size_t i = 0; i < swapImageViews.size(); i++)
	{
		VkImageViewCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = swapImages[i];
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = swapFormat;
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(device.device, &createInfo, nullptr, &swapImageViews[i]) != VK_SUCCESS)
		{
			logger->error("Failed to create image view!");
			exit(EXIT_FAILURE);
		}
	}
}

void DestroyImageViews()
{
	for (auto imageView : swapImageViews)
	{
		vkDestroyImageView(device.device, imageView, nullptr);
	}
}

void DestroySwapchain()
{
	vkb::destroy_swapchain(swapchain);
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
	CreateSwapchain();
	CreateImageViews();

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
	}

	DestroyImageViews();
	DestroySwapchain();
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