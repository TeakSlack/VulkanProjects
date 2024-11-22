#include <exception>
#include <iostream>
#include <vector>
#include <cmath>

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

vk::ClearColorValue rbgToHsv(float h, float s, float v)
{
	float r, g, b;

	int i = std::floor(h * 6);
	float f = h * 6 - i;
	float p = v * (1 - s);
	float q = v * (1 - f * s);
	float t = v * (1 - (1 - f) * s);

	switch (i % 6)
	{
	case 0: r = v, g = t, b = p; break;
	case 1: r = q, g = v, b = p; break;
	case 2: r = p, g = v, b = t; break;
	case 3: r = p, g = q, b = v; break;
	case 4: r = t, g = p, b = v; break;
	case 5: r = v, g = p, b = q; break;
	default: break;
	}

	return { r, g, b, 1.0f };
}

class Triangle
{
public:
	std::string application_name = "Clear";

public:
	void init()
	{
		create_base_objects();
		create_command_objects();
		create_sync_objects();
	}

	void run()
	{
		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();
			hue = hue > 1.0f ? 0.0f : hue + 0.001f;
			clearValue = rbgToHsv(hue, 0.5f, 1.0f);
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
	float hue = 0.0f;
	vk::ClearColorValue clearValue{ 0.0f, 0.0f, 0.0f, 1.0f };

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
	vk::CommandPool commandPool;
	std::vector<vk::CommandBuffer> commandBuffers;
	std::vector<vk::Semaphore> imageAvailableSemaphores, renderFinishedSemaphores;
	std::vector<vk::Fence> inFlightFences;
	uint32_t currentFrame = 0;

private:
	void create_window()
	{
		if (!glfwInit()) exit(EXIT_FAILURE);

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		window = glfwCreateWindow(1280, 720, application_name.c_str(), NULL, NULL);
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
		swapBuilder.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT);
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

	void create_command_objects()
	{
		vk::CommandPoolCreateInfo poolInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, graphicsIdx);
		commandPool = device.createCommandPool(poolInfo);

		commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

		vk::CommandBufferAllocateInfo bufferInfo(commandPool, vk::CommandBufferLevel::ePrimary, commandBuffers.size());
		commandBuffers = device.allocateCommandBuffers(bufferInfo);
	}

	void record_command_buffer(vk::CommandBuffer& commandBuffer, uint32_t imageIdx)
	{
		vk::CommandBufferBeginInfo beginInfo{};
		beginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;

		vk::ImageSubresourceRange subresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);

		vk::ImageMemoryBarrier presentToClearBarrier(vk::AccessFlagBits::eMemoryRead, vk::AccessFlagBits::eMemoryWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, presentIdx, graphicsIdx, swapImages[imageIdx], subresourceRange);

		vk::ImageMemoryBarrier clearToPresentBarrier(vk::AccessFlagBits::eMemoryWrite, vk::AccessFlagBits::eMemoryRead, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::ePresentSrcKHR, graphicsIdx, presentIdx, swapImages[imageIdx], subresourceRange);

		commandBuffer.begin(beginInfo);

		commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, presentToClearBarrier);
		commandBuffer.clearColorImage(swapImages[imageIdx], vk::ImageLayout::eTransferDstOptimal, clearValue, subresourceRange);
		commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eBottomOfPipe, {}, {}, {}, clearToPresentBarrier);

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
		auto res2 = device.acquireNextImageKHR(swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrame], {});

		if (res1 != vk::Result::eSuccess || res2.result != vk::Result::eSuccess) error("Failed to acquire next image.");
		uint32_t imageIdx = res2.value;

		commandBuffers[currentFrame].reset();
		record_command_buffer(commandBuffers[currentFrame], imageIdx);

		const std::vector<vk::Semaphore> waitSemaphores(1, imageAvailableSemaphores[currentFrame]);
		const std::vector<vk::Semaphore> signalSemaphores(1, renderFinishedSemaphores[currentFrame]);
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