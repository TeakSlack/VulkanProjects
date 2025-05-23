#include <cassert>
#include <fstream>

#include <spdlog/spdlog.h>

#include "pipeline_builder.h"

// Constructor for PipelineBuilder.
// Ensures the provided Vulkan device is valid and stores it for later pipeline/shader creation.
PipelineBuilder::PipelineBuilder(vk::Device device)
	: m_Device(device)
{
	assert(device && "vk::Device has not been initialized!");
}

// Adds a shader stage to the pipeline by loading a SPIR-V shader from file.
// shaderPath: Path to the SPIR-V binary file.
// stage: Enum value indicating the shader stage (e.g., vertex, fragment).
// Returns a reference to this builder for method chaining.
PipelineBuilder& PipelineBuilder::add_shader_stage(const std::string& shaderPath, ShaderStageType stage)
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

std::optional<vk::Pipeline> PipelineBuilder::build()
{
	std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;

	for (const auto& stage : m_ShaderStages)
	{
		vk::PipelineShaderStageCreateInfo stageInfo{};
		stageInfo.stage = static_cast<vk::ShaderStageFlagBits>(stage.type);
		stageInfo.module = stage.module;
		stageInfo.pName = "main";
		shaderStages.push_back(stageInfo);
	}
}