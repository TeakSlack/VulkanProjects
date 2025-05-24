#ifndef PIPELINE_BUILDER_H
#define PIPELINE_BUILDER_H

#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <type_traits>

#include <vulkan/vulkan.hpp>

#include "vertex.h"

enum class PrimitiveTopology
{
	TriangleFan = vk::PrimitiveTopology::eTriangleFan,
	TriangleList = vk::PrimitiveTopology::eTriangleList,
	TriangleStrip = vk::PrimitiveTopology::eTriangleStrip,
	LineList = vk::PrimitiveTopology::eLineList,
	LineStrip = vk::PrimitiveTopology::eLineStrip,
	PointList = vk::PrimitiveTopology::ePointList,
};

class PipelineBuilder
{
public:
	PipelineBuilder(vk::Device device);

	PipelineBuilder& add_shader_stage(const std::string& shaderPath, vk::ShaderStageFlagBits stage);
	PipelineBuilder& set_primitive_topology(vk::PrimitiveTopology topology);
	PipelineBuilder& set_primitive_restart(bool enable);
	PipelineBuilder& set_dynamic_topology(bool enable);
	PipelineBuilder& set_patch_control_points(uint32_t points);
	PipelineBuilder& add_dynamic_state(vk::DynamicState dynamicState);

	std::optional<vk::Pipeline> build();

	template<typename T>
	PipelineBuilder& set_vertex_format()
	{
		static_assert(std::is_base_of_v<T<VertexFormat>, VertexFormat>, "Must inherit from VertexFormat CRTP");

		state.vertexInput.add_binding_descriptions(T::get_binding_description());
		state.vertexInput.add_attribute_description(T::get_attribute_description());
	}

	struct State
	{
		struct VertexInput
		{
			std::vector<vk::VertexInputBindingDescription> bindingDescriptions;
			std::vector<vk::VertexInputAttributeDescription> attributeDescriptions;

			VertexInput& add_binding_descriptions(uint32_t binding, uint32_t stride, vk::VertexInputRate rate)
			{
				bindingDescriptions.push_back({ binding, stride, rate });
				return *this;
			}

			VertexInput& add_binding_descriptions(vk::VertexInputBindingDescription description)
			{
				bindingDescriptions.push_back(description);
				return *this;
			}

			VertexInput& add_attribute_description(uint32_t binding, uint32_t location, vk::Format format, uint32_t offset)
			{
				attributeDescriptions.push_back({ binding, location, format, offset });
				return *this;
			}

			VertexInput& add_attribute_description(vk::VertexInputAttributeDescription description)
			{
				attributeDescriptions.push_back(description);
				return *this;
			}
		} vertexInput;

		struct InputAssembly
		{
			vk::PrimitiveTopology topology = vk::PrimitiveTopology::eTriangleList;
			vk::Bool32 primitiveRestartEnable = vk::False;
		} inputAssembly;

		struct TesselationState
		{
			uint32_t patchControlPoints = 0;
		} tesselationState;

		std::vector<vk::DynamicState> dynamicStates;
	} state;

	struct ShaderStage
	{
		vk::ShaderModule module;
		vk::ShaderStageFlagBits type;
	};

private:
	vk::Device m_Device;

	std::vector<ShaderStage> m_ShaderStages;
};
#endif