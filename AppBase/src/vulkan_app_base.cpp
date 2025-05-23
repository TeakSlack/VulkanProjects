#include "vulkan_app_base.h"

VulkanAppBase::VulkanAppBase()
	: config{}
{
	// Default configuration values
	config.application_name = "Vulkan Application";
	config.application_version = VK_MAKE_VERSION(1, 0, 0);
	config.engine_name = "Vulkan Application Base";
	config.engine_version = VK_MAKE_VERSION(1, 0, 0);
	config.enable_validation_layers = true;
	config.window_width = 1280;
	config.window_height = 720;
}

// Constructor: Initializes the Vulkan application base with the given configuration.
VulkanAppBase::VulkanAppBase(const AppConfig& config)
	: config(config)
{
	init();
}

// Destructor: Cleans up Vulkan resources.
VulkanAppBase::~VulkanAppBase()
{
	destroy();
}

// Initializes the Vulkan application (instance, device, window, swapchain, etc.)
void VulkanAppBase::init()
{
	// Set logger format for consistent log output
	spdlog::set_pattern("[%H:%M:%S %^%l%$]: %v");

	// Create Vulkan instance
	create_instance();

	// Create window and surface
	create_window_and_surface();

	// Create physical & logical devices
	create_device();

	// Create device and swapchain
	create_swapchain();

	// Create command pools
	create_command_pools();

	// Create framebuffers for swapchain images
	create_framebuffers();

	// Create synchronization objects (semaphores and fences)
	create_sync_objects();
}

// Destroys and cleans up all Vulkan and window resources.
void VulkanAppBase::destroy()
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

	// Destroy framebuffers
	for (const auto& framebuffer : m_Framebuffers)
		m_Device.destroyFramebuffer(framebuffer);

	// Destroy surface
	if (m_Surface)
		m_Instance.destroySurfaceKHR(m_Surface);

	// Destroy debug messenger
	vk::DispatchLoaderDynamic dldi(m_Instance, vkGetInstanceProcAddr);
	m_Instance.destroyDebugUtilsMessengerEXT(m_DebugMessenger, nullptr, dldi);

	// Destroy logical device and Vulkan instance
	m_Device.destroy();
	m_Instance.destroy();

	// Destroy window and terminate GLFW
	glfwDestroyWindow(m_Window);
	glfwTerminate();
}

// Creates the Vulkan instance using vk-bootstrap.
void VulkanAppBase::create_instance()
{
	// Get required extensions from GLFW
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	config.instance_extensions.insert(
		config.instance_extensions.end(),
		glfwExtensions,
		glfwExtensions + glfwExtensionCount
	);

	// Build the Vulkan instance with vk-bootstrap
	vkb::InstanceBuilder instanceBuilder;
	instanceBuilder.set_app_name(config.application_name.c_str())
		.set_app_version(config.application_version)
		.set_engine_name(config.engine_name.c_str())
		.set_engine_version(config.engine_version)
		.enable_validation_layers(config.enable_validation_layers)
		.request_validation_layers(config.enable_validation_layers)
		.use_default_debug_messenger()
		.enable_extensions(config.instance_extensions);

	// Create the Vulkan instance
	auto instanceRet = instanceBuilder.build();
	if (!instanceRet)
		error("Failed to create Vulkan instance (" + instanceRet.error().message() + ")");

	m_Instance = instanceRet.value().instance;
	m_VkbInstance = instanceRet.value();
	m_DebugMessenger = instanceRet.value().debug_messenger;
}

