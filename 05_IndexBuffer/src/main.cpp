#include <exception>
#include <iostream>
#include <vector>
#include <fstream>
#include <unordered_set>
#include <array>
#include <utility>

#include <vulkan/vulkan.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "VkBootstrap.h"

// Create a color logger using spdlog
auto logger = spdlog::stdout_color_mt("logger");

// Basic vertex
struct Vertex
{
	glm::vec2 pos;
	glm::vec3 color;

	// Defines how the data is passed to the vertex shader
	static vk::VertexInputBindingDescription get_binding_description()
	{
		// Defines the rate to load data from memory
		vk::VertexInputBindingDescription bindingDescription(
			0,								// Binding: only one--all per-vertex data is in one array
			sizeof(Vertex),					// Stride: number of bytes from one entry to the next
			vk::VertexInputRate::eVertex	// Input rate: either vertex or instance for instanced rendering
		);

		return bindingDescription;
	}

	// Describes how to retrieve vertex attribute from a chunk of vertex data
	static std::array<vk::VertexInputAttributeDescription, 2> get_attribute_descriptions()
	{
		std::array<vk::VertexInputAttributeDescription, 2> attributeDescriptions{};

		// For pos (vec2)
		attributeDescriptions[0].binding = 0;							// Which binding does this come from?
		attributeDescriptions[0].location = 0;							// References (location = 0) in vtex shader
		attributeDescriptions[0].format = vk::Format::eR32G32Sfloat;	// Type of data for attribute, vec2, specified using same enum as colors
		attributeDescriptions[0].offset = offsetof(Vertex, pos);		// Specifies number of bytes since start of per-vertex data

		// For color (vec3)
		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;
		attributeDescriptions[1].offset = offsetof(Vertex, color);

		return attributeDescriptions;
	}
};

class IndexBuffer
{
public:
	std::string application_name = "Index Buffer";

public:
	// Initialize Vulkan and related resources
	void init()
	{
		// Set logger format
		logger->set_pattern("[%H:%M:%S %^%l%$]: %v");

		create_base_objects();
		create_swapchain();
		create_render_pass();
		create_command_objects();
		create_vertex_buffer();
		create_index_buffer();
		create_graphics_pipeline();
		create_framebuffers();
		create_sync_objects();
	}

	// Run the main render loop
	void run()
	{
		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents(); // Handle window events
			draw_frame(); // Draw one frame
		}
	}

	// Cleanup Vulkan resources in reverse order of initialization
	void destroy()
	{
		// Wait until all GPU work is done
		device.waitIdle();

		destroy_swapchain();

		// Destroy vertex buffer & free its memory
		device.destroyBuffer(vertexBuffer);
		device.freeMemory(vertexBufferMemory);

		// Destroy index buffer
		device.destroyBuffer(indexBuffer);
		device.freeMemory(indexBufferMemory);

		// Destroy graphics pipeline
		device.destroyPipeline(graphicsPipeline);
		device.destroyPipelineLayout(pipelineLayout);
		device.destroyRenderPass(renderPass);

		// Destroy synchronization primitives
		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			device.destroySemaphore(renderFinishedSemaphores[i]);
			device.destroySemaphore(imageAvailableSemaphores[i]);
			device.destroyFence(inFlightFences[i]);
		}

		device.destroyCommandPool(graphicsCommandPool);
		device.destroyCommandPool(transferCommandPool);
		
		instance.destroySurfaceKHR(surface);
		device.destroy();

#ifdef DEBUG
		// Destroy debug messenger only in debug builds
		vk::DispatchLoaderDynamic dldi(instance, vkGetInstanceProcAddr);
		instance.destroyDebugUtilsMessengerEXT(debug_messenger, nullptr, dldi);
#endif

		instance.destroy();
		glfwDestroyWindow(window);
	}

	// Helper function to log an error and exit the application
	void error(std::string message)
	{
		logger->error("An error has occurred: " + message);
		exit(EXIT_FAILURE);
	}

