#pragma once

#include <exception>
#include <iostream>
#include <vector>
#include <fstream>
#include <unordered_set>
#include <array>
#include <utility>
#include <string>
#include <cassert>

#include <vulkan/vulkan.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "VkBootstrap.h"

// Configuration structure for the application
struct AppConfig
{
	std::string application_name = "Vulkan Application";
	uint32_t application_version = VK_MAKE_VERSION(1, 0, 0);
	std::string engine_name = "Vulkan Application Base";
	uint32_t engine_version = VK_MAKE_VERSION(1, 0, 0);
	bool enable_validation_layers = true;
	int window_width = 1280;
	int window_height = 720;
	std::vector<const char*> instance_extensions;
	std::vector<const char*> device_extensions = 
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME, 
		VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME
	};
	vk::PhysicalDeviceFeatures device_features;
};

struct SwapchainConfig
{
	vk::SurfaceFormatKHR format;
	vk::PresentModeKHR presentMode;
	vk::ImageUsageFlags usage;
};

// Base class boilerplate for Vulkan applications
class VulkanAppBase
{
public:
	VulkanAppBase();
	VulkanAppBase(const AppConfig& m_Config);
	virtual ~VulkanAppBase();

	virtual void run() = 0;

protected:
	// Application information
	AppConfig m_Config;
	SwapchainConfig m_SwapConfig;

	// GLFW window
	GLFWwindow* m_Window;

	// Frames in flight
	const int m_FramesInFlight = 2;

	// Check if framebuffer is resized
	bool m_FramebufferResized = false;

	// Core Vulkan objects
	vk::Instance m_Instance;
	vk::DebugUtilsMessengerEXT m_DebugMessenger;
	vk::SurfaceKHR m_Surface;
	vk::PhysicalDevice m_PhysicalDevice;
	vk::Device m_Device;
	vk::Queue m_GraphicsQueue, m_PresentQueue, m_TransferQueue;
	uint32_t m_GraphicsIdx, m_PresentIdx, m_TransferIdx;
	vk::SwapchainKHR m_Swapchain;
	vk::Extent2D m_SwapExtent;
	vk::Format m_SwapFormat;
	std::vector<vk::Framebuffer> m_Framebuffers;
	std::vector<vk::Image> m_Images;
	std::vector<vk::ImageView> m_ImageViews;
	vk::RenderPass m_RenderPass;
	vk::CommandPool m_GraphicsCommandPool, m_TransferCommandPool;
	std::vector<vk::Semaphore> m_ImageAvailableSemaphores, m_RenderFinishedSemaphores;
	std::vector<vk::Fence> m_InFlightFences;

protected:
	virtual void init();
	virtual void destroy();

	// Core Vulkan lifecycle functions
	virtual void create_instance();
	virtual void create_device();
	virtual void create_window_and_surface();
	virtual void create_swapchain();
	virtual void create_swapchain(const SwapchainConfig& swapConfig);
	virtual void recreate_swapchain();
	virtual void destroy_swapchain();
	virtual void create_framebuffers();
	virtual void create_command_pools();
	virtual void create_sync_objects();
	
	// Utility
	static std::vector<char> read_file(const std::string& fileName);
	virtual vk::ShaderModule create_shader_module(const std::vector<char>& code);
	virtual uint32_t find_memory_type(uint32_t typeFilter, vk::MemoryPropertyFlags properties);
	std::pair<vk::Buffer, vk::DeviceMemory> create_buffer(vk::DeviceSize size, vk::BufferUsageFlags flags, vk::MemoryPropertyFlags properties, vk::SharingMode sharingMode, std::vector<uint32_t> queues);
	void copy_buffer(vk::Buffer src, vk::Buffer dst, vk::DeviceSize size);

	inline void error(std::string message)
	{
		spdlog::error("An error has occurred: " + message);
		exit(EXIT_FAILURE);
	}

private:
	vkb::Instance m_VkbInstance;

private:
	static void framebuffer_size_callback(GLFWwindow* window, int width, int height)
	{
		auto app = reinterpret_cast<VulkanAppBase*>(glfwGetWindowUserPointer(window));
		app->m_FramebufferResized = true;
	}
};