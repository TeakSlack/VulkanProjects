#include <exception>
#include <iostream>
#include <vector>
#include <cmath>

#include <vulkan/vulkan.hpp> // Vulkan C++ bindings
#include <spdlog/spdlog.h> // Logging library
#include <spdlog/sinks/stdout_color_sinks.h> // Color output for logging library
#include <vulkan_app_base.h>

// Create a color logger using spdlog
auto logger = spdlog::stdout_color_mt("logger");

// Logs error message and quits application
void error(std::string message)
{
	logger->error(message);
	exit(EXIT_FAILURE);
}

// Converts from HSV to RGB colorspace for color-shift animation
vk::ClearColorValue hsvToRgb(float h, float s, float v)
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

// Main class representing the clear-color Vulkan application
class Clear : public VulkanAppBase
{
public:
	// Constructor: initializes the base Vulkan application and logs startup
	Clear() : VulkanAppBase()
	{
		logger->info("Clear application initialized");
		init();
	}

	// Main application loop
	void run()
	{
		create_command_buffers();

		while (!glfwWindowShouldClose(m_Window))
		{
			glfwPollEvents(); // Process user input and window events

			// Animate the clear color by cycling the hue value
			static float hue = 0.0f;
			hue = (hue > 1.0f) ? 0.0f : hue + 0.001f;
			m_ClearValue = hsvToRgb(hue, 0.5f, 1.0f);

			draw_frame(); // Render the current frame
		}
	}

protected:
	// Initializes Vulkan and related resources
	void init() override
	{
		// Configure swapchain format, present mode, and usage flags
		SwapchainConfig swapConfig{
			.format = vk::SurfaceFormatKHR(vk::Format::eB8G8R8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear),
			.presentMode = vk::PresentModeKHR::eMailbox,
			.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst
		};

		create_instance();
		create_window_and_surface();
		create_device();
		create_swapchain(swapConfig);
		create_command_pools();
		create_sync_objects();
	}

	// Cleans up all Vulkan and window resources
	void destroy() override
	{
		// Wait until all GPU work is done before cleanup
		m_Device.waitIdle();

		// Destroy all synchronization primitives
		for (auto& fence : m_InFlightFences) m_Device.destroyFence(fence);
		for (auto& semaphore : m_ImageAvailableSemaphores) m_Device.destroySemaphore(semaphore);
		for (auto& semaphore : m_RenderFinishedSemaphores) m_Device.destroySemaphore(semaphore);

		// Destroy swapchain and related resources
		destroy_swapchain();

		// Destroy command pools
		m_Device.destroyCommandPool(m_GraphicsCommandPool);
		m_Device.destroyCommandPool(m_TransferCommandPool);

		// Destroy image views and images
		for (const auto& imageView : m_ImageViews)
			m_Device.destroyImageView(imageView);

		for (const auto& image : m_Images)
			m_Device.destroyImage(image);

		// Destroy surface
		if (m_Surface)
			m_Instance.destroySurfaceKHR(m_Surface);

		// Destroy logical device and Vulkan instance
		m_Device.destroy();
		m_Instance.destroy();

		// Destroy window and terminate GLFW
		glfwDestroyWindow(m_Window);
		glfwTerminate();
	}

private:
	vk::ClearColorValue m_ClearValue{ 0.0f, 0.0f, 0.0f, 1.0f }; // Current clear color
	std::vector<vk::CommandBuffer> m_CommandBuffers; // Command buffers for each frame
	int m_CurrentFrame = 0; // Index of the current frame

	// Allocates command buffers for each frame in flight
	void create_command_buffers()
	{
		m_CommandBuffers.resize(m_FramesInFlight);
		vk::CommandBufferAllocateInfo allocInfo(
			m_GraphicsCommandPool,
			vk::CommandBufferLevel::ePrimary,
			static_cast<uint32_t>(m_CommandBuffers.size())
		);

		// Allocate command buffers from the command pool
		m_CommandBuffers = m_Device.allocateCommandBuffers(allocInfo);
	}