private:
	// Core application objects
	GLFWwindow* window = nullptr;

	// Define how many frames can be processed simultaneously
	const int MAX_FRAMES_IN_FLIGHT = 2;
	uint32_t currentFrame = 0;
	bool framebufferResized = false;

	// Vertex data for a quad (4 unique vertices with color)
	const std::vector<Vertex> vertices =
	{
		{ { -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f } }, // Bottom left - Red
		{ {  0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f } }, // Bottom right - Green
		{ {  0.5f,  0.5f }, { 0.0f, 0.0f, 1.0f } }, // Top right - Blue
		{ { -0.5f,  0.5f }, { 1.0f, 1.0f, 0.0f } }  // Top left - Yellow
	};

	// Index data defining two triangles from the 4 vertices
	const std::vector<uint16_t> indices =
	{
		0, 1, 2,  // First triangle (bottom left -> bottom right -> top right)
		2, 3, 0   // Second triangle (top right -> top left -> bottom left)
	};

	vk::Instance instance;
	vk::DebugUtilsMessengerEXT debug_messenger;
	vk::PhysicalDevice physicalDevice;
	vk::Device device;
	vk::SurfaceKHR surface;
	uint32_t presentIdx = 0, graphicsIdx = 0, transferIdx = 0;
	vk::Queue presentQueue, graphicsQueue, transferQueue;
	vk::SwapchainKHR swapchain;
	std::vector<vk::Image> swapImages;
	std::vector<vk::ImageView> swapImageViews;
	vk::Extent2D swapExtent;
	vk::Format swapFormat{};
	vk::RenderPass renderPass;
	vk::PipelineLayout pipelineLayout;
	vk::Pipeline graphicsPipeline;
	std::vector<vk::Framebuffer> swapFramebuffers;
	vk::CommandPool graphicsCommandPool, transferCommandPool;
	std::vector<vk::CommandBuffer> commandBuffers;
	std::vector<vk::Semaphore> imageAvailableSemaphores, renderFinishedSemaphores;
	std::vector<vk::Fence> inFlightFences;
	vk::Buffer vertexBuffer, indexBuffer;
	vk::DeviceMemory vertexBufferMemory, indexBufferMemory;

private:
	// Helper to read binary file (e.g., SPIR-V shader)
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

	// Create GLFW window and Vulkan surface
	void create_window()
	{
		if (!glfwInit()) exit(EXIT_FAILURE);

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		window = glfwCreateWindow(1280, 720, application_name.c_str(), NULL, NULL);
		if (!window) error("Failed to create window!");

		glfwSetWindowUserPointer(window, this);
		glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

		VkResult err = glfwCreateWindowSurface(instance, window, NULL, reinterpret_cast<VkSurfaceKHR*>(&surface));
		if (err != VK_SUCCESS) error("Failed to create window surface");
	}

	// Callback for framebuffer resize in case of device driver not sending resize signal (vk::Result::eErrorOutOfDateKHR)
	static void framebuffer_size_callback(GLFWwindow* window, int width, int height)
	{
		auto app = reinterpret_cast<IndexBuffer*>(glfwGetWindowUserPointer(window));
		app->framebufferResized = true;
	}

	// Create Vulkan device, physical device, logical device
	void create_base_objects()
	{
		// Get the extensions required by GLFW
		uint32_t glfwCount = 0;
		const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwCount);
		std::vector<const char*> requiredExtensions(glfwExtensions, glfwExtensions + glfwCount);

		// Initialize Vulkan with vk-bootstrap
		vkb::InstanceBuilder instanceBuilder;
		instanceBuilder.set_app_name(application_name.c_str())
			.set_app_version(VK_MAKE_VERSION(1, 0, 0))
			.set_engine_name("No Engine")
			.set_engine_version(VK_MAKE_VERSION(0, 0, 0)) // 0.0.0 for no engine
			.enable_extensions(requiredExtensions);

#ifdef DEBUG
		// Setup debug messages only in case of debug build
		instanceBuilder.request_validation_layers();
		instanceBuilder.use_default_debug_messenger();