// Selects a physical device and creates a logical device and queues.
void VulkanAppBase::create_device()
{
	// Enable synchronization2 (required for Vulkan 1.3+)
	vk::PhysicalDeviceSynchronization2FeaturesKHR synchronization2{ vk::True };

	// If no device features are specified, enable geometry shader by default
	vk::PhysicalDeviceFeatures zeroFeatures{};
	if (memcmp(&config.device_features, &zeroFeatures, sizeof(VkPhysicalDeviceFeatures)) == 0)
	{
		config.device_features.geometryShader = vk::True;
	}

	assert(m_Surface && "vk::SurfaceKHR has not been initialized!");

	// Select a physical device using vk-bootstrap
	vkb::PhysicalDeviceSelector physicalDeviceSelector{ m_VkbInstance };
	physicalDeviceSelector.add_required_extensions(config.device_extensions)
		.set_surface(m_Surface)
		.set_required_features(config.device_features);

	auto physRet = physicalDeviceSelector.select();
	if (!physRet)
		error("Failed to select physical device: " + physRet.error().message());

	m_PhysicalDevice = physRet.value().physical_device;

	// Create logical device
	vkb::DeviceBuilder deviceBuilder{ physRet.value() };
	deviceBuilder.add_pNext(&synchronization2);

	auto deviceRet = deviceBuilder.build();
	if (!deviceRet)
		error("Failed to create logical device: " + deviceRet.error().message());

	m_Device = deviceRet.value().device;

	// Retrieve queue handles and indices
	auto presentQueueRet = deviceRet.value().get_queue(vkb::QueueType::present);
	m_PresentIdx = deviceRet.value().get_queue_index(vkb::QueueType::present).value();
	if (!presentQueueRet)
		error("Failed to get presentation queue: " + presentQueueRet.error().message());

	auto graphicsQueueRet = deviceRet.value().get_queue(vkb::QueueType::graphics);
	m_GraphicsIdx = deviceRet.value().get_queue_index(vkb::QueueType::graphics).value();
	if (!graphicsQueueRet)
		error("Failed to get graphics queue: " + graphicsQueueRet.error().message());

	// Attempt to get a dedicated transfer queue, fallback to graphics (implicit transfer functionality) if unavailable
	auto transferQueueRet = deviceRet.value().get_dedicated_queue(vkb::QueueType::transfer);
	if (transferQueueRet.error() == vkb::QueueError::transfer_unavailable)
	{
		m_TransferIdx = m_GraphicsIdx;
		m_TransferQueue = graphicsQueueRet.value();
	}
	else
	{
		m_TransferIdx = deviceRet.value().get_dedicated_queue_index(vkb::QueueType::transfer).value();
		m_TransferQueue = transferQueueRet.value();
	}

	m_PresentQueue = presentQueueRet.value();
	m_GraphicsQueue = graphicsQueueRet.value();
}

// Creates the GLFW window and Vulkan surface.
void VulkanAppBase::create_window_and_surface()
{
	if (!glfwInit())
		error("Failed to initialize GLFW!");

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	m_Window = glfwCreateWindow(
		config.window_width,
		config.window_height,
		config.application_name.c_str(),
		NULL,
		NULL
	);

	if (!m_Window)
		error("Failed to create window!");

	glfwSetWindowUserPointer(m_Window, this);
	glfwSetFramebufferSizeCallback(m_Window, framebuffer_size_callback);

	VkResult err = glfwCreateWindowSurface(m_Instance, m_Window, NULL, reinterpret_cast<VkSurfaceKHR*>(&m_Surface));
	if (err != VK_SUCCESS)
		error("Failed to create window surface");
}

// Creates the swapchain and associated image views.
void VulkanAppBase::create_swapchain()
{
	vkb::SwapchainBuilder swapBuilder{ m_PhysicalDevice, m_Device, m_Surface, m_GraphicsIdx, m_PresentIdx };

	int width, height;
	glfwGetFramebufferSize(m_Window, &width, &height);

	swapBuilder.use_default_format_selection()
		.use_default_present_mode_selection()
		.use_default_image_usage_flags()
		.set_desired_extent(width, height)
		.set_image_array_layer_count(1);

	auto swapRet = swapBuilder.build();
	if (!swapRet)
		error("Failed to create swapchain (" + swapRet.error().message() + ")");

	m_Swapchain = swapRet.value().swapchain;

	// Store swapchain images and views
	for (const auto image : swapRet.value().get_images().value())
		m_Images.push_back(image);

	for (const auto imageView : swapRet.value().get_image_views().value())
		m_ImageViews.push_back(imageView);
}

