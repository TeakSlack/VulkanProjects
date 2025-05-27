#ifndef PIPELINE_BUILDER_H
#define PIPELINE_BUILDER_H

#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <type_traits>
#include <array>

#include <vulkan/vulkan.hpp>

#include "vertex.h"

class PipelineBuilder
{
public:
	PipelineBuilder(vk::PhysicalDevice physicalDevice, vk::Device device);
	~PipelineBuilder();

	PipelineBuilder& add_shader_stage(const std::string& shaderPath, vk::ShaderStageFlagBits stage);
	PipelineBuilder& set_primitive_topology(vk::PrimitiveTopology topology);
	PipelineBuilder& set_primitive_restart(bool enable);
	PipelineBuilder& set_dynamic_topology(bool enable);
	PipelineBuilder& set_patch_control_points(uint32_t points);
	PipelineBuilder& add_dynamic_state(vk::DynamicState dynamicState);
	PipelineBuilder& add_viewport(vk::Viewport viewport);
	PipelineBuilder& add_viewport(float x, float y, float width, float height, float minDepth, float maxDepth);
	PipelineBuilder& add_scissor(vk::Rect2D scissor);
	PipelineBuilder& add_scissor(int32_t x, int32_t y, uint32_t width, uint32_t height);
	PipelineBuilder& set_depth_clamp(bool enable);
	PipelineBuilder& set_rasterizer_discard(bool enable);
	PipelineBuilder& set_polygon_mode(vk::PolygonMode mode);
	PipelineBuilder& set_cull_mode(vk::CullModeFlags mode);
	PipelineBuilder& set_front_face(vk::FrontFace face);
	PipelineBuilder& set_depth_bias(bool enable);
	PipelineBuilder& set_line_width(float width);
	PipelineBuilder& set_depth_bias_constant(float constant);
	PipelineBuilder& set_depth_bias_clamp(float clamp);
	PipelineBuilder& set_depth_bias_slope(float slope);
	PipelineBuilder& set_dynamic_line_width(bool enable);
	PipelineBuilder& set_dynamic_depth_bias(bool enable);
	PipelineBuilder& set_sample_count(vk::SampleCountFlagBits count);
	PipelineBuilder& set_sample_shading(bool enable, float minSampleShading);
	PipelineBuilder& add_sample_mask(vk::SampleMask mask);
	PipelineBuilder& set_alpha_to_coverage(bool enable);
	PipelineBuilder& set_alpha_to_one(bool enable);
	PipelineBuilder& set_render_pass(vk::RenderPass renderPass, uint32_t subpassIndex);
	PipelineBuilder& set_depth_test(bool enable);
	PipelineBuilder& set_depth_write(bool enable);
	PipelineBuilder& set_depth_compare_op(vk::CompareOp compareOp);
	PipelineBuilder& set_depth_bounds_test(bool enable);
	PipelineBuilder& set_stencil_test(bool enable);
	PipelineBuilder& set_stencil_front(vk::StencilOpState front);
	PipelineBuilder& set_stencil_back(vk::StencilOpState back);
	PipelineBuilder& set_min_depth_bounds(float minDepthBounds);
	PipelineBuilder& set_max_depth_bounds(float maxDepthBounds);
	
	PipelineBuilder& add_color_blend_attachment(vk::PipelineColorBlendAttachmentState attachment);

	vk::Pipeline build();

	template<typename T>
	PipelineBuilder& set_vertex_format()
	{
		static_assert(std::is_base_of_v<T, VertexFormat>, "Must inherit from VertexFormat CRTP");

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
		} tessellationState;

		std::vector<vk::DynamicState> dynamicStates;

		struct ViewportState
		{
			std::vector<vk::Viewport> viewports;
			std::vector<vk::Rect2D> scissors;
		} viewportState;

		struct RasterizationState
		{
			vk::Bool32 depthClamp = vk::False;
			vk::Bool32 rasterizerDiscard = vk::False;
			vk::PolygonMode polygonMode = vk::PolygonMode::eFill;
			vk::CullModeFlags cullMode = vk::CullModeFlagBits::eBack;
			vk::FrontFace frontFace = vk::FrontFace::eClockwise;
			vk::Bool32 depthBias = vk::False;
			float lineWidth = 1.0f;
			float depthBiasConstant = 0.0f;
			float depthBiasClamp = 0.0f;
			float depthBiasSlope = 0.0f;
		} rasterizationState;

		struct MultisampleState
		{
			vk::SampleCountFlagBits rasterizationSamples = vk::SampleCountFlagBits::e1;
			vk::Bool32 sampleShadingEnable = vk::False;
			float minSampleShading = 1.0f;
			std::vector<vk::SampleMask> sampleMasks;
			vk::Bool32 alphaToCoverageEnable = vk::False;
			vk::Bool32 alphaToOneEnable = vk::False;
		} multisampleState;

		struct DepthStencilState
		{
			vk::Bool32 depthTestEnable = vk::False;
			vk::Bool32 depthWriteEnable = vk::False;
			vk::CompareOp depthCompareOp = vk::CompareOp::eNever;
			vk::Bool32 depthBoundsTestEnable = vk::False;
			vk::Bool32 stencilTestEnable = vk::False;
			vk::StencilOpState front{};
			vk::StencilOpState back{};
			float minDepthBounds = 0.0f;
			float maxDepthBounds = 0.0f;
		} depthStencilState;

		// TODO: write functions to set parameters & add attachment
		// TODO: add helper for attachment states
		struct ColorBlendState
		{
			vk::Bool32 logicOpEnable = false;
			vk::LogicOp logicOp = vk::LogicOp::eNoOp;
			std::vector<vk::PipelineColorBlendAttachmentState> attachments;
			std::array<float, 4> blendConstants;
		} colorBlendState;

		// TODO: write helpers functions
		struct PipelineLayout
		{
			std::vector<vk::DescriptorSetLayout> descriptorSetLayouts;
			std::vector<vk::PushConstantRange> pushConstantRanges;
		} pipelineLayout;
	} state;

	struct ShaderStage
	{
		vk::ShaderModule module;
		vk::ShaderStageFlagBits type;
	};

private:
	vk::PhysicalDevice m_PhysicalDevice;
	vk::PhysicalDeviceProperties m_DeviceProperties;
	vk::PhysicalDeviceFeatures m_DeviceFeatures;
	vk::Device m_Device;
	vk::PipelineLayout m_PipelineLayout;
	vk::RenderPass m_RenderPass;
	uint32_t m_SubpassIndex;

	std::vector<ShaderStage> m_ShaderStages;

	uint32_t m_MaxViewports = 0;

private:
	void toggle_dynamic_state(bool enable, vk::DynamicState dynamicState);
};
#endif