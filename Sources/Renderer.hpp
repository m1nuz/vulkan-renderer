#pragma once

#include "Vulkan.hpp"

namespace Storage {
struct Storage;
} // namespace Storage

namespace Renderer {

constexpr uint32_t MaxFramesInFlight = 3;

struct Renderer {
    Vulkan::Device device;
    Vulkan::SwapChain swapchain;
    Vulkan::Pipeline graphics_pipeline;

    uint32_t framebuffer_width = 0;
    uint32_t framebuffer_height = 0;
    uint32_t image_index = 0;

    bool debug = false;

    bool is_valid() const {
        return device && swapchain;
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

[[nodiscard]] auto create_renderer(Storage::Storage& storage, const CreateRendererInfo& info) -> std::optional<Renderer>;

auto destroy_renderer(Renderer& renderer) -> void;

auto draw_frame(Renderer& renderer) -> void;

} // namespace Renderer