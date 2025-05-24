#pragma once

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>
#include <array>

// VertexFormat CRTP base class
// Provides a static interface for derived vertex types to define their
// Vulkan binding and attribute descriptions.
template<typename Derived>
class VertexFormat
{
    // Returns the name of the vertex format (must be implemented by Derived)
    static consteval std::string_view name() { return Derived::name(); }

    // Returns the Vulkan binding description for this vertex format
    static consteval vk::VertexInputBindingDescription get_binding_description();

    // Returns the Vulkan attribute descriptions for this vertex format
    static consteval vk::VertexInputAttributeDescription get_attribute_description();
};

// Vertex2DColor
// A simple 2D vertex with position and color attributes.
// Inherits from VertexFormat for static interface.
struct Vertex2DColor : VertexFormat<Vertex2DColor>
{
    glm::vec2 position; // 2D position of the vertex
    glm::vec3 color;    // RGB color of the vertex

    // Returns the Vulkan binding description for this vertex type.
    // Describes how the vertex data is laid out in memory and how it is
    // provided to the vertex shader.
    static consteval vk::VertexInputBindingDescription get_binding_description()
    {
        // binding: binding index in the shader (0 = first binding)
        // stride: size of each vertex in bytes
        // inputRate: per-vertex or per-instance data
        vk::VertexInputBindingDescription bindingDescription(
            0,                              // binding
            sizeof(Vertex2DColor),          // stride
            vk::VertexInputRate::eVertex    // inputRate
        );

        return bindingDescription;
    }

    // Returns the Vulkan attribute descriptions for this vertex type.
    // Describes how to extract each attribute from the vertex data.
    static consteval std::array<vk::VertexInputAttributeDescription, 2> get_attribute_description()
    {
        std::array<vk::VertexInputAttributeDescription, 2> attributeDescriptions{};

        // Attribute 0: position (vec2)
        attributeDescriptions[0].binding = 0;                              // binding index
        attributeDescriptions[0].location = 0;                             // location in shader
        attributeDescriptions[0].format = vk::Format::eR32G32Sfloat;       // format: 2x float
        attributeDescriptions[0].offset = offsetof(Vertex2DColor, position); // offset in struct

        // Attribute 1: color (vec3)
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;    // format: 3x float
        attributeDescriptions[1].offset = offsetof(Vertex2DColor, color);

        return attributeDescriptions;
    }
};