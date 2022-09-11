#include "VulkanDebug.hpp"
#include "Journal.hpp"
#include "Tags.hpp"

namespace Vulkan::Debugging {

auto error_string(VkResult res) -> std::string_view {
    switch (res) {
    case VK_SUCCESS:
        return "Success";
    case VK_NOT_READY:
        return "Not ready";
    case VK_TIMEOUT:
        return "Timeout";
    case VK_EVENT_SET:
        return "Event set";
    case VK_EVENT_RESET:
        return "Event reset";
    case VK_INCOMPLETE:
        return "Incomplete";
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        return "Error out of host memory";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        return "Error out of device memory";
    case VK_ERROR_INITIALIZATION_FAILED:
        return "Error initialization failed";
    case VK_ERROR_DEVICE_LOST:
        return "Error device lost";
    case VK_ERROR_MEMORY_MAP_FAILED:
        return "Error memory map failed";
    case VK_ERROR_LAYER_NOT_PRESENT:
        return "Error layer not present";
    case VK_ERROR_EXTENSION_NOT_PRESENT:
        return "Error extension not present";
    case VK_ERROR_FEATURE_NOT_PRESENT:
        return "Error feature not present";
    case VK_ERROR_INCOMPATIBLE_DRIVER:
        return "Error incompatible driver";
    case VK_ERROR_TOO_MANY_OBJECTS:
        return "Error too many objects";
    case VK_ERROR_FORMAT_NOT_SUPPORTED:
        return "Error format not supported";
    case VK_ERROR_FRAGMENTED_POOL:
        return "Error fragmented pool";
    case VK_ERROR_UNKNOWN:
        return "Error unknown";
    case VK_ERROR_OUT_OF_POOL_MEMORY:
        return "Error out of pool memory";
    case VK_ERROR_INVALID_EXTERNAL_HANDLE:
        return "Error invalid external handle";
    case VK_ERROR_FRAGMENTATION:
        return "Error fragmentation";
    case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
        return "Error invalid opaque capture address";
    case VK_ERROR_SURFACE_LOST_KHR:
        return "Error surface lost";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
        return "Error native window in use";
    case VK_SUBOPTIMAL_KHR:
        return "Suboptimal";
    case VK_ERROR_OUT_OF_DATE_KHR:
        return "Error out of date";
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
        return "Error incompatible display";
    case VK_ERROR_VALIDATION_FAILED_EXT:
        return "Error validation failed";
    case VK_ERROR_INVALID_SHADER_NV:
        return "Error invalid shader NV";
    case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT:
        return "Error invalid DRM format modifier plane layout";
    case VK_ERROR_NOT_PERMITTED_EXT:
        return "Error not permitted";
    case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
        return "Error full screen exclusive mode lost";
    default:
        break;
    }

    return "";
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    [[maybe_unused]] void* pUserData) {

    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
        // Journal::verbose(Tags::Vulkan, "{}", pCallbackData->pMessage);
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        Journal::message(Tags::Vulkan, "{}", pCallbackData->pMessage);
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        Journal::warning(Tags::Vulkan, "{}", pCallbackData->pMessage);
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        Journal::error(Tags::Vulkan, "{}", pCallbackData->pMessage);
    }

    return VK_FALSE;
}

static auto CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pCallback) noexcept {
    auto vkCreateDebugUtilsMessenger_fp
        = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (vkCreateDebugUtilsMessenger_fp)
        return vkCreateDebugUtilsMessenger_fp(instance, pCreateInfo, pAllocator, pCallback);

    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

static void DestroyDebugUtilsMessengerEXT(
    VkInstance instance, VkDebugUtilsMessengerEXT callback, const VkAllocationCallbacks* pAllocator) noexcept {
    auto vkDestroyDebugUtilsMessenger_fp
        = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (vkDestroyDebugUtilsMessenger_fp) {
        vkDestroyDebugUtilsMessenger_fp(instance, callback, pAllocator);
    }
}

auto create_debug_messager(VkInstance instance) -> std::tuple<VkDebugUtilsMessengerEXT, VkResult> {
    VkDebugUtilsMessengerCreateInfoEXT create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    create_info.pfnUserCallback = debugCallback;
    create_info.pUserData = nullptr;

    VkDebugUtilsMessengerEXT debug_utils_messenger;
    if (const auto res = CreateDebugUtilsMessengerEXT(instance, &create_info, nullptr, &debug_utils_messenger); res != VK_SUCCESS) {
        Journal::error(Tags::Vulkan, "{}", error_string(res));
        return { nullptr, res };
    }

    return { debug_utils_messenger, VK_SUCCESS };
}

auto destroy_debug_messager(VkInstance instance, VkDebugUtilsMessengerEXT debug_messager) -> void {
    DestroyDebugUtilsMessengerEXT(instance, debug_messager, nullptr);
}

} // namespace Vulkan::Debugging