// Overload: Creates the swapchain with a custom configuration.
void VulkanAppBase::create_swapchain(const SwapchainConfig& swapConfig)
{
	vkb::SwapchainBuilder swapBuilder{ m_PhysicalDevice, m_Device, m_Surface, m_GraphicsIdx, m_PresentIdx };

	int width, height;
	glfwGetFramebufferSize(m_Window, &width, &height);

	swapBuilder.set_desired_format(swapConfig.format)
		.set_desired_present_mode(static_cast<VkPresentModeKHR>(swapConfig.presentMode))
		.set_image_usage_flags(static_cast<VkImageUsageFlags>(swapConfig.usage))
		.set_desired_extent(width, height)
		.set_image_array_layer_count(1);

	auto swapRet = swapBuilder.build();
	if (!swapRet)
		error("Failed to create swapchain (" + swapRet.error().message() + ")");

	m_Swapchain = swapRet.value().swapchain;

	// Store swapchain images and views
	for (const auto image : swapRet.value().get_images().value())
		m_Images.push_back(image);

	for (const auto imageView : swapRet.value().get_image_views().value())
		m_ImageViews.push_back(imageView);

	m_SwapExtent = swapRet.value().extent;
	m_SwapFormat = static_cast<vk::Format>(swapRet.value().image_format);
}

// Recreates the swapchain and framebuffers, e.g., after a window resize.
void VulkanAppBase::recreate_swapchain()
{
	// Pause application while minimized
	int width = 0, height = 0;
	glfwGetFramebufferSize(m_Window, &width, &height);
	while (width == 0 || height == 0)
	{
		glfwGetFramebufferSize(m_Window, &width, &height);
		glfwWaitEvents();
	}

	m_Device.waitIdle();

	// Destroy previous swapchain before recreation
	destroy_swapchain();

	create_swapchain();
	create_framebuffers();
}

// Destroys the swapchain and associated resources.
void VulkanAppBase::destroy_swapchain()
{
	// Destroy all framebuffers
	for (const auto& framebuffer : m_Framebuffers)
		m_Device.destroyFramebuffer(framebuffer);

	// Destroy image views
	for (const auto& imageView : m_ImageViews)
		m_Device.destroyImageView(imageView);

	// Clear images and views
	m_ImageViews.clear();
	m_Images.clear();

	m_Device.destroySwapchainKHR(m_Swapchain);
}

// Creates framebuffers for each swapchain image view.
void VulkanAppBase::create_framebuffers()
{
	m_Framebuffers.resize(m_ImageViews.size());

	for (size_t i = 0; i < m_Framebuffers.size(); i++)
	{
		std::vector<vk::ImageView> attachments{ m_ImageViews[i] };
		vk::FramebufferCreateInfo framebufferInfo(
			{}, m_RenderPass, attachments, m_SwapExtent.width, m_SwapExtent.height, 1
		);
		m_Framebuffers[i] = m_Device.createFramebuffer(framebufferInfo);
	}
}

// Creates command pools for graphics and transfer operations.
void VulkanAppBase::create_command_pools()
{
	vk::CommandPoolCreateInfo graphicsPoolInfo(
		vk::CommandPoolCreateFlagBits::eResetCommandBuffer, m_GraphicsIdx
	);
	m_GraphicsCommandPool = m_Device.createCommandPool(graphicsPoolInfo);

	vk::CommandPoolCreateInfo transferPoolInfo(
		vk::CommandPoolCreateFlagBits::eTransient, m_TransferIdx
	);
	m_TransferCommandPool = m_Device.createCommandPool(transferPoolInfo);
}

// Creates synchronization primitives (semaphores and fences) for each frame in flight.
// These are used to coordinate rendering and presentation between the CPU and GPU.
void VulkanAppBase::create_sync_objects()
{
	// Create info structures for semaphores and fences.
	vk::SemaphoreCreateInfo semaphoreInfo{};
	// Fences are created in the signaled state so the first frame can be rendered immediately.
	vk::FenceCreateInfo fenceInfo(vk::FenceCreateFlagBits::eSignaled);

	// Resize the vectors to hold one set of sync objects per frame in flight.
	m_ImageAvailableSemaphores.resize(m_FramesInFlight);
	m_RenderFinishedSemaphores.resize(m_FramesInFlight);
	m_InFlightFences.resize(m_FramesInFlight);

	// Create a semaphore and fence for each frame in flight.
	for (int i = 0; i < m_FramesInFlight; i++)
	{
		// Semaphore signaled when an image is available for rendering.
		m_ImageAvailableSemaphores[i] = m_Device.createSemaphore(semaphoreInfo);
		// Semaphore signaled when rendering is finished and presentation can occur.
		m_RenderFinishedSemaphores[i] = m_Device.createSemaphore(semaphoreInfo);
		// Fence signaled when the GPU has finished processing commands for this frame.
		m_InFlightFences[i] = m_Device.createFence(fenceInfo);
	}
}

