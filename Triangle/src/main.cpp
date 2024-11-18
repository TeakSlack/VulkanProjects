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
std::vector<VkFramebuffer> swapFramebuffers;
VkRenderPass renderPass;
VkPipelineLayout pipelineLayout;
VkPipeline graphicsPipeline;
VkCommandPool commandPool;
VkCommandBuffer commandBuffer;

VkSemaphore imageAvaliableSemaphore;
VkSemaphore renderFinishedSemaphore;
VkFence inFlightFence;

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

	VkPhysicalDeviceSynchronization2Features synchronization2{};
	synchronization2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
	synchronization2.synchronization2 = true;

	vkb::DeviceBuilder deviceBuilder{ physicalDevice };
	deviceBuilder.add_pNext(&synchronization2);
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

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

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

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

	if (vkCreatePipelineLayout(device.device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
		logger->error("Failed to create pipeline layout!");
		exit(EXIT_FAILURE);
	}

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = pipelineLayout;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0;

	if (vkCreateGraphicsPipelines(device.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
	{
		logger->error("Failed to create graphics pipeline!");
		exit(EXIT_FAILURE);
	}

	vkDestroyShaderModule(device.device, vertModule, nullptr);
	vkDestroyShaderModule(device.device, fragModule, nullptr);
}

void CreateFramebuffers()
{
	swapFramebuffers.resize(swapImageViews.size());

	for (size_t i = 0; i < swapFramebuffers.size(); i++)
	{
		VkImageView attachments[] =
		{
			swapImageViews[i]
		};

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = swapExtent.width;
		framebufferInfo.height = swapExtent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(device.device, &framebufferInfo, nullptr, &swapFramebuffers[i]) != VK_SUCCESS)
		{
			logger->error("Failed to create framebufffers!");
			exit(EXIT_FAILURE);
		}
	}
}

void CreateCommandPool()
{
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = device.get_queue_index(vkb::QueueType::graphics).value();

	if (vkCreateCommandPool(device.device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
	{
		logger->error("Failed to create command pool!");
		exit(EXIT_FAILURE);
	}
}

void CreateCommandBuffer()
{
	VkCommandBufferAllocateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	bufferInfo.commandPool = commandPool;
	bufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	bufferInfo.commandBufferCount = 1;

	if (vkAllocateCommandBuffers(device.device, &bufferInfo, &commandBuffer) != VK_SUCCESS)
	{
		logger->error("Failed to allocate command buffers!");
		exit(EXIT_FAILURE);
	}
}

void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIdx)
{
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
	{
		logger->error("Failed to begin recording command buffer!");
		exit(EXIT_FAILURE);
	}

	VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = renderPass;
	renderPassInfo.framebuffer = swapFramebuffers[imageIdx];
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = swapExtent;
	renderPassInfo.clearValueCount = 1;
	renderPassInfo.pClearValues = &clearColor;

	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(swapExtent.width);
	viewport.height = static_cast<float>(swapExtent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = swapExtent;
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	vkCmdDraw(commandBuffer, 3, 1, 0, 0);
	vkCmdEndRenderPass(commandBuffer);

	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
	{
		logger->error("Failed to record command buffer!");
		exit(EXIT_FAILURE);
	}
}

void CreateSyncObjects()
{
	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	if (
		vkCreateSemaphore(device.device, &semaphoreInfo, nullptr, &imageAvaliableSemaphore) != VK_SUCCESS ||
		vkCreateSemaphore(device.device, &semaphoreInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS ||
		vkCreateFence(device.device, &fenceInfo, nullptr, &inFlightFence) != VK_SUCCESS
		)
	{
		logger->error("Failed to create syncronization objects!");
		exit(EXIT_FAILURE);
	}
}

void DrawFrame()
{
	vkWaitForFences(device.device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
	vkResetFences(device.device, 1, &inFlightFence);

	uint32_t imageIdx;
	vkAcquireNextImageKHR(device.device, swapchain, UINT64_MAX, imageAvaliableSemaphore, VK_NULL_HANDLE, &imageIdx);
	vkResetCommandBuffer(commandBuffer, 0);
	RecordCommandBuffer(commandBuffer, imageIdx);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = { imageAvaliableSemaphore };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	VkSemaphore signalSemaphores[] = { renderFinishedSemaphore };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence) != VK_SUCCESS)
	{
		logger->error("Failed to submit queue for rendering!");
		exit(EXIT_FAILURE);
	}

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	VkSwapchainKHR swapchains[] = { swapchain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapchains;
	presentInfo.pImageIndices = &imageIdx;

	vkQueuePresentKHR(graphicsQueue, &presentInfo);
}

void DestroySyncObjects()
{
	vkDestroySemaphore(device.device, imageAvaliableSemaphore, nullptr);
	vkDestroySemaphore(device.device, renderFinishedSemaphore, nullptr);
	vkDestroyFence(device.device, inFlightFence, nullptr);
}

void DestroyCommandBuffer()
{
	vkDestroyCommandPool(device.device, commandPool, nullptr);
}

void DestroyCommandPool()
{
	vkDestroyCommandPool(device.device, commandPool, nullptr);
}

void DestroyFramebuffers()
{
	for (auto framebuffer : swapFramebuffers)
	{
		vkDestroyFramebuffer(device.device, framebuffer, nullptr);
	}
}

void DestroyRenderPass()
{
	vkDestroyRenderPass(device.device, renderPass, nullptr);
}

void DestroyGraphicsPipeline()
{
	vkDestroyPipeline(device.device, graphicsPipeline, nullptr);
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
	GetQueues();
	CreateSwapchain();
	CreateRenderPass();
	CreateGraphicsPipeline();
	CreateFramebuffers();
	CreateCommandPool();
	CreateCommandBuffer();
	CreateSyncObjects();

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
		DrawFrame();
	}

	vkDeviceWaitIdle(device.device);

	DestroySyncObjects();
	DestroyCommandBuffer();
	DestroyFramebuffers();
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