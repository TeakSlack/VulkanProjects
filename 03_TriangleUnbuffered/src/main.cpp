#include <exception>
#include <vector>

#include <spdlog/spdlog.h>
#include <vulkan_app_base.h>
#include <pipeline_builder.h>

class TriangleBuffered : public VulkanAppBase
{
public:
	TriangleBuffered() : VulkanAppBase()
	{
		init();
		spdlog::info("Triangle application initialized");
	}

	~TriangleBuffered()
	{
		destroy();
		spdlog::info("Triangle application destroyed");
	}

	void run()
	{
		while (!glfwWindowShouldClose(m_Window))
		{
			glfwPollEvents(); // Handle window events
			draw_frame(); // Draw one frame
		}
	}

private:
	vk::RenderPass m_RenderPass; // Render pass for the triangle rendering
	std::vector<vk::Framebuffer> m_Framebuffers; // Framebuffers for each swapchain image
	std::vector<vk::CommandBuffer> m_CommandBuffers; // Command buffers for recording draw commands
	vk::UniquePipeline m_GraphicsPipeline; // Graphics pipeline for rendering the triangle

	int16_t m_CurrentFrame = 0; // Current frame for rendering

private:
	void init() override
	{
		VulkanAppBase::init();
		create_command_buffer();
		create_render_pass();
		create_framebuffers();
		create_graphics_pipeline();
	}

	void destroy() override
	{
		m_Device.waitIdle();

		destroy_framebuffers();
		m_Device.destroyRenderPass(m_RenderPass);

		for (auto& commandBuffer : m_CommandBuffers)
		{
			m_Device.freeCommandBuffers(m_GraphicsCommandPool, commandBuffer);
		}
	}

	void create_command_buffer()
	{
		m_CommandBuffers.resize(m_FramesInFlight);

		vk::CommandBufferAllocateInfo bufferInfo(m_GraphicsCommandPool, vk::CommandBufferLevel::ePrimary, static_cast<uint32_t>(m_CommandBuffers.size()));
		m_CommandBuffers = m_Device.allocateCommandBuffers(bufferInfo);
	}

	void create_render_pass()
	{
		vk::AttachmentDescription colorAttachment(
			{},									// No flags
			m_SwapFormat,						// Use defined swapchain format
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

		m_RenderPass = m_Device.createRenderPass(renderPassInfo);
	}

	void create_framebuffers()
	{
		m_Framebuffers.resize(m_ImageViews.size());

		for (size_t i = 0; i < m_Framebuffers.size(); i++)
		{
			std::vector<vk::ImageView> attachments{ m_ImageViews[i] };

			vk::FramebufferCreateInfo framebufferInfo({}, m_RenderPass, attachments, m_SwapExtent.width, m_SwapExtent.height, 1);

			m_Framebuffers[i] = m_Device.createFramebuffer(framebufferInfo);
		}
	}

	void create_graphics_pipeline()
	{
		vk::PipelineColorBlendAttachmentState colorBlendAttachmentInfo{};
		colorBlendAttachmentInfo.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
		colorBlendAttachmentInfo.blendEnable = vk::False;

		PipelineBuilder pipelineBuilder(PipelineType::Graphics);

		pipelineBuilder.add_shader_stage("src/shader/vert.spv", vk::ShaderStageFlagBits::eVertex)
			.add_shader_stage("src/shader/frag.spv", vk::ShaderStageFlagBits::eFragment)
			.set_primitive_topology(vk::PrimitiveTopology::eTriangleStrip)
			.add_viewport(0.0f, 0.0f, static_cast<float>(m_SwapExtent.width), static_cast<float>(m_SwapExtent.height), 0.0f, 1.0f)
			.add_scissor(0, 0, m_SwapExtent.width, m_SwapExtent.height)
			.add_dynamic_state(vk::DynamicState::eViewport)
			.add_dynamic_state(vk::DynamicState::eScissor)
			.add_color_blend_attachment(colorBlendAttachmentInfo)
			.set_render_pass(m_RenderPass, 0);

		m_GraphicsPipeline = pipelineBuilder.build(m_Device);
	}

	void record_command_buffer(vk::CommandBuffer commandBuffer, uint32_t imageIdx)
	{
		vk::CommandBufferBeginInfo beginInfo{};
		commandBuffer.begin(beginInfo);

		vk::ClearValue clearColor({ 0.0f, 0.0f, 0.0f, 1.0f });

		vk::Rect2D renderArea({ 0,0 }, m_SwapExtent);
		vk::RenderPassBeginInfo renderPassInfo(m_RenderPass, m_Framebuffers[imageIdx], renderArea, clearColor);

		commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_GraphicsPipeline.get());

		vk::Viewport viewport(0.0f, 0.0f, static_cast<float>(m_SwapExtent.width), static_cast<float>(m_SwapExtent.height), 0.0f, 1.0f);
		commandBuffer.setViewport(0, viewport);
		commandBuffer.setScissor(0, renderArea);

		commandBuffer.draw(3, 1, 0, 0);
		commandBuffer.endRenderPass();
		commandBuffer.end();
	}

