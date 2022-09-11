#pragma once

#include <string_view>
#include <tuple>

#include <vulkan/vulkan.h>

namespace Vulkan::Debugging {

auto error_string(VkResult res) -> std::string_view;

auto create_debug_messager(VkInstance instance) -> std::tuple<VkDebugUtilsMessengerEXT, VkResult>;

auto destroy_debug_messager(VkInstance instance, VkDebugUtilsMessengerEXT debug_messager) -> void;

} // namespace Vulkan::Debugging

namespace Vulkan {
using Debugging::error_string;
}