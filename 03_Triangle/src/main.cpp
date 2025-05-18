#include <exception>
#include <iostream>
#include <vector>
#include <fstream>
#include <unordered_set>

#include <vulkan/vulkan.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <GLFW/glfw3.h>

#include "VkBootstrap.h"

const int MAX_FRAMES_IN_FLIGHT = 2;

auto logger = spdlog::stdout_color_mt("logger");

void error(std::string message)
{
	logger->error("An error has occurred: " + message);
	exit(EXIT_FAILURE);
}

class Triangle
{
public:
	std::string application_name = "Triangle";

public:
	void init()
	{
		create_base_objects();
		create_render_pass();
		create_graphics_pipeline();
		create_framebuffers();
		create_command_objects();
		create_sync_objects();
	}

	void run()
	{
		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();
			draw_frame();
		}
	}

	void destroy()
	{
		device.waitIdle();
		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			device.destroySemaphore(renderFinishedSemaphores[i]);
			device.destroySemaphore(imageAvailableSemaphores[i]);
			device.destroyFence(inFlightFences[i]);
		}
		device.destroyCommandPool(commandPool);
		for (const auto& framebuffer : swapFramebuffers) device.destroyFramebuffer(framebuffer);
		device.destroyPipeline(graphicsPipeline);
		device.destroyPipelineLayout(pipelineLayout);
		device.destroyRenderPass(renderPass);
		for (const auto& imageView : swapImageViews) device.destroyImageView(imageView);
		device.destroySwapchainKHR(swapchain);
		instance.destroySurfaceKHR(surface);
		device.destroy();
#ifdef DEBUG
		vk::DispatchLoaderDynamic dldi(instance, vkGetInstanceProcAddr);
		instance.destroyDebugUtilsMessengerEXT(debug_messenger, nullptr, dldi);
#endif
		instance.destroy();
		glfwDestroyWindow(window);
	}

private:
	GLFWwindow* window;

	vk::Instance instance;
	vk::DebugUtilsMessengerEXT debug_messenger;
	vk::PhysicalDevice physicalDevice;
	vk::Device device;
	vk::SurfaceKHR surface;
	uint32_t presentIdx, graphicsIdx;
	vk::Queue presentQueue, graphicsQueue;
	vk::SwapchainKHR swapchain;
	std::vector<vk::Image> swapImages;
	std::vector<vk::ImageView> swapImageViews;
	vk::Extent2D swapExtent;
	vk::Format swapFormat;
	vk::RenderPass renderPass;
	vk::PipelineLayout pipelineLayout;
	vk::Pipeline graphicsPipeline;
	std::vector<vk::Framebuffer> swapFramebuffers;
	vk::CommandPool commandPool;
	std::vector<vk::CommandBuffer> commandBuffers;
	std::vector<vk::Semaphore> imageAvailableSemaphores, renderFinishedSemaphores;
	std::vector<vk::Fence> inFlightFences;
	uint32_t currentFrame = 0;

private:
	static std::vector<char> read_file(const std::string& fileName)
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

	void create_window()
	{
		if (!glfwInit()) exit(EXIT_FAILURE);

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		window = glfwCreateWindow(1280, 720, "Triangle", NULL, NULL);
		if (!window) error("Failed to create window!");

		VkResult err = glfwCreateWindowSurface(instance, window, NULL, reinterpret_cast<VkSurfaceKHR*>(&surface));
		if (err != VK_SUCCESS) error("Failed to create window surface");
	}

	void create_base_objects()
	{
		uint32_t glfwCount = 0;
		const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwCount); // Get GLFW's required extensions
		std::vector<const char*> requiredExtensions(glfwExtensions, glfwExtensions + glfwCount); // Create array of required extensions

		vkb::InstanceBuilder instanceBuilder;
		instanceBuilder.set_app_name(application_name.c_str());
		instanceBuilder.set_app_version(VK_MAKE_VERSION(1, 0, 0));
		instanceBuilder.set_engine_name("No Engine");
		instanceBuilder.set_engine_version(VK_MAKE_VERSION(1, 0, 0));
		instanceBuilder.enable_extensions(requiredExtensions);

