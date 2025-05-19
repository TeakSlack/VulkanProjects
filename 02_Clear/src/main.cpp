#include <exception>
#include <iostream>
#include <vector>
#include <cmath>

#include <vulkan/vulkan.hpp> // Vulkan C++ bindings
#include <spdlog/spdlog.h> // Logging library
#include <spdlog/sinks/stdout_color_sinks.h> // Color output for logging library
#include <GLFW/glfw3.h> // Window and input management library

#include "VkBootstrap.h" // Helper library for Vulkan initialization

const int MAX_FRAMES_IN_FLIGHT = 2; // Max number of frames that can be rendered concurrently

auto logger = spdlog::stdout_color_mt("logger");

// Logs error message and quits application
void error(std::string message)
{
	logger->error(message);
	exit(EXIT_FAILURE);
}

// Converts from HSV to RGB colorspace for color-shift animation
vk::ClearColorValue rbgToHsv(float h, float s, float v)
{
	float r = 0.0f, g = 0.0f, b = 0.0f;

	int i = static_cast<int>(std::floor(h * 6));
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

// Main class representing the application
class Clear
{
public:
	std::string application_name = "Clear";

public:
	// Initialize Vulkan and related resources
	void init()
	{
		create_base_objects(); // Create Vulkan instance, physical and logical device, queues, and window
		create_swapchain();
		create_command_objects(); // Create command pool and buffers
		create_sync_objects(); // Create synchronization primitives
	}

	// Main application loop
	void run()
	{
		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents(); // Process user input and window events
			static float hue;
			hue = hue > 1.0f ? 0.0f : hue + 0.001f; // Cycle hue value
			clearValue = rbgToHsv(hue, 0.5f, 1.0f); // Update clear color
			draw_frame(); // Renders the current frame
		}
	}

	// Cleanup resources
	void destroy()
	{
		device.waitIdle(); // Wait till all device operations are complete
		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) // Destroy synchronization primitives
		{
			device.destroySemaphore(renderFinishedSemaphores[i]);
			device.destroySemaphore(imageAvailableSemaphores[i]);
			device.destroyFence(inFlightFences[i]);
		}

		//Destroy Vulkan related objects
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
	// Core application objects
	vk::ClearColorValue clearValue{ 0.0f, 0.0f, 0.0f, 1.0f }; // Clear color for rendering

	GLFWwindow* window = nullptr; // Handle to the application window

	vk::Instance instance; // Vulkan instance, the communication layer between the client code and Vulkan API
	vk::DebugUtilsMessengerEXT debug_messenger;
	vk::PhysicalDevice physicalDevice;
	vk::Device device;
	vk::SurfaceKHR surface;
	uint32_t presentIdx = 0, graphicsIdx = 0;
	vk::Queue presentQueue, graphicsQueue;
	vk::SwapchainKHR swapchain;
	std::vector<vk::Image> swapImages;
	std::vector<vk::ImageView> swapImageViews;
	vk::Extent2D swapExtent;
	vk::Format swapFormat{};
	vk::CommandPool commandPool;
	std::vector<vk::CommandBuffer> commandBuffers;
	std::vector<vk::Semaphore> imageAvailableSemaphores, renderFinishedSemaphores;
	std::vector<vk::Fence> inFlightFences;
	uint32_t currentFrame = 0;
	bool framebufferResized = false;

private:
	// Create application window
	void create_window()
	{
		if (!glfwInit()) exit(EXIT_FAILURE);

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // Ensure OpenGL context isn't created
		window = glfwCreateWindow(1280, 720, application_name.c_str(), NULL, NULL);
		if (!window) error("Failed to create window!");

		// Helper for GLFW to get 'this' pointer
		glfwSetWindowUserPointer(window, this);
		glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

		// Create Vulkan surface for window
		VkResult err = glfwCreateWindowSurface(instance, window, NULL, reinterpret_cast<VkSurfaceKHR*>(&surface));
		if (err != VK_SUCCESS) error("Failed to create window surface");
	}

	static void framebuffer_size_callback(GLFWwindow* window, int width, int height)
	{
		auto app = reinterpret_cast<Clear*>(glfwGetWindowUserPointer(window));
		app->framebufferResized = true;
	}

	// Create Vulkan instance, physical and logical device, queues, swapchain, and window
	void create_base_objects()
	{
		// Get instance extensions
		uint32_t glfwCount = 0;
		const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwCount);
		std::vector<const char*> requiredExtensions(glfwExtensions, glfwExtensions + glfwCount);

		// Give Vulkan instance it's neccessary build info
		vkb::InstanceBuilder instanceBuilder;
		instanceBuilder.set_app_name(application_name.c_str());
		instanceBuilder.set_app_version(VK_MAKE_VERSION(1, 0, 0));
		instanceBuilder.set_engine_name("No Engine");
		instanceBuilder.set_engine_version(VK_MAKE_VERSION(1, 0, 0));
		instanceBuilder.enable_extensions(requiredExtensions);

		// If debug is enabled, use debug messenger
#ifdef DEBUG
		instanceBuilder.request_validation_layers();
		instanceBuilder.use_default_debug_messenger();
#endif

		auto instanceRet = instanceBuilder.build();
		if (!instanceRet) error("Failed to create Vulkan instance (" + instanceRet.error().message() + ")");

		debug_messenger = instanceRet.value().debug_messenger;
		instance = instanceRet.value().instance;

		create_window(); // Create the application window

		// Required extensions and features for the device
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

		// Select the physical device (GPU) based upon defined criteria
		vkb::PhysicalDeviceSelector physDeviceSelector{ instanceRet.value() };
		physDeviceSelector.add_required_extensions(requiredDeviceExtensions);
		physDeviceSelector.set_surface(surface);
		physDeviceSelector.set_required_features(features);

		auto physRet = physDeviceSelector.select();
		if (!physRet) error("Failed to select physical device (" + physRet.error().message() + ")");

		physicalDevice = physRet.value().physical_device;

		// Create the logical device from the GPU
		vkb::DeviceBuilder deviceBuilder{ physRet.value() };
		deviceBuilder.add_pNext(&synchronization2); // Enable synchronization2 extension

		auto devRet = deviceBuilder.build();
		if (!devRet) error("Failed to create device (" + devRet.error().message() + ")");

		device = devRet.value().device;

		// Get present and graphics queues and indicies
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
	}

	// Create swapchain, images, and image view
	void create_swapchain()
	{
		// Build the swapchain for presentation
		vkb::SwapchainBuilder swapBuilder{ physicalDevice, device, surface, graphicsIdx, presentIdx };

		int width, height;
		glfwGetFramebufferSize(window, &width, &height);

		swapBuilder.use_default_format_selection()
			.use_default_present_mode_selection()
			.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
			.set_desired_extent(width, height)
			.set_image_array_layer_count(1);

		auto swapRet = swapBuilder.build();
		if (!swapRet) error("Failed to create swapchain (" + swapRet.error().message() + ")");

		swapchain = swapRet.value().swapchain;

		// Get swapchain images and image views
		swapImages.clear();
		for (const auto image : swapRet.value().get_images().value()) swapImages.push_back(image);
		swapImageViews.clear();
		for (const auto imageView : swapRet.value().get_image_views().value()) swapImageViews.push_back(imageView);

		// Get swapchain extent and format
		swapExtent = swapRet.value().extent;
		swapFormat = static_cast<vk::Format>(swapRet.value().image_format);
	}

	// Create command pool and buffers
	void create_command_objects()
	{
		vk::CommandPoolCreateInfo poolInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, graphicsIdx); // Create command pool for graphics queue
		commandPool = device.createCommandPool(poolInfo);

		// Create command buffer for every frame in flight
		commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

		vk::CommandBufferAllocateInfo bufferInfo(commandPool, vk::CommandBufferLevel::ePrimary, static_cast<uint32_t>(commandBuffers.size()));
		commandBuffers = device.allocateCommandBuffers(bufferInfo);
	}

	// Record draw calls and info to command buffer
	void record_command_buffer(vk::CommandBuffer& commandBuffer, uint32_t imageIdx)
	{
		// Begin recording commands into the command buffer
		vk::CommandBufferBeginInfo beginInfo{};

		// Allows the command buffer to be resubmitted while it is still being executed
		beginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;

		// Define the image subresource range that will be affected by the commands
		vk::ImageSubresourceRange subresourceRange(
			vk::ImageAspectFlagBits::eColor,	// Specify that the range applies to the color aspect of the image
			0,									// Base mipmap level
			1,									// Number of mipmap levels
			0,									// Base array layer
			1									// Number of array layers
		);

		// Define the barrier for transitioning the image layout from undefined to optimal for transfer
		vk::ImageMemoryBarrier presentToClearBarrier(
			vk::AccessFlagBits::eMemoryRead,		// Current access mask: read access (presenting)
			vk::AccessFlagBits::eMemoryWrite,		// New access mask: write access (transfer operation)
			vk::ImageLayout::eUndefined,			// Current layout: undefined (initial state)
			vk::ImageLayout::eTransferDstOptimal,	// New layout: optimal for transfer as destination
			presentIdx,								// Source queue family index
			presentIdx,								// Destination queue family index
			swapImages[imageIdx],					// Target image
			subresourceRange						// Subresource range for the image
		);

		// Define the barrier for transitioning the image layout from transfer destination to presentable
		vk::ImageMemoryBarrier clearToPresentBarrier(
			vk::AccessFlagBits::eMemoryWrite,		// Current access mask: write access (transfer operation)
			vk::AccessFlagBits::eMemoryRead,		// New access mask: read access (presenting)
			vk::ImageLayout::eTransferDstOptimal,	// Current layout: optimal for transfer as destination
			vk::ImageLayout::ePresentSrcKHR,		// New layout: suitable for presenting
			presentIdx,								// Source queue family index
			presentIdx,								// Destination queue family index
			swapImages[imageIdx],					// Target image
			subresourceRange						// Subresource range for the image
		);

		// Start recording commands
		commandBuffer.begin(beginInfo);

		// Insert a pipeline barrier to transition the image layout to the transfer destination layout
		commandBuffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer,	// Source stage: transfer stage
			vk::PipelineStageFlagBits::eTransfer,	// Destination stage: transfer stage
			{},										// No dependency flags
			{},										// No memory barriers
			{},										// No buffer memory barriers
			presentToClearBarrier					// Image memory barrier
		);

		// Clear the image using the specified clear color
		commandBuffer.clearColorImage(
			swapImages[imageIdx],					// Target image
			vk::ImageLayout::eTransferDstOptimal,	// Current layout of the image
			clearValue,								// Clear color (HSV to RGB conversion result)
			subresourceRange						// Subresource range to clear
		);

		// Insert a pipeline barrier to transition the image layout to the presentable layout
		commandBuffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer,		// Source stage: transfer stage
			vk::PipelineStageFlagBits::eBottomOfPipe,	// Destination stage: bottom of the pipeline
			{},											// No dependency flags
			{},											// No memory barriers
			{},											// No buffer memory barriers
			clearToPresentBarrier						// Image memory barrier
		);

		// Finish recording commands into the command buffer
		commandBuffer.end();
	}

	// Create synchronization primitives
	void create_sync_objects()
	{
		// Structure defining semaphore creation info
		vk::SemaphoreCreateInfo semaphoreInfo{};

		// Structure defining fence creation info
		// Fences are created in the signaled state to allow the first frame to render without waiting
		vk::FenceCreateInfo fenceInfo(vk::FenceCreateFlagBits::eSignaled);

		// Resize the semaphore and fence vectors to accommodate the maximum frames in flight
		imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

		// Loop to create synchronization objects for each frame in flight
		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			// Create a semaphore to signal when an image is available from the swapchain
			imageAvailableSemaphores[i] = device.createSemaphore(semaphoreInfo);

			// Create a semaphore to signal when rendering is finished
			renderFinishedSemaphores[i] = device.createSemaphore(semaphoreInfo);

			// Create a fence to synchronize the CPU with the GPU for each frame
			inFlightFences[i] = device.createFence(fenceInfo);
		}
	}

	// Renders a single frame by synchronizing, recording, and presenting
	void draw_frame()
	{
		// Wait for the current frame's fence to ensure the GPU has finished processing it
		auto res1 = device.waitForFences(inFlightFences[currentFrame], vk::True, UINT64_MAX);

		if (res1 != vk::Result::eSuccess) error("Fence operation failed!");

		// Acquire the index of the next available swapchain image
		auto res2 = device.acquireNextImageKHR(swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrame], {});

		// Recreate swapchain if out of date, handle errors during image acquisition
		if (res2.result == vk::Result::eErrorOutOfDateKHR)
		{
			recreate_swapchain();
			return;
		}
		else if (res2.result != vk::Result::eSuccess && res2.result != vk::Result::eSuboptimalKHR) error("Failed to acquire next swapchain image.");

		// Reset the fence for the current frame to prepare for its next use only if we are submitting work (deadlock prevention)
		device.resetFences(inFlightFences[currentFrame]);

		// Extract the index of the acquired image
		uint32_t imageIdx = res2.value;

		// Reset the command buffer for the current frame, preparing it for new commands
		commandBuffers[currentFrame].reset();

		// Record commands into the command buffer for rendering the current frame
		record_command_buffer(commandBuffers[currentFrame], imageIdx);

		// Configure synchronization structures for the command submission
		const std::vector<vk::Semaphore> waitSemaphores(1, imageAvailableSemaphores[currentFrame]); // Wait for the image to be available
		const std::vector<vk::Semaphore> signalSemaphores(1, renderFinishedSemaphores[currentFrame]); // Signal when rendering is finished
		const std::vector<vk::PipelineStageFlags> waitStages(1, vk::PipelineStageFlagBits::eColorAttachmentOutput); // Pipeline stage to wait on

		// Populate the submit info structure with synchronization and command buffer details
		vk::SubmitInfo submitInfo{};
		submitInfo.setWaitSemaphoreCount(static_cast<uint32_t>(waitSemaphores.size()));      // Semaphores to wait on before executing
		submitInfo.setPWaitSemaphores(waitSemaphores.data());
		submitInfo.setSignalSemaphoreCount(static_cast<uint32_t>(signalSemaphores.size())); // Semaphores to signal when execution finishes
		submitInfo.setPSignalSemaphores(signalSemaphores.data());
		submitInfo.setPWaitDstStageMask(waitStages.data());          // Specify the pipeline stages to wait at
		submitInfo.setCommandBufferCount(1);                         // Submit only one command buffer
		submitInfo.setPCommandBuffers(&commandBuffers[currentFrame]);

		// Submit the recorded command buffer to the graphics queue and associate it with the current frame's fence
		graphicsQueue.submit(submitInfo, inFlightFences[currentFrame]);

		// Configure presentation info with the signal semaphore and swapchain details
		vk::PresentInfoKHR presentInfo(signalSemaphores, swapchain, imageIdx);

		// Present the rendered image to the screen
		auto res3 = graphicsQueue.presentKHR(presentInfo);

		// Recreate swapchain if needed
		if (res3 == vk::Result::eErrorOutOfDateKHR || res3 == vk::Result::eSuboptimalKHR || framebufferResized)
		{
			framebufferResized = false;
			recreate_swapchain();
		}
		else if (res3 != vk::Result::eSuccess) error("Failed to present!");

		// Move to the next frame by updating the current frame index
		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	// Cleanup swapchain objects for application exit or swapchain recreation
	void destroy_swapchain()
	{
		// Destroy image views
		for (const auto& imageView : swapImageViews) device.destroyImageView(imageView);

		device.destroySwapchainKHR(swapchain);
	}

	// Swapchain recreation for window resizing
	void recreate_swapchain()
	{
		// Pause application while minimized
		int width = 0, height = 0;
		glfwGetFramebufferSize(window, &width, &height);
		while (width == 0 || height == 0)
		{
			glfwGetFramebufferSize(window, &width, &height);
			glfwWaitEvents();
		}

		device.waitIdle();

		destroy_swapchain();

		create_swapchain();
	}
};

int main()
{
	logger->set_pattern("[%H:%M:%S %^%l%$]: %v"); // [Hour:Minute:Second Level]: Message

	try
	{
		Clear app;

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