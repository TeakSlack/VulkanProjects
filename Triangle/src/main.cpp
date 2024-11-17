#include <exception>
#include <iostream>
#include <vector>
#include <fstream>

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
VkRenderPass renderPass;
VkPipelineLayout pipelineLayout;

static std::vector<char> readFile(const std::string& fileName)
{
	std::ifstream file(fileName, std::ifstream::ate | std::ifstream::binary);

	if (!file.is_open())
	{
		logger->error("Failed to open file " + fileName);
		exit(EXIT_FAILURE);
	}

	size_t fileSize = file.tellg();
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);

	file.close();
	return buffer;
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
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME
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
	swapImageViews = swapchain.get_image_views().value();
}

VkShaderModule CreateShaderModule(const std::vector<char>& code)
{
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule module;
	if (vkCreateShaderModule(device.device, &createInfo, nullptr, &module) != VK_SUCCESS)
	{
		logger->error("Failed to create shader module!");
		exit(EXIT_FAILURE);
	}

	return module;
}

void CreateRenderPass()
{
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = swapFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;

	if (vkCreateRenderPass(device.device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
	{
		logger->error("Failed to create render pass!");
		exit(EXIT_FAILURE);
	}
}

void CreateGraphicsPipeline()
{
	auto vertShader = readFile("src/shader/vert.spv");
	auto fragShader = readFile("src/shader/frag.spv");

	VkShaderModule vertModule = CreateShaderModule(vertShader);
	VkShaderModule fragModule = CreateShaderModule(fragShader);

	VkPipelineShaderStageCreateInfo vertStageInfo{};
	vertStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertStageInfo.module = vertModule;
	vertStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragStageInfo{};
	fragStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragStageInfo.module = fragModule;
	fragStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertStageInfo, fragStageInfo };

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.vertexAttributeDescriptionCount = 0;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)swapExtent.width;
	viewport.height = (float)swapExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = swapExtent;

	std::vector<VkDynamicState> dynamicStates
	{
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

	if (vkCreatePipelineLayout(device.device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
		logger->error("Failed to create pipeline layout!");
		exit(EXIT_FAILURE);
	}

	vkDestroyShaderModule(device.device, vertModule, nullptr);
	vkDestroyShaderModule(device.device, fragModule, nullptr);
}

void DestroyRenderPass()
{
	vkDestroyRenderPass(device.device, renderPass, nullptr);
}

void DestroyGraphicsPipeline()
{
	vkDestroyPipelineLayout(device.device, pipelineLayout, nullptr);
}

void DestroySwapchain()
{
	swapchain.destroy_image_views(swapImageViews);
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
	CreateRenderPass();
	CreateGraphicsPipeline();

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
	}

	DestroyGraphicsPipeline();
	DestroyRenderPass();
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