#ifdef DEBUG
		instanceBuilder.request_validation_layers();
		instanceBuilder.use_default_debug_messenger();
#endif

		auto instanceRet = instanceBuilder.build();
		if (!instanceRet) error("Failed to create Vulkan instance (" + instanceRet.error().message() + ")");

		debug_messenger = instanceRet.value().debug_messenger;
		instance = instanceRet.value().instance;
		create_window();

		std::vector<const char*> requiredDeviceExtensions
		{
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
			VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME
		};

		vk::PhysicalDeviceSynchronization2Features synchronization2
		{
			VK_TRUE
		};

		vk::PhysicalDeviceFeatures features{};
		features.geometryShader = true;

		vkb::PhysicalDeviceSelector physDeviceSelector{ instanceRet.value() };
		physDeviceSelector.add_required_extensions(requiredDeviceExtensions);
		physDeviceSelector.set_surface(surface);
		physDeviceSelector.set_required_features(features);
		
		auto physRet = physDeviceSelector.select();
		if (!physRet) error("Failed to select physical device (" + physRet.error().message() + ")");

		physicalDevice = physRet.value().physical_device;

		vkb::DeviceBuilder deviceBuilder{ physRet.value() };
		deviceBuilder.add_pNext(&synchronization2);

		auto devRet = deviceBuilder.build();
		if (!devRet) error("Failed to create device (" + devRet.error().message() + ")");

		device = devRet.value().device;

		auto presentQueueRet = devRet.value().get_queue(vkb::QueueType::present);
		presentIdx = devRet.value().get_queue_index(vkb::QueueType::present).value();
		if (!presentQueueRet)
		{
			logger->error("Failed to get presentation queue: " + presentQueueRet.error().message());
			exit(EXIT_FAILURE);
		}

		auto graphicsQueueRet = devRet.value().get_queue(vkb::QueueType::graphics);
		graphicsIdx = devRet.value().get_queue_index(vkb::QueueType::graphics).value();

		if (!graphicsQueueRet)
		{
			logger->error("Failed to get graphics queue: " + graphicsQueueRet.error().message());
			exit(EXIT_FAILURE);
		}

		presentQueue = presentQueueRet.value();
		graphicsQueue = graphicsQueueRet.value();

		vkb::SwapchainBuilder swapBuilder{ devRet.value() };

		int width, height;
		glfwGetFramebufferSize(window, &width, &height);

		swapBuilder.use_default_format_selection();
		swapBuilder.use_default_present_mode_selection();
		swapBuilder.use_default_image_usage_flags();
		swapBuilder.set_desired_extent(width, height);
		swapBuilder.set_image_array_layer_count(1);
		auto swapRet = swapBuilder.build();

		if (!swapRet) error("Failed to create swapchain (" + swapRet.error().message() + ")");
		swapchain = swapRet.value().swapchain;

		std::vector<VkImage> _swapImages = swapRet.value().get_images().value();
		for (const auto image : _swapImages) swapImages.push_back(image);

		std::vector<VkImageView> _swapImageViews = swapRet.value().get_image_views().value();
		for (const auto imageView : _swapImageViews) swapImageViews.push_back(imageView);

		swapExtent = swapRet.value().extent;
		swapFormat = static_cast<vk::Format>(swapRet.value().image_format);
	}

	vk::ShaderModule create_shader_module(std::vector<char>& code)
	{
		
		vk::ShaderModuleCreateInfo moduleInfo({}, code.size(), reinterpret_cast<const uint32_t*>(code.data()));
		return device.createShaderModule(moduleInfo);
	}

	void create_render_pass()
	{
		vk::AttachmentDescription colorAttachment(
			{}, swapFormat, vk::SampleCountFlagBits::e1,
			vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
			vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
			vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR
		);

		vk::AttachmentReference colorAttachmentRef(0, vk::ImageLayout::eAttachmentOptimal);

		vk::SubpassDescription subpass(
			{}, vk::PipelineBindPoint::eGraphics,
			nullptr, colorAttachmentRef, 
			nullptr, nullptr, 
			nullptr
		);

		vk::SubpassDependency dependency(
			VK_SUBPASS_EXTERNAL, 0,
			vk::PipelineStageFlagBits::eColorAttachmentOutput,
			vk::PipelineStageFlagBits::eColorAttachmentOutput,
			{}, {}, {}
		);

		vk::RenderPassCreateInfo renderPassInfo({}, colorAttachment, subpass, dependency);

		renderPass = device.createRenderPass(renderPassInfo);
	}

	void create_graphics_pipeline()
	{
		auto vertShader = read_file("src/shader/vert.spv");
		auto fragShader = read_file("src/shader/frag.spv");

		vk::ShaderModule vertModule = create_shader_module(vertShader);
		vk::ShaderModule fragModule = create_shader_module(fragShader);

		vk::PipelineShaderStageCreateInfo vertStageInfo({}, vk::ShaderStageFlagBits::eVertex, vertModule, "main");
		vk::PipelineShaderStageCreateInfo fragStageInfo({}, vk::ShaderStageFlagBits::eFragment, fragModule, "main");

		std::vector<vk::PipelineShaderStageCreateInfo> shaderStages { vertStageInfo, fragStageInfo };

		vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};

		vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo({}, vk::PrimitiveTopology::eTriangleStrip, vk::False);

		vk::Viewport viewport(0.0f, 0.0f, (float)swapExtent.width, (float)swapExtent.height, 0.0f, 1.0f);

		vk::Rect2D scissor({}, swapExtent);

		std::vector<vk::DynamicState> dynamicStates
		{
			vk::DynamicState::eViewport,
			vk::DynamicState::eScissor
		};

		vk::PipelineDynamicStateCreateInfo dynamicStateInfo({}, dynamicStates);

		vk::PipelineViewportStateCreateInfo viewportStateInfo({}, viewport, scissor);

		vk::PipelineRasterizationStateCreateInfo rasterizerInfo({}, vk::False, vk::False, vk::PolygonMode::eFill, vk::CullModeFlagBits::eBack, vk::FrontFace::eClockwise, vk::False);
		rasterizerInfo.lineWidth = 1.0f;

		vk::PipelineMultisampleStateCreateInfo multisamplingInfo{};

		vk::PipelineColorBlendAttachmentState colorBlendAttachmentInfo{};
		colorBlendAttachmentInfo.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
		colorBlendAttachmentInfo.blendEnable = vk::False;

		vk::PipelineColorBlendStateCreateInfo colorBlendInfo({}, vk::False, vk::LogicOp::eNoOp, colorBlendAttachmentInfo, {});

		vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};

		pipelineLayout = device.createPipelineLayout(pipelineLayoutInfo);

		vk::GraphicsPipelineCreateInfo pipelineInfo({}, shaderStages, &vertexInputInfo, &inputAssemblyInfo, {}, &viewportStateInfo, &rasterizerInfo, &multisamplingInfo, {}, &colorBlendInfo, &dynamicStateInfo, pipelineLayout, renderPass);

		vk::ResultValue<vk::Pipeline> result = device.createGraphicsPipeline(VK_NULL_HANDLE, pipelineInfo);
		if (result.result != vk::Result::eSuccess) error("Failed to create graphics pipeline!");

		graphicsPipeline = result.value;

		device.destroyShaderModule(vertModule);
		device.destroyShaderModule(fragModule);
	}

	void create_framebuffers()
	{
		swapFramebuffers.resize(swapImageViews.size());

		for (size_t i = 0; i < swapFramebuffers.size(); i++)
		{
			std::vector<vk::ImageView> attachments
			{
				swapImageViews[i]
			};

			vk::FramebufferCreateInfo framebufferInfo({}, renderPass, attachments, swapExtent.width, swapExtent.height, 1);

			swapFramebuffers[i] = device.createFramebuffer(framebufferInfo);
		}
	}

	void create_command_objects()
	{
		vk::CommandPoolCreateInfo poolInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, graphicsIdx);
		commandPool = device.createCommandPool(poolInfo);

		commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

		vk::CommandBufferAllocateInfo bufferInfo(commandPool, vk::CommandBufferLevel::ePrimary, commandBuffers.size());
		commandBuffers = device.allocateCommandBuffers(bufferInfo);
	}

	void record_command_buffer(vk::CommandBuffer commandBuffer, uint32_t imageIdx)
	{
		vk::CommandBufferBeginInfo beginInfo{};
		commandBuffer.begin(beginInfo);

		vk::ClearValue clearColor({ 0.0f, 0.0f, 0.0f, 1.0f });

		vk::Rect2D renderArea({ 0,0 }, swapExtent);
		vk::RenderPassBeginInfo renderPassInfo(renderPass, swapFramebuffers[imageIdx], renderArea, clearColor);

		commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);

		vk::Viewport viewport(0.0f, 0.0f, static_cast<float>(swapExtent.width), static_cast<float>(swapExtent.height), 0.0f, 1.0f);
		commandBuffer.setViewport(0, viewport);
		commandBuffer.setScissor(0, renderArea);

		commandBuffer.draw(3, 1, 0, 0);
		commandBuffer.endRenderPass();
		commandBuffer.end();
	}

	void create_sync_objects()
	{
		vk::SemaphoreCreateInfo semaphoreInfo{};
		vk::FenceCreateInfo fenceInfo(vk::FenceCreateFlagBits::eSignaled);

		imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			imageAvailableSemaphores[i] = device.createSemaphore(semaphoreInfo);
			renderFinishedSemaphores[i] = device.createSemaphore(semaphoreInfo);
			inFlightFences[i] = device.createFence(fenceInfo);
		}
	}
	
	void draw_frame()
	{
		auto res1 = device.waitForFences(inFlightFences[currentFrame], vk::True, UINT64_MAX);
		device.resetFences(inFlightFences[currentFrame]);

		if (res1 != vk::Result::eSuccess) error("Fence operation failed!");

		auto res2 = device.acquireNextImageKHR(swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrame], {});

		if (res2.result != vk::Result::eSuccess) error("Failed to acquire next image.");
		uint32_t imageIdx = res2.value;

		commandBuffers[currentFrame].reset();
		record_command_buffer(commandBuffers[currentFrame], imageIdx);

		const std::vector<vk::Semaphore> waitSemaphores(1, imageAvailableSemaphores[currentFrame]);
		const std::vector<vk::Semaphore> signalSemaphores(1,renderFinishedSemaphores[currentFrame]);
		const std::vector<vk::PipelineStageFlags> waitStages(1, vk::PipelineStageFlagBits::eColorAttachmentOutput);
		vk::SubmitInfo submitInfo{};
		submitInfo.setWaitSemaphoreCount(waitSemaphores.size());
		submitInfo.setPWaitSemaphores(waitSemaphores.data());
		submitInfo.setSignalSemaphoreCount(signalSemaphores.size());
		submitInfo.setPSignalSemaphores(signalSemaphores.data());
		submitInfo.setPWaitDstStageMask(waitStages.data());
		submitInfo.setCommandBufferCount(1);
		submitInfo.setPCommandBuffers(&commandBuffers[currentFrame]);
		graphicsQueue.submit(submitInfo, inFlightFences[currentFrame]);

		vk::PresentInfoKHR presentInfo(signalSemaphores, swapchain, imageIdx);
		if (graphicsQueue.presentKHR(presentInfo) != vk::Result::eSuccess) error("Failed to present!");

		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}
};

int main()
{
	logger->set_pattern("[%H:%M:%S %^%l%$]: %v"); // [Hour:Minute:Second Level]: Message

	try
	{
		Triangle app;

		app.init();
		app.run();
		app.destroy();
	}
	catch (const std::exception& e)
	{
		logger->error(e.what());
	}

	return EXIT_SUCCESS;
}