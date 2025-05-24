#include <cassert>
#include <fstream>

#include <spdlog/spdlog.h>

#include "pipeline_builder.h"

// Constructor for PipelineBuilder.
// Ensures the provided Vulkan device is valid and stores it for later pipeline/shader creation.
PipelineBuilder::PipelineBuilder(vk::PhysicalDevice physicalDevice, vk::Device device)
	: m_PhysicalDevice(physicalDevice), m_Device(device)
{
	assert(physicalDevice && "vk::PhysicalDevice has not been initialized!");
	assert(device && "vk::Device has not been initialized!");

	m_DeviceProperties = m_PhysicalDevice.getProperties();
	m_DeviceFeatures = m_PhysicalDevice.getFeatures();

	m_MaxViewports = m_DeviceProperties.limits.maxViewports;
}

// Destructor for PipelineBuilder.
PipelineBuilder::~PipelineBuilder()
{
	for (auto& shaderModule : m_ShaderStages)
	{
		if (shaderModule.module)
		{
			m_Device.destroyShaderModule(shaderModule.module);
		}
	}
}

// Adds a shader stage to the pipeline by loading a SPIR-V shader from file.
// shaderPath: Path to the SPIR-V binary file.
// stage: Enum value indicating the shader stage (e.g., vertex, fragment).
// Returns a reference to this builder for method chaining.
PipelineBuilder& PipelineBuilder::add_shader_stage(const std::string& shaderPath, vk::ShaderStageFlagBits stage)
{
	// Check if a shader of this stage type already exists
	auto it = std::find_if(
		m_ShaderStages.begin(),
		m_ShaderStages.end(),
		[stage](const ShaderStage& s) { return s.type == stage; }
	);

	if (it != m_ShaderStages.end())
	{
		spdlog::error("A shader for this stage already exists in the pipeline (stage: {}).", static_cast<int>(stage));
		throw std::runtime_error("Duplicate shader stage in pipeline.");
	}

	// Open the shader file in binary mode, reading from the end to get the file size.
	std::ifstream file(shaderPath, std::ifstream::ate | std::ifstream::binary);

	if (!file.is_open())
	{
		spdlog::error("Failed to open file " + shaderPath);
		exit(EXIT_FAILURE);
	}

	// Read the entire file into a buffer.
	size_t fileSize = file.tellg();
	std::vector<char> shaderCode(fileSize);

	file.seekg(0);
	file.read(shaderCode.data(), fileSize);
	file.close();

	// Create a Vulkan shader module from the loaded SPIR-V code.
	vk::ShaderModuleCreateInfo shaderModuleInfo({}, shaderCode.size(), reinterpret_cast<const uint32_t*>(shaderCode.data()));
	vk::ShaderModule module = m_Device.createShaderModule(shaderModuleInfo);

	// Store the shader module and its stage for later pipeline creation.
	ShaderStage shaderStage =
	{
		.module = module,
		.type = stage
	};

	m_ShaderStages.push_back(shaderStage);

	// Return reference to this builder to allow chaining.
	return *this;
}

PipelineBuilder& PipelineBuilder::set_primitive_topology(vk::PrimitiveTopology topology)
{
	// Set the primitive topology for the input assembly state.
	state.inputAssembly.topology = topology;
	return *this;
}

PipelineBuilder& PipelineBuilder::set_primitive_restart(bool enable = true)
{
	// Enable or disable primitive restart in the input assembly state.
	state.inputAssembly.primitiveRestartEnable = enable ? vk::True : vk::False;
	return *this;
}

PipelineBuilder& PipelineBuilder::set_dynamic_topology(bool enable)
{
	toggle_dynamic_state(enable, vk::DynamicState::ePrimitiveTopology);
	return *this;
}

PipelineBuilder& PipelineBuilder::set_patch_control_points(uint32_t points)
{
	state.tessellationState.patchControlPoints = points;
}

PipelineBuilder& PipelineBuilder::add_dynamic_state(vk::DynamicState dynamicState)
{
	// Add a dynamic state to the pipeline.
	state.dynamicStates.push_back(dynamicState);
	return *this;
}

PipelineBuilder& PipelineBuilder::add_viewport(vk::Viewport viewport)
{
	// Add a viewport to the pipeline.
	state.viewportState.viewports.push_back(viewport);
	return *this;
}

PipelineBuilder& PipelineBuilder::add_viewport(float x, float y, float width, float height, float minDepth, float maxDepth)
{
	// Create a viewport and add it to the pipeline.
	vk::Viewport viewport(x, y, width, height, minDepth, maxDepth);
	state.viewportState.viewports.push_back(viewport);
	return *this;
}

