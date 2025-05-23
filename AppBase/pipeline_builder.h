#ifndef PIPELINE_BUILDER_H
#define PIPELINE_BUILDER_H

#include <string>
#include <vector>
#include <optional>
#include <unordered_map>

#include <vulkan/vulkan.hpp>

enum class ShaderStageType
{
	Vertex = vk::ShaderStageFlagBits::eVertex,
	Fragment = vk::ShaderStageFlagBits::eFragment
};

class PipelineBuilder
{
public:
	PipelineBuilder(vk::Device device);

	PipelineBuilder& add_shader_stage(const std::string& shaderPath, ShaderStageType stage);

	std::optional<vk::Pipeline> build();

	struct State
	{
		struct VertexInput
		{
			std::vector<vk::VertexInputBindingDescription> bindingDescriptions;
			std::vector<vk::VertexInputAttributeDescription> attributeDescriptions;

			VertexInput& add_binding_description(uint32_t binding, uint32_t stride, vk::VertexInputRate rate)
			{
				bindingDescriptions.push_back({ binding, stride, rate });
				return *this;
			}

			VertexInput& add_attribute_description(uint32_t binding, uint32_t location, vk::Format format, uint32_t offset)
			{
				attributeDescriptions.push_back({ binding, location, format, offset });
				return *this;
			}
		} vertexInput;
	};

	struct ShaderStage
	{
		vk::ShaderModule module;
		ShaderStageType type;
	};
	
private:
	vk::Device m_Device;

	std::vector<ShaderStage> m_ShaderStages;
	std::unordered_map<uint32_t, std::vector<uint8_t>> m_SpecializationConstants;
};

#endif