// Reads a binary file (e.g., SPIR-V shader) into a byte buffer.
std::vector<char> VulkanAppBase::read_file(const std::string& fileName)
{
	std::ifstream file(fileName, std::ifstream::ate | std::ifstream::binary);

	if (!file.is_open())
	{
		spdlog::error("Failed to open file " + fileName);
		exit(EXIT_FAILURE);
	}

	size_t fileSize = file.tellg();
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);
	file.close();

	return buffer;
}

// Creates a Vulkan shader module from SPIR-V bytecode.
vk::ShaderModule VulkanAppBase::create_shader_module(const std::vector<char>& code)
{
	vk::ShaderModuleCreateInfo moduleInfo(
		{}, code.size(), reinterpret_cast<const uint32_t*>(code.data())
	);
	return m_Device.createShaderModule(moduleInfo);
}

// Finds a suitable memory type for a buffer or image.
uint32_t VulkanAppBase::find_memory_type(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
{
	// Query info about available types of memory
	vk::PhysicalDeviceMemoryProperties memProperties = m_PhysicalDevice.getMemoryProperties();

	// Iterate memory types and properties until a suitable match is found
	for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
	{
		bool isTypeSuitable = typeFilter & (1 << i);
		bool hasRequiredProperties = (memProperties.memoryTypes[i].propertyFlags & properties) == properties;

		if (isTypeSuitable && hasRequiredProperties)
			return i;
	}

	error("Unable to find suitable memory type!");
	return 0;
}

// Creates a Vulkan buffer and allocates memory for it.
std::pair<vk::Buffer, vk::DeviceMemory> VulkanAppBase::create_buffer(
	vk::DeviceSize size,
	vk::BufferUsageFlags flags,
	vk::MemoryPropertyFlags properties,
	vk::SharingMode sharingMode,
	std::vector<uint32_t> queues
)
{
	vk::BufferCreateInfo bufferInfo(
		{},                 // No creation flags
		size,               // Size of the buffer in bytes
		flags,              // Buffer usage flags
		sharingMode,        // Sharing mode: exclusive or concurrent
		queues              // List of queue family indices
	);

	vk::Buffer buffer = m_Device.createBuffer(bufferInfo);

	// Query buffer memory requirements
	vk::MemoryRequirements memReqs = m_Device.getBufferMemoryRequirements(buffer);

	// Find suitable memory type and allocate
	uint32_t memTypeIndex = find_memory_type(memReqs.memoryTypeBits, properties);
	vk::MemoryAllocateInfo allocInfo(memReqs.size, memTypeIndex);

	vk::DeviceMemory mem = m_Device.allocateMemory(allocInfo);

	// Bind memory to buffer
	m_Device.bindBufferMemory(buffer, mem, 0);

	return { buffer, mem };
}

// Copies data from one buffer to another using a command buffer.
void VulkanAppBase::copy_buffer(vk::Buffer src, vk::Buffer dst, vk::DeviceSize size)
{
	// Allocate a temporary command buffer for the copy operation
	vk::CommandBufferAllocateInfo allocInfo(
		m_TransferCommandPool, vk::CommandBufferLevel::ePrimary, 1
	);

	vk::CommandBuffer commandBuffer = m_Device.allocateCommandBuffers(allocInfo)[0];

	// Begin recording the command buffer
	vk::CommandBufferBeginInfo beginInfo({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
	commandBuffer.begin(beginInfo);

	// Define the region to copy and perform the copy
	vk::BufferCopy copyRegion(0, 0, size);
	commandBuffer.copyBuffer(src, dst, copyRegion);

	commandBuffer.end();

	// Submit the command buffer and wait for completion
	vk::SubmitInfo submitInfo{};
	submitInfo.setCommandBuffers(commandBuffer);
	m_TransferQueue.submit(submitInfo);
	m_TransferQueue.waitIdle();

	// Free the temporary command buffer
	m_Device.freeCommandBuffers(m_TransferCommandPool, commandBuffer);
}