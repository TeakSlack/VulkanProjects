#pragma once
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

template<typename Derived>
class VertexFormat
{
	static consteval std::string_view name() {return Derived::name() }
	static consteval vk::VertexInputBindingDescription get_binding_description();
	static consteval vk::VertexInputAttributeDescription get_attribute_description();
};

// Basic vertex structure
struct Vertex2DColor : VertexFormat<Vertex2DColor>
{
	glm::vec2 position;
	glm::vec3 color;

	// Defines how the data is passed to the vertex shader
	static consteval vk::VertexInputBindingDescription get_binding_description()
	{
		// Defines the rate to load data from memory
		vk::VertexInputBindingDescription bindingDescription(
			0,								// Binding: only one--all per-vertex data is in one array
			sizeof(Vertex2DColor),					// Stride: number of bytes from one entry to the next
			vk::VertexInputRate::eVertex	// Input rate: either vertex or instance for instanced rendering
		);

		return bindingDescription;
	}

	// Describes how to retrieve vertex attribute from a chunk of vertex data
	static consteval std::array<vk::VertexInputAttributeDescription, 2> get_attribute_description()
	{
		std::array<vk::VertexInputAttributeDescription, 2> attributeDescriptions{};

		// For position (vec2)
		attributeDescriptions[0].binding = 0;							// Which binding does this come from?
		attributeDescriptions[0].location = 0;							// References (location = 0) in vtex shader
		attributeDescriptions[0].format = vk::Format::eR32G32Sfloat;	// Type of data for attribute, vec2, specified using same enum as colors
		attributeDescriptions[0].offset = offsetof(Vertex2DColor, position);		// Specifies number of bytes since start of per-vertex data

		// For color (vec3)
		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;
		attributeDescriptions[1].offset = offsetof(Vertex2DColor, color);

		return attributeDescriptions;
	}
};