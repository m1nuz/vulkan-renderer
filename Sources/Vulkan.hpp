#pragma once

#include <vulkan/vulkan.h>

#include <optional>
#include <span>
#include <string>
#include <tuple>
#include <vector>

typedef struct GLFWwindow GLFWwindow;

namespace Vulkan {

struct QueueParameters {
    VkQueue handle = VK_NULL_HANDLE;
    uint32_t family_index = 0;
};

struct QueueFamilyIndices {
    uint32_t graphics_queue_family_index = UINT32_MAX;
    uint32_t present_queue_family_index = UINT32_MAX;
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities = {};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};

struct SwapChain {
    VkSurfaceFormatKHR image_format = {};
    uint32_t max_frames_in_flight = 0;
    VkSwapchainKHR handle = VK_NULL_HANDLE;
    VkRenderPass render_pass = VK_NULL_HANDLE;
    VkExtent2D extent = {};
    size_t current_frame = 0;

    std::vector<VkImage> images;
    std::vector<VkImageView> views;
    std::vector<VkFramebuffer> framebuffes;
    std::vector<VkSemaphore> image_available_semaphores;
    std::vector<VkSemaphore> render_finished_semaphores;
    std::vector<VkFence> in_flight_fences;

    bool is_valid() const noexcept {
        return handle != VK_NULL_HANDLE;
    }

    operator bool() const noexcept {
        return is_valid();
    }
};

enum class ShaderType {
    Unknown,
    Vertex, // Vertex Shader
    Fragmet, // Fragment Shader
    Geometry, // Geometry Shader
};

struct ShaderInfo {
    ShaderType shader_type = ShaderType::Unknown;
    std::span<const uint8_t> shader_binary;
};

struct CreateVulkanInstanceInfo {
    std::string_view app_name;
    std::string_view engine_name = "No Engine";
    bool validate = true;
    GLFWwindow* window = nullptr;
};

struct CreateDeviceInfo {
    std::string_view app_name;
    std::string_view engine_name = "No Engine";
    bool validate = true;
    GLFWwindow* window = nullptr;
    uint32_t max_frames_in_flight = 0;
};

struct CreatePipelineInfo {
    VkDevice device = VK_NULL_HANDLE;
    VkRenderPass render_pass = VK_NULL_HANDLE;
    std::span<ShaderInfo> shaders;
};

struct Device {
    VkInstance instance = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkSurfaceKHR presentation_surface = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;

    QueueParameters graphics_queue = {};
    QueueParameters present_queue = {};

    std::vector<VkCommandBuffer> present_queue_command_buffers;
    VkCommandPool present_queue_command_pool = VK_NULL_HANDLE;

    bool is_valid() const noexcept {
        return instance != VK_NULL_HANDLE && device != VK_NULL_HANDLE && presentation_surface != VK_NULL_HANDLE;
    }

    operator bool() const noexcept {
        return is_valid();
    }
};

struct Pipeline {
    VkDevice device = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;

    bool is_valid() const noexcept {
        return device != VK_NULL_HANDLE && layout != VK_NULL_HANDLE && pipeline != VK_NULL_HANDLE;
    }

    operator bool() const noexcept {
        return is_valid();
    }
};

[[nodiscard]] auto create_device(const CreateDeviceInfo& info) -> Device;
auto destroy_device(Device& device) -> void;

[[nodiscard]] auto create_vulkan_instance(const CreateVulkanInstanceInfo& info) -> VkInstance;
auto destroy_instance(VkInstance instance) noexcept -> void;

[[nodiscard]] auto pick_physical_device(const VkInstance instance, VkSurfaceKHR presentation_surface,
    uint32_t& selected_graphics_queue_family_index, uint32_t& selected_present_queue_family_index) -> std::optional<VkPhysicalDevice>;
[[nodiscard]] auto create_logical_device(VkPhysicalDevice physical_device, const uint32_t selected_graphics_queue_family_index,
    const uint32_t selected_present_queue_family_index) -> std::optional<VkDevice>;
auto destroy_logical_device(VkDevice device) noexcept -> void;

[[nodiscard]] auto create_swapchain(VkPhysicalDevice physical_device, VkDevice device, VkSurfaceKHR surface,
    const QueueFamilyIndices& indices, int32_t width, int32_t height, size_t frame_in_flights) -> std::optional<SwapChain>;
auto destroy_swapchain(VkDevice device, SwapChain& swapchain) -> void;

[[nodiscard]] auto create_shader(VkDevice device, std::span<const uint8_t> shader_binary) -> VkShaderModule;
auto destroy_shader(VkDevice device, VkShaderModule shader_module) noexcept -> void;

[[nodiscard]] auto create_graphics_pipeline(const CreatePipelineInfo& info) -> Pipeline;
auto destroy_graphics_pipeline(Pipeline& pipeline) -> void;

[[nodiscard]] auto create_render_pass() -> VkRenderPass;
auto destroy_render_pass(VkRenderPass render_pass) -> void;

} // namespace Vulkan