	// Records commands to clear the swapchain image for the given frame
	void record_command_buffer(vk::CommandBuffer& commandBuffer, uint32_t imageIdx)
	{
		// Begin recording commands into the command buffer
		vk::CommandBufferBeginInfo beginInfo{};
		beginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;

		// Define the image subresource range to clear (entire color image)
		vk::ImageSubresourceRange subresourceRange(
			vk::ImageAspectFlagBits::eColor, // Color aspect
			0, 1, // Mip levels
			0, 1  // Array layers
		);

		// Barrier: undefined -> transfer destination
		vk::ImageMemoryBarrier presentToClearBarrier(
			vk::AccessFlagBits::eMemoryRead,
			vk::AccessFlagBits::eMemoryWrite,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eTransferDstOptimal,
			m_PresentIdx,
			m_PresentIdx,
			m_Images[imageIdx],
			subresourceRange
		);

		// Barrier: transfer destination -> presentable
		vk::ImageMemoryBarrier clearToPresentBarrier(
			vk::AccessFlagBits::eMemoryWrite,
			vk::AccessFlagBits::eMemoryRead,
			vk::ImageLayout::eTransferDstOptimal,
			vk::ImageLayout::ePresentSrcKHR,
			m_PresentIdx,
			m_PresentIdx,
			m_Images[imageIdx],
			subresourceRange
		);

		commandBuffer.begin(beginInfo);

		// Transition image to transfer destination layout
		commandBuffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eTransfer,
			{},
			{},
			{},
			presentToClearBarrier
		);

		// Clear the image to the current clear color
		commandBuffer.clearColorImage(
			m_Images[imageIdx],
			vk::ImageLayout::eTransferDstOptimal,
			m_ClearValue,
			subresourceRange
		);

		// Transition image to presentable layout
		commandBuffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eBottomOfPipe,
			{},
			{},
			{},
			clearToPresentBarrier
		);

		commandBuffer.end();
	}

	// Renders a single frame: acquire, record, submit, and present
	void draw_frame()
	{
		// Wait for the current frame's fence to ensure the GPU has finished processing it
		auto res1 = m_Device.waitForFences(m_InFlightFences[m_CurrentFrame], vk::True, UINT64_MAX);
		if (res1 != vk::Result::eSuccess) error("Fence operation failed!");

		// Acquire the index of the next available swapchain image
		auto res2 = m_Device.acquireNextImageKHR(
			m_Swapchain, UINT64_MAX, m_ImageAvailableSemaphores[m_CurrentFrame], {}
		);

		// Recreate swapchain if out of date, handle errors during image acquisition
		if (res2.result == vk::Result::eErrorOutOfDateKHR)
		{
			recreate_swapchain();
			return;
		}
		else if (res2.result != vk::Result::eSuccess && res2.result != vk::Result::eSuboptimalKHR)
		{
			error("Failed to acquire next swapchain image.");
		}

		// Reset the fence for the current frame to prepare for its next use
		m_Device.resetFences(m_InFlightFences[m_CurrentFrame]);

		// Extract the index of the acquired image
		uint32_t imageIdx = res2.value;

		// Reset and record the command buffer for the current frame
		m_CommandBuffers[m_CurrentFrame].reset();
		record_command_buffer(m_CommandBuffers[m_CurrentFrame], imageIdx);

		// Set up synchronization for queue submission
		const std::vector<vk::Semaphore> waitSemaphores{ m_ImageAvailableSemaphores[m_CurrentFrame] };
		const std::vector<vk::Semaphore> signalSemaphores{ m_RenderFinishedSemaphores[m_CurrentFrame] };
		const std::vector<vk::PipelineStageFlags> waitStages{ vk::PipelineStageFlagBits::eColorAttachmentOutput };

		vk::SubmitInfo submitInfo{};
		submitInfo.setWaitSemaphoreCount(static_cast<uint32_t>(waitSemaphores.size()));
		submitInfo.setPWaitSemaphores(waitSemaphores.data());
		submitInfo.setSignalSemaphoreCount(static_cast<uint32_t>(signalSemaphores.size()));
		submitInfo.setPSignalSemaphores(signalSemaphores.data());
		submitInfo.setPWaitDstStageMask(waitStages.data());
		submitInfo.setCommandBufferCount(1);
		submitInfo.setPCommandBuffers(&m_CommandBuffers[m_CurrentFrame]);

		// Submit the command buffer to the graphics queue
		m_GraphicsQueue.submit(submitInfo, m_InFlightFences[m_CurrentFrame]);

		// Present the rendered image to the screen
		vk::PresentInfoKHR presentInfo(signalSemaphores, m_Swapchain, imageIdx);
		auto res3 = m_GraphicsQueue.presentKHR(presentInfo);

		// Recreate swapchain if needed (e.g., window resize)
		if (res3 == vk::Result::eErrorOutOfDateKHR || res3 == vk::Result::eSuboptimalKHR || m_FramebufferResized)
		{
			m_FramebufferResized = false;
			recreate_swapchain();
		}
		else if (res3 != vk::Result::eSuccess)
		{
			error("Failed to present!");
		}

		// Move to the next frame
		m_CurrentFrame = (m_CurrentFrame + 1) % m_FramesInFlight;
	}
};

int main()
{
	logger->set_pattern("[%H:%M:%S %^%l%$]: %v"); // Set log output format

	try
	{
		Clear app;
		app.run();
	}
	catch (const std::exception& e)
	{
		logger->error(e.what());
	}

	return EXIT_SUCCESS;
}