PipelineBuilder& PipelineBuilder::add_scissor(vk::Rect2D scissor)
{
	// Add a scissor rectangle to the pipeline.
	state.viewportState.scissors.push_back(scissor);
	return *this;
}

PipelineBuilder& PipelineBuilder::add_scissor(int32_t x, int32_t y, uint32_t width, uint32_t height)
{
	// Create a scissor rectangle and add it to the pipeline.
	vk::Rect2D scissor({ x, y }, { width, height });
	state.viewportState.scissors.push_back(scissor);
	return *this;
}

PipelineBuilder& PipelineBuilder::set_depth_clamp(bool enable)
{
	// Enable or disable depth clamping in the rasterization state.
	state.rasterizationState.depthClamp = enable ? vk::True : vk::False;
	return *this;
}

PipelineBuilder& PipelineBuilder::set_rasterizer_discard(bool enable)
{
	// Enable or disable rasterizer discard in the rasterization state.
	state.rasterizationState.rasterizerDiscard = enable ? vk::True : vk::False;
	return *this;
}

PipelineBuilder& PipelineBuilder::set_polygon_mode(vk::PolygonMode mode)
{
	// Set the polygon mode in the rasterization state.
	state.rasterizationState.polygonMode = mode;
	return *this;
}

PipelineBuilder& PipelineBuilder::set_cull_mode(vk::CullModeFlags mode)
{
	// Set the cull mode in the rasterization state.
	state.rasterizationState.cullMode = mode;
	return *this;
}

PipelineBuilder& PipelineBuilder::set_front_face(vk::FrontFace face)
{
	// Set the front face in the rasterization state.
	state.rasterizationState.frontFace = face;
	return *this;
}

PipelineBuilder& PipelineBuilder::set_depth_bias(bool enable)
{
	// Enable or disable depth bias in the rasterization state.
	state.rasterizationState.depthBias = enable ? vk::True : vk::False;
	return *this;
}

PipelineBuilder& PipelineBuilder::set_line_width(float width)
{
	// Set the line width in the rasterization state.
	state.rasterizationState.lineWidth = width;
	return *this;
}

PipelineBuilder& PipelineBuilder::set_depth_bias_constant(float constant)
{
	// Set the depth bias constant in the rasterization state.
	state.rasterizationState.depthBiasConstant = constant;
	return *this;
}

PipelineBuilder& PipelineBuilder::set_depth_bias_clamp(float clamp)
{
	// Set the depth bias clamp in the rasterization state.
	state.rasterizationState.depthBiasClamp = clamp;
	return *this;
}

PipelineBuilder& PipelineBuilder::set_depth_bias_slope(float slope)
{
	// Set the depth bias slope in the rasterization state.
	state.rasterizationState.depthBiasSlope = slope;
	return *this;
}

PipelineBuilder& PipelineBuilder::set_dynamic_line_width(bool enable)
{
	toggle_dynamic_state(enable, vk::DynamicState::eLineWidth);
	return *this;
}

PipelineBuilder& PipelineBuilder::set_dynamic_depth_bias(bool enable)
{
	toggle_dynamic_state(enable, vk::DynamicState::eDepthBias);
	return *this;
}

PipelineBuilder& PipelineBuilder::set_sample_count(vk::SampleCountFlagBits count) 
{
	state.multisampleState.rasterizationSamples = count;
	return *this;
}

PipelineBuilder& PipelineBuilder::set_sample_shading(bool enable, float minSampleShading)
{
	state.multisampleState.sampleShadingEnable = enable ? vk::True : vk::False;
	state.multisampleState.minSampleShading = minSampleShading;
	return *this;
}

PipelineBuilder& PipelineBuilder::add_sample_mask(vk::SampleMask mask)
{
	// Add a sample mask to the multisample state.
	state.multisampleState.sampleMasks.push_back(mask);
	return *this;
}

PipelineBuilder& PipelineBuilder::set_alpha_to_coverage(bool enable)
{
	// Enable or disable alpha-to-coverage in the multisample state.
	state.multisampleState.alphaToCoverageEnable = enable ? vk::True : vk::False;
	return *this;
}

PipelineBuilder& PipelineBuilder::set_alpha_to_one(bool enable)
{
	// Enable or disable alpha-to-one in the multisample state.
	state.multisampleState.alphaToOneEnable = enable ? vk::True : vk::False;
	return *this;
}

