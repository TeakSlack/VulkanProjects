#pragma once

#include <vulkan/vulkan.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace Vulkan
{
	inline auto logger = spdlog::stdout_color_mt("logger");
	inline vk::Instance instance;
	inline vk::DebugUtilsMessengerEXT debugMessenger;
}