	void draw_frame()
	{
		auto res1 = m_Device.waitForFences(m_InFlightFences[m_CurrentFrame], vk::True, UINT64_MAX);

		if (res1 != vk::Result::eSuccess) error("Fence operation failed!");

		auto res2 = m_Device.acquireNextImageKHR(m_Swapchain, UINT64_MAX, m_ImageAvailableSemaphores[m_CurrentFrame], {});

		// Recreate swapchain if inadequate, exit draw_frame
		if (res2.result == vk::Result::eErrorOutOfDateKHR)
		{
			recreate_swapchain();
			return;
		}
		else if (res2.result != vk::Result::eSuccess && res2.result != vk::Result::eSuboptimalKHR) error("Failed to acquire next image.");
		uint32_t imageIdx = res2.value;

		// Only reset fences if we are submitting work with it
		m_Device.resetFences(m_InFlightFences[m_CurrentFrame]);

		m_CommandBuffers[m_CurrentFrame].reset();
		record_command_buffer(m_CommandBuffers[m_CurrentFrame], imageIdx);

		const std::vector<vk::Semaphore> waitSemaphores(1, m_ImageAvailableSemaphores[m_CurrentFrame]);
		const std::vector<vk::Semaphore> signalSemaphores(1, m_RenderFinishedSemaphores[m_CurrentFrame]);
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
		submitInfo.setPCommandBuffers(&m_CommandBuffers[m_CurrentFrame]);

		// Submit the submit info
		m_GraphicsQueue.submit(submitInfo, m_InFlightFences[m_CurrentFrame]);

		vk::PresentInfoKHR presentInfo(signalSemaphores, m_Swapchain, imageIdx);

		// Recreate swapchain if out-of-date or suboptimal
		auto res3 = m_GraphicsQueue.presentKHR(presentInfo);
		if (res3 == vk::Result::eErrorOutOfDateKHR || res3 == vk::Result::eSuboptimalKHR || m_FramebufferResized)
		{
			m_FramebufferResized = false;
			recreate_swapchain();
		}
		else if (res3 != vk::Result::eSuccess) error("Failed to present!");

		m_CurrentFrame = (m_CurrentFrame + 1) % m_FramesInFlight;
	}

	void recreate_swapchain() override
	{
		VulkanAppBase::recreate_swapchain();

		destroy_framebuffers();
		create_framebuffers();
	}

	void destroy_framebuffers()
	{
		for (auto framebuffer : m_Framebuffers)
		{
			m_Device.destroyFramebuffer(framebuffer);
		}

		m_Framebuffers.clear();
	}
};

int main()
{
	TriangleBuffered app;

	try
	{
		app.run();
	}
	catch (std::exception& e)
	{
		spdlog::error(e.what());
	}
}