std::optional<vk::Pipeline> PipelineBuilder::build()
{
	// Shader stage: create shader stages
	std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;

	for (const auto& stage : m_ShaderStages)
	{
		vk::PipelineShaderStageCreateInfo stageInfo{};
		stageInfo.stage = stage.type;
		stageInfo.module = stage.module;
		stageInfo.pName = "main";
		shaderStages.push_back(stageInfo);
	}

	// Vertex input: define how Vulkan should interpret vertex input
	vk::PipelineVertexInputStateCreateInfo vertexInputInfo(
		{},
		state.vertexInput.bindingDescriptions,
		state.vertexInput.attributeDescriptions
	);

	vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo(
		{},
		state.inputAssembly.topology,
		state.inputAssembly.primitiveRestartEnable
	);

	if (state.tessellationState.patchControlPoints > 0 &&
		state.inputAssembly.topology != vk::PrimitiveTopology::ePatchList) {
		throw std::runtime_error("Tessellation requires patch list topology");
	}

	vk::PipelineTessellationStateCreateInfo tessellationInfo({}, state.tessellationState.patchControlPoints);

	vk::PipelineDynamicStateCreateInfo dynamicStateInfo({}, state.dynamicStates);

	if (state.viewportState.viewports.size() > m_MaxViewports)
	{
		spdlog::error("Number of viewports exceeds the maximum limit.");
		throw std::runtime_error("Too many viewports.");
	}

	if (state.viewportState.scissors.size() > m_MaxViewports)
	{
		spdlog::error("Number of scissors exceeds the maximum limit.");
		throw std::runtime_error("Too many scissors.");
	}

	if (state.viewportState.viewports.size() != state.viewportState.scissors.size())
	{
		spdlog::error("Number of viewports and scissors do not match.");
		throw std::runtime_error("Mismatched viewports and scissors.");
	}

	vk::PipelineViewportStateCreateInfo viewportStateInfo(
		{},
		state.viewportState.viewports,
		state.viewportState.scissors
	);

	if (state.rasterizationState.lineWidth != 1.0f && !m_DeviceFeatures.wideLines)
	{
		spdlog::error("Wide lines requested, but not available.");
		throw std::runtime_error("Wide lines not supported.");
	}

	if (state.rasterizationState.depthClamp && !m_DeviceFeatures.depthClamp)
	{
		spdlog::error("Depth clamp requested, but not available.");
		throw std::runtime_error("Depth clamp not supported.");
	}
	
	vk::PipelineRasterizationStateCreateInfo rasterizerInfo(
		{},
		state.rasterizationState.depthClamp,
		state.rasterizationState.rasterizerDiscard,
		state.rasterizationState.polygonMode,
		state.rasterizationState.cullMode,
		state.rasterizationState.frontFace,
		state.rasterizationState.depthBias,
		state.rasterizationState.lineWidth,
		state.rasterizationState.depthBiasConstant,
		state.rasterizationState.depthBiasClamp,
		state.rasterizationState.depthBiasSlope
	);

	if (!(m_DeviceProperties.limits.framebufferColorSampleCounts & state.multisampleState.rasterizationSamples))
	{
		spdlog::error("Sample count not supported by device.");
		throw std::runtime_error("Unsupported sample count.");
	}

	if (!m_DeviceFeatures.sampleRateShading && state.multisampleState.sampleShadingEnable)
	{
		spdlog::error("Sample shading requested, but not available.");
		throw std::runtime_error("Sample shading not supported.");
	}

	vk::PipelineMultisampleStateCreateInfo multisamplingInfo(
		{}, 
		state.multisampleState.rasterizationSamples,
		state.multisampleState.sampleShadingEnable,
		state.multisampleState.minSampleShading,
		state.multisampleState.sampleMasks.data(),
		state.multisampleState.alphaToCoverageEnable,
		state.multisampleState.alphaToOneEnable
	);

	vk::GraphicsPipelineCreateInfo pipelineInfo(
		{},
		shaderStages,
		&vertexInputInfo,
		&inputAssemblyInfo,
		&tessellationInfo,
		&viewportStateInfo,
		&rasterizerInfo,
		&multisamplingInfo,
		);
}

void PipelineBuilder::toggle_dynamic_state(bool enable, vk::DynamicState dynamicState)
{
	auto it = std::find(state.dynamicStates.begin(),
		state.dynamicStates.end(),
		dynamicState
	);

	if (enable && it == state.dynamicStates.end())
	{
		state.dynamicStates.push_back(dynamicState);
	}

	else if (!enable && it != state.dynamicStates.end())
	{
		state.dynamicStates.erase(it);
	}
}