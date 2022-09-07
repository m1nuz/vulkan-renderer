#pragma once

#include "Vulkan.hpp"

namespace Storage {
struct Storage;
} // namespace Storage

namespace Renderer {

struct Renderer {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkSurfaceKHR presentation_surface = VK_NULL_HANDLE;
    Vulkan::QueueParameters graphics_queue = {};
    Vulkan::QueueParameters present_queue = {};
    Vulkan::QueueFamilyIndices queue_family_indices;
    Vulkan::SwapChain swapchain;

    VkSemaphore image_available_semaphore = VK_NULL_HANDLE;
    VkSemaphore rendering_finished_semaphore = VK_NULL_HANDLE;
    VkFence in_flight_fence = VK_NULL_HANDLE;

    VkCommandBuffer present_queue_command_buffer = VK_NULL_HANDLE;
    VkCommandPool present_queue_command_pool = VK_NULL_HANDLE;

    VkRenderPass render_pass = VK_NULL_HANDLE;
    Vulkan::Pipeline graphics_pipeline;

    uint32_t framebuffer_width = 0;
    uint32_t framebuffer_height = 0;

    bool debug = false;

    bool is_valid() const {
        return instance != VK_NULL_HANDLE && physical_device != VK_NULL_HANDLE && device != VK_NULL_HANDLE && !swapchain;
    }

    operator bool() const {
        return is_valid();
    }
};

struct CreateRendererInfo {
    std::string_view app_name;
    std::string_view engine_name = "No Engine";
    bool validate = true;
    GLFWwindow* window = nullptr;
};

auto create_renderer(Storage::Storage& storage, const CreateRendererInfo& info) -> std::optional<Renderer>;

auto destroy_renderer(Renderer& renderer) -> void;

auto draw_frame(Renderer& renderer) -> void;

} // namespace Renderer