#endif

		auto instanceRet = instanceBuilder.build();
		if (!instanceRet) error("Failed to create Vulkan instance (" + instanceRet.error().message() + ")");

		debug_messenger = instanceRet.value().debug_messenger;
		instance = instanceRet.value().instance;

		create_window();

		// Setup required device extensions
		std::vector<const char*> requiredDeviceExtensions
		{
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
			VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME
		};

		// Enable synchronization2 (specific to Vulkan 1.3)
		vk::PhysicalDeviceSynchronization2Features synchronization2
		{
			VK_TRUE
		};

		vk::PhysicalDeviceFeatures features{};
		features.geometryShader = true;

		// Select a physical device using vk-bootstrap
		vkb::PhysicalDeviceSelector physDeviceSelector{ instanceRet.value() };
		physDeviceSelector.add_required_extensions(requiredDeviceExtensions)
			.set_surface(surface)
			.set_required_features(features);
		
		auto physRet = physDeviceSelector.select();
		if (!physRet) error("Failed to select physical device (" + physRet.error().message() + ")");

		physicalDevice = physRet.value().physical_device;

		vkb::DeviceBuilder deviceBuilder{ physRet.value() };
		deviceBuilder.add_pNext(&synchronization2);

		// Create logical device
		auto devRet = deviceBuilder.build();
		if (!devRet) error("Failed to create device (" + devRet.error().message() + ")");
		device = devRet.value().device;

		// Retrieve queues
		auto presentQueueRet = devRet.value().get_queue(vkb::QueueType::present);
		presentIdx = devRet.value().get_queue_index(vkb::QueueType::present).value();
		if (!presentQueueRet) error("Failed to get presentation queue: " + presentQueueRet.error().message());

		auto graphicsQueueRet = devRet.value().get_queue(vkb::QueueType::graphics);
		graphicsIdx = devRet.value().get_queue_index(vkb::QueueType::graphics).value();
		if (!graphicsQueueRet) error("Failed to get graphics queue: " + graphicsQueueRet.error().message());

		// Check if device has a dedicated transfer queue. If not, fall back on graphics queue
		auto transferQueueRet = devRet.value().get_dedicated_queue(vkb::QueueType::transfer);
		if (transferQueueRet.error() == vkb::QueueError::transfer_unavailable)
		{
			logger->info("Transfer queue not found, falling back on graphics queue...");
			transferIdx = graphicsIdx;
			transferQueue = graphicsQueueRet.value();
		}
		else
		{
			transferIdx = devRet.value().get_dedicated_queue_index(vkb::QueueType::transfer).value();
			transferQueue = transferQueueRet.value();
		}

		presentQueue = presentQueueRet.value();
		graphicsQueue = graphicsQueueRet.value();
	}

	// Creates swapchain, images, and image views
	void create_swapchain()
	{
		vkb::SwapchainBuilder swapBuilder{ physicalDevice, device, surface, graphicsIdx, presentIdx };
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);

		swapBuilder.use_default_format_selection()
			.use_default_present_mode_selection()
			.use_default_image_usage_flags()
			.set_desired_extent(width, height)
			.set_image_array_layer_count(1);

		auto swapRet = swapBuilder.build();
		if (!swapRet) error("Failed to create swapchain (" + swapRet.error().message() + ")");

		swapchain = swapRet.value().swapchain;

		// Create swapchain image and views
		for (const auto image : swapRet.value().get_images().value()) swapImages.push_back(image);
		for (const auto imageView : swapRet.value().get_image_views().value()) swapImageViews.push_back(imageView);

		swapExtent = swapRet.value().extent;
		swapFormat = static_cast<vk::Format>(swapRet.value().image_format);
	}

	// Helper to create shader module from SPIR-V code
	vk::ShaderModule create_shader_module(std::vector<char>& code)
	{
		
		vk::ShaderModuleCreateInfo moduleInfo({}, code.size(), reinterpret_cast<const uint32_t*>(code.data()));
		return device.createShaderModule(moduleInfo);
	}

	// Create a basic render pass for presenting images
	void create_render_pass()
	{
		vk::AttachmentDescription colorAttachment(
			{},									// No flags
			swapFormat,							// Use defined swapchain format
			vk::SampleCountFlagBits::e1,		// One sample per pixel (no multisampling)
			vk::AttachmentLoadOp::eClear,		// Clear previous contents within render area
			vk::AttachmentStoreOp::eStore,		// Save contents generated during render pass to memory
			vk::AttachmentLoadOp::eDontCare,	// No stencil, don't care
			vk::AttachmentStoreOp::eDontCare,	// No stencil, don't care
			vk::ImageLayout::eUndefined,		// Don't change attachment image subresource format upon render pass start
			vk::ImageLayout::ePresentSrcKHR		// Set format to be presented to screen
		);

		// Reference to a color attachment at index 0, specifying the layout it will be in during the subpass.
		// 'eAttachmentOptimal' ensures the layout is optimal for color attachment operations.
		vk::AttachmentReference colorAttachmentRef(0, vk::ImageLayout::eAttachmentOptimal);

		// Define a subpass within a render pass. This subpass will use the color attachment defined above.
		vk::SubpassDescription subpass(
			{},									// No special flags for this subpass
			vk::PipelineBindPoint::eGraphics,	// This subpass will be bound to the graphics pipeline
			nullptr,							// No input attachments (used when reading from previous passes)
			colorAttachmentRef,					// One color attachment (our colorAttachmentRef)
			nullptr,							// No resolve attachments (used with multisampling)
			nullptr,							// No depth/stencil attachment
			nullptr								// No preserved attachments (attachments not used in this subpass but used in others)
		);

		// Create a dependency to ensure proper synchronization between external operations and this subpass.
		// This ensures that the color attachment is ready before the subpass starts and is properly handled after it ends.
		vk::SubpassDependency dependency(
			VK_SUBPASS_EXTERNAL,								// Source is external to the render pass (e.g., previous frame, presentation engine)
			0,													// Destination is subpass 0 (our subpass)
			vk::PipelineStageFlagBits::eColorAttachmentOutput,	// Wait for operations that write to color attachments to finish
			vk::PipelineStageFlagBits::eColorAttachmentOutput,	// Subpass will perform color attachment output
			{},													// No specific access mask for source (could be more specific for strict sync)
			{},													// No specific access mask for destination
			{}													// No dependency flags
		);

		// Define render pass info with color attachment, subpasses, and dependencies
		vk::RenderPassCreateInfo renderPassInfo({}, colorAttachment, subpass, dependency);

		renderPass = device.createRenderPass(renderPassInfo);
	}

	// Finds an optimal chunk of memory based upon type and flags
	uint32_t find_memory_type(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
	{
		// Query info about available types of memory
		// This structure contains two arrays: memoryTypes and memoryHeaps (eg. VRAM, swap space when VRAM runs out)
		vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();

		// Iterate memory type and properties till find a suitable match is found
		for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
		{
			bool isTypeSuitable = typeFilter & (1 << i);
			bool hasRequiredProperties = (memProperties.memoryTypes[i].propertyFlags & properties) == properties;

			if (isTypeSuitable && hasRequiredProperties) return i;
		}

		error("Unable to find suitable memory type!");
		return 0;
	}

	// Helper to abstract buffer creation
	std::pair<vk::Buffer, vk::DeviceMemory> create_buffer(vk::DeviceSize size, vk::BufferUsageFlags flags, vk::MemoryPropertyFlags properties, vk::SharingMode sharingMode, std::vector<uint32_t> queues)
	{
		vk::BufferCreateInfo bufferInfo(
			{},                 // No creation flags
			size,               // Size of the buffer in bytes
			flags,              // Buffer usage flags (e.g., vertex buffer, transfer source/destination)
			sharingMode,        // Sharing mode: exclusive or concurrent between queue families
			queues              // List of queue family indices that will access the buffer
		);

		vk::Buffer buffer = device.createBuffer(bufferInfo);

		// After the buffer is created, memory must be assigned to it
		// First, query the buffer memory requirments
		vk::MemoryRequirements memReqs = device.getBufferMemoryRequirements(buffer);

		// Assemble the allocation info after finding a suitable memory type
		uint32_t memTypeIndex = find_memory_type(memReqs.memoryTypeBits, properties);
		vk::MemoryAllocateInfo allocInfo(memReqs.size, memTypeIndex);

		// Allocate the vertex buffer memory
		vk::DeviceMemory mem = device.allocateMemory(allocInfo);

		// Finally, bind the memory with no offset
		device.bindBufferMemory(buffer, mem, 0);

		return { buffer, mem };
	}

	// Utility to copy buffer data
	void copy_buffer(vk::Buffer src, vk::Buffer dst, vk::DeviceSize size)
	{
		vk::CommandBufferAllocateInfo allocInfo(transferCommandPool, vk::CommandBufferLevel::ePrimary, 1);

		vk::CommandBuffer commandBuffer = device.allocateCommandBuffers(allocInfo)[0];

		// Let the driver know our intent
		vk::CommandBufferBeginInfo beginInfo({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
		commandBuffer.begin(beginInfo);

		// Define region to copy & copy the data
		vk::BufferCopy copyRegion(0, 0, size);
		commandBuffer.copyBuffer(src, dst, copyRegion);

		commandBuffer.end();

		// Submit the command buffer, wait till idle
		vk::SubmitInfo submitInfo{};
		submitInfo.setCommandBuffers(commandBuffer);
		transferQueue.submit(submitInfo);
		transferQueue.waitIdle();

		// Destroy command buffer
		device.freeCommandBuffers(transferCommandPool, commandBuffer);
	}

	// Create vertex buffer and stage vertex data using a staging buffer
	void create_vertex_buffer()
	{
		// Determine queue families that will access the buffer
		// Default: both graphics and transfer queues
		std::vector<uint32_t> bufferQueueIdx = { graphicsIdx, transferIdx };
		vk::DeviceSize size = sizeof(vertices[0]) * vertices.size(); // Total byte size of the vertex data
		vk::SharingMode sharingMode = vk::SharingMode::eConcurrent;  // Allow concurrent access if queues differ

		// If graphics and transfer queues are the same, use exclusive mode for better performance
		if (graphicsIdx == transferIdx)
		{
			bufferQueueIdx = { graphicsIdx };
			sharingMode = vk::SharingMode::eExclusive;
		}

		// Create a staging buffer in host-visible memory to upload vertex data from the CPU
		auto [stagingBuffer, stagingBufferMemory] = create_buffer(
			size,
			vk::BufferUsageFlagBits::eTransferSrc, // Will serve as the source of a transfer operation
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, // CPU-visible memory
			sharingMode,
			bufferQueueIdx
		);

		// Map the staging buffer memory and copy vertex data from CPU to GPU-accessible staging memory
		void* data = device.mapMemory(stagingBufferMemory, 0, size);
		memcpy(data, vertices.data(), static_cast<size_t>(size));
		device.unmapMemory(stagingBufferMemory); // Unmap after copying

		// Create the final vertex buffer in device-local memory for optimal GPU performance
		std::tie(vertexBuffer, vertexBufferMemory) = create_buffer(
			size,
			vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, // Used as copy target and vertex buffer
			vk::MemoryPropertyFlagBits::eDeviceLocal, // High-performance GPU-local memory (not CPU-visible)
			sharingMode,
			bufferQueueIdx
		);

		// Copy the staging buffer to the vertex buffer on device-local memory
		copy_buffer(stagingBuffer, vertexBuffer, size);

		device.destroyBuffer(stagingBuffer);
		device.freeMemory(stagingBufferMemory);
	}

	void create_index_buffer()
	{
		// Determine queue families that will access the buffer
		// Default: both graphics and transfer queues
		std::vector<uint32_t> bufferQueueIdx = { graphicsIdx, transferIdx };
		vk::DeviceSize size = sizeof(indices[0]) * indices.size(); // Total byte size of the vertex data
		vk::SharingMode sharingMode = vk::SharingMode::eConcurrent;  // Allow concurrent access if queues differ

		// If graphics and transfer queues are the same, use exclusive mode for better performance
		if (graphicsIdx == transferIdx)
		{
			bufferQueueIdx = { graphicsIdx };
			sharingMode = vk::SharingMode::eExclusive;
		}

		// Create a staging buffer in host-visible memory to upload index data from the CPU
		auto [stagingBuffer, stagingBufferMemory] = create_buffer(
			size,
			vk::BufferUsageFlagBits::eTransferSrc, // Will serve as the source of a transfer operation
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, // CPU-visible memory
			sharingMode,
			bufferQueueIdx
		);

		// Map the staging buffer memory and copy vertex data from CPU to GPU-accessible staging memory
		void* data = device.mapMemory(stagingBufferMemory, 0, size);
		memcpy(data, indices.data(), static_cast<size_t>(size));
		device.unmapMemory(stagingBufferMemory); // Unmap after copying

		// Create the final vertex buffer in device-local memory for optimal GPU performance
		std::tie(indexBuffer, indexBufferMemory) = create_buffer(
			size,
			vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, // Used as copy target and vertex buffer
			vk::MemoryPropertyFlagBits::eDeviceLocal, // High-performance GPU-local memory (not CPU-visible)
			sharingMode,
			bufferQueueIdx
		);

		// Copy the staging buffer to the vertex buffer on device-local memory
		copy_buffer(stagingBuffer, indexBuffer, size);

		device.destroyBuffer(stagingBuffer);
		device.freeMemory(stagingBufferMemory);
	}

	// Create graphics pipeline including shaders, pipeline layout, and fixed function stages
	void create_graphics_pipeline()
	{
		auto vertShader = read_file("src/shader/vert.spv");
		auto fragShader = read_file("src/shader/frag.spv");

		vk::ShaderModule vertModule = create_shader_module(vertShader);
		vk::ShaderModule fragModule = create_shader_module(fragShader);

		// Shader stage creation
		vk::PipelineShaderStageCreateInfo vertStageInfo({}, vk::ShaderStageFlagBits::eVertex, vertModule, "main");
		vk::PipelineShaderStageCreateInfo fragStageInfo({}, vk::ShaderStageFlagBits::eFragment, fragModule, "main");

		std::vector<vk::PipelineShaderStageCreateInfo> shaderStages { vertStageInfo, fragStageInfo };

		// Vertex input: tell Vulkan how to interpret vertex input data passed to vertex shader
		auto bindingDescription = Vertex::get_binding_description();
		auto attributeDescriptions = Vertex::get_attribute_descriptions();

		vk::PipelineVertexInputStateCreateInfo vertexInputInfo(
			{},						// No special flags
			bindingDescription,		// Binding description
			attributeDescriptions	// Attribute descriptions (position and color)
		);

		// Input assembly: draw triangle from vertices
		vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo({}, vk::PrimitiveTopology::eTriangleList, vk::False);

		// Viewport and scissor
		vk::Viewport viewport(0.0f, 0.0f, (float)swapExtent.width, (float)swapExtent.height, 0.0f, 1.0f);
		vk::Rect2D scissor({}, swapExtent);
		vk::PipelineViewportStateCreateInfo viewportStateInfo({}, viewport, scissor);

		std::vector<vk::DynamicState> dynamicStates
		{
			vk::DynamicState::eViewport,
			vk::DynamicState::eScissor
		};

		vk::PipelineDynamicStateCreateInfo dynamicStateInfo({}, dynamicStates);

		// Configure rasterization stage of the graphics pipeline
		vk::PipelineRasterizationStateCreateInfo rasterizerInfo(
			{},										// No special flags
			vk::False,								// Disable depth clamping (clipping instead of clamping fragments outside near/far planes)
			vk::False,								// Disable rasterizer discard (enables actual rasterization)
			vk::PolygonMode::eFill,					// Fill polygons (can also use eLine or ePoint for wireframe/point rendering)
			vk::CullModeFlagBits::eBack,			// Cull back-facing polygons to improve performance
			vk::FrontFace::eClockwise,				// Clockwise vertex winding is considered front-facing
			vk::False								// Disable depth bias (used for things like shadow mapping)
		);

		// Set the width of lines when rendering in line mode (must be 1.0 unless wide lines are enabled via GPU features)
		rasterizerInfo.lineWidth = 1.0f;

		// Multisampling
		vk::PipelineMultisampleStateCreateInfo multisamplingInfo{};

		// Color blending
		vk::PipelineColorBlendAttachmentState colorBlendAttachmentInfo{};
		colorBlendAttachmentInfo.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
		colorBlendAttachmentInfo.blendEnable = vk::False;

		vk::PipelineColorBlendStateCreateInfo colorBlendInfo({}, vk::False, vk::LogicOp::eNoOp, colorBlendAttachmentInfo, {});

		// Pipeline info: no descriptors or push constants
		vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayout = device.createPipelineLayout(pipelineLayoutInfo);

		// Create graphics pipeline configuration with various pipeline stages and fixed-function states
		vk::GraphicsPipelineCreateInfo pipelineInfo(
			{},										// No special flags
			shaderStages,							// Array of shader stages (e.g., vertex and fragment shaders)
			&vertexInputInfo,						// Pointer to vertex input state (binding and attribute descriptions)
			&inputAssemblyInfo,						// Pointer to input assembly state (e.g., topology like Triangle_Unbuffered list)
			{},										// No tessellation state (not used unless tessellation shaders are enabled)
			&viewportStateInfo,						// Pointer to viewport and scissor rectangle state
			&rasterizerInfo,						// Pointer to rasterization state (polygon mode, culling, front face, etc.)
			&multisamplingInfo,						// Pointer to multisample state (anti-aliasing configuration)
			{},										// No depth/stencil state (not used here)
			&colorBlendInfo,						// Pointer to color blending state (blend operations per attachment)
			&dynamicStateInfo,						// Pointer to dynamic state (e.g., dynamic viewport/scissor)
			pipelineLayout,							// Pipeline layout (descriptor sets and push constants layout)
			renderPass								// Render pass this pipeline will be used with
			// subpass index (optional) and base pipeline info (optional) not provided here
		);


		vk::ResultValue<vk::Pipeline> result = device.createGraphicsPipeline(VK_NULL_HANDLE, pipelineInfo);
		if (result.result != vk::Result::eSuccess) error("Failed to create graphics pipeline!");

		graphicsPipeline = result.value;

		// Cleanup shader modules after pipeline creation
		device.destroyShaderModule(vertModule);
		device.destroyShaderModule(fragModule);
	}

	// Create framebuffers for each swapchain image
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

	// Create command pool for graphics and transfer and allocate two command buffers per frame
	void create_command_objects()
	{
		vk::CommandPoolCreateInfo graphicsPoolInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, graphicsIdx);
		graphicsCommandPool = device.createCommandPool(graphicsPoolInfo);

		vk::CommandPoolCreateInfo transferPoolInfo(vk::CommandPoolCreateFlagBits::eTransient, transferIdx);
		transferCommandPool = device.createCommandPool(transferPoolInfo);

		commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

		vk::CommandBufferAllocateInfo bufferInfo(graphicsCommandPool, vk::CommandBufferLevel::ePrimary, static_cast<uint32_t>(commandBuffers.size()));
		commandBuffers = device.allocateCommandBuffers(bufferInfo);
	}

	// Record command buffer for each frame
	void record_command_buffer(vk::CommandBuffer commandBuffer, uint32_t imageIdx)
	{
		vk::CommandBufferBeginInfo beginInfo{};
		commandBuffer.begin(beginInfo);

		vk::ClearValue clearColor({ 0.0f, 0.0f, 0.0f, 1.0f });

		vk::Rect2D renderArea({ 0,0 }, swapExtent);
		vk::RenderPassBeginInfo renderPassInfo(renderPass, swapFramebuffers[imageIdx], renderArea, clearColor);

		commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);

		// Array of vertex buffers to bind (in this case, only one buffer)
		vk::Buffer vertexBuffers[] = { vertexBuffer };

		// Array of offsets into each vertex buffer (start at beginning of buffer)
		vk::DeviceSize vertexOffsets[] = { 0 };

		// Bind the vertex buffer(s) to the input assembly stage of the graphics pipeline
		// Parameters:
		//   0              -> First binding index (binding location in the shader)
		//   vertexBuffers  -> Array of buffers to bind
		//   offsets        -> Byte offsets into each buffer
		commandBuffer.bindVertexBuffers(0, vertexBuffers, vertexOffsets);

		// Bind the index buffer
		commandBuffer.bindIndexBuffer(indexBuffer, 0, vk::IndexType::eUint16);

		vk::Viewport viewport(0.0f, 0.0f, static_cast<float>(swapExtent.width), static_cast<float>(swapExtent.height), 0.0f, 1.0f);
		commandBuffer.setViewport(0, viewport);
		commandBuffer.setScissor(0, renderArea);

		// Issue a draw call to render primitives using the currently bound pipeline and vertex buffers
		// Parameters:
		//   vertexCount        -> Number of vertices to draw (size of the vertex array)
		//   instanceCount      -> Number of instances to draw (1 = no instancing)
		//   firstVertex        -> Index of the first vertex to start with in the vertex buffer
		//   firstInstance      -> Index of the first instance (used for instancing)
		// In this case, we're drawing all vertices once, without instancing
		//commandBuffer.draw(static_cast<uint32_t>(vertices.size()), 1, 0, 0);
		commandBuffer.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

		commandBuffer.endRenderPass();
		commandBuffer.end();
	}

	// Create fences and semaphores for synchronization
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
	
	// Main rendering logic per frame
	void draw_frame()
	{
		auto res1 = device.waitForFences(inFlightFences[currentFrame], vk::True, UINT64_MAX);

		if (res1 != vk::Result::eSuccess) error("Fence operation failed!");

		auto res2 = device.acquireNextImageKHR(swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrame], {});

		// Recreate swapchain if inadequate, exit draw_frame
		if (res2.result == vk::Result::eErrorOutOfDateKHR)
		{
			recreate_swapchain();
			return;
		}
		else if (res2.result != vk::Result::eSuccess && res2.result != vk::Result::eSuboptimalKHR) error("Failed to acquire next image.");
		uint32_t imageIdx = res2.value;

		// Only reset fences if we are submitting work with it
		device.resetFences(inFlightFences[currentFrame]);

		commandBuffers[currentFrame].reset();
		record_command_buffer(commandBuffers[currentFrame], imageIdx);

		const std::vector<vk::Semaphore> waitSemaphores(1, imageAvailableSemaphores[currentFrame]);
		const std::vector<vk::Semaphore> signalSemaphores(1,renderFinishedSemaphores[currentFrame]);
		const std::vector<vk::PipelineStageFlags> waitStages(1, vk::PipelineStageFlagBits::eColorAttachmentOutput);

		// Structure describing how command buffers should be submitted to a queue
		vk::SubmitInfo submitInfo{};

		// Number of semaphores to wait on before executing the command buffer
		submitInfo.setWaitSemaphoreCount(static_cast<uint32_t>(waitSemaphores.size()));

		// Pointer to the array of semaphores that must be signaled before this submission begins
		submitInfo.setPWaitSemaphores(waitSemaphores.data());

		// Number of semaphores to signal once the command buffer has finished executing
		submitInfo.setSignalSemaphoreCount(static_cast<uint32_t>(signalSemaphores.size()));

		// Pointer to the array of semaphores to signal after command buffer execution
		submitInfo.setPSignalSemaphores(signalSemaphores.data());

		// Array of pipeline stage flags indicating the stages to wait at for each corresponding wait semaphore
		submitInfo.setPWaitDstStageMask(waitStages.data());

		// Number of command buffers to submit (just one in this case)
		submitInfo.setCommandBufferCount(1);

		// Pointer to the command buffer to execute (specific to the current frame)
		submitInfo.setPCommandBuffers(&commandBuffers[currentFrame]);

		// Submit the submit info
		graphicsQueue.submit(submitInfo, inFlightFences[currentFrame]);

		vk::PresentInfoKHR presentInfo(signalSemaphores, swapchain, imageIdx);

		// Recreate swapchain if out-of-date or suboptimal
		auto res3 = graphicsQueue.presentKHR(presentInfo);
		if (res3 == vk::Result::eErrorOutOfDateKHR || res3 == vk::Result::eSuboptimalKHR || framebufferResized)
		{
			framebufferResized = false;
			recreate_swapchain();
		}
		else if (res3 != vk::Result::eSuccess) error("Failed to present!");

		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	// Cleanup swapchain objects for application exit or swapchain recreation
	void destroy_swapchain()
	{
		// Destroy all framebuffers
		for (const auto& framebuffer : swapFramebuffers) device.destroyFramebuffer(framebuffer);

		// Destroy image views
		for (const auto& imageView : swapImageViews) device.destroyImageView(imageView);
		swapImageViews.clear();

		// Clear images (destroyed by 'destroySwapchainKHR')
		swapImages.clear();

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

		// Destroy previous swapchain before recreation
		destroy_swapchain();

		create_swapchain();
		create_framebuffers();
	}
};

// Application execution and logic
int main()
{
	IndexBuffer app;

	try
	{
		app.init();
		app.run();
		app.destroy();
	}
	catch (const std::exception& e)
	{
		app.error(e.what());
	}

	return EXIT_SUCCESS;
}