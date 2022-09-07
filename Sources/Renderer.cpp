#include "Renderer.hpp"
#include "Journal.hpp"
#include "Storage.hpp"
#include "Tags.hpp"

#include <GLFW/glfw3.h>

namespace Renderer {

auto create_renderer([[maybe_unused]] Storage::Storage& storage, const CreateRendererInfo& info) -> std::optional<Renderer> {
    if (!info.window) {
        Journal::error(Tags::Renderer, "Couldn't create renderer! Invalid Window");
        return {};
    }

    auto instance = Vulkan::create_vulkan_instance(
        { .app_name = info.app_name, .engine_name = info.engine_name, .validate = info.validate, .window = info.window });
    if (!instance) {
        Journal::error(Tags::Renderer, "Couldn't create renderer! Invalid instance");
        return {};
    }

    VkSurfaceKHR presentation_surface = VK_NULL_HANDLE;
    if (const auto res = glfwCreateWindowSurface(instance, info.window, nullptr, &presentation_surface); res != VK_SUCCESS) {
        Journal::critical(Tags::Renderer, "Could not create presentation surface!");
        return std::nullopt;
    }

    int window_width, window_height;
    glfwGetFramebufferSize(info.window, &window_width, &window_height);

    Vulkan::QueueFamilyIndices selected_queue_family_indices;

    auto selected_physical_device = Vulkan::pick_physical_device(instance, presentation_surface,
        selected_queue_family_indices.graphics_queue_family_index, selected_queue_family_indices.present_queue_family_index);
    if (!selected_physical_device) {
        Journal::critical(Tags::Renderer, "Could not select physical device based on the chosen properties!");
        return std::nullopt;
    }

    auto logical_device = Vulkan::create_logical_device(*selected_physical_device,
        selected_queue_family_indices.graphics_queue_family_index, selected_queue_family_indices.present_queue_family_index);
    if (!logical_device) {
        Journal::critical(Tags::Renderer, "Couldn't create logical device!");
        return std::nullopt;
    }

    Vulkan::QueueParameters graphics_queue;
    Vulkan::QueueParameters present_queue;
    vkGetDeviceQueue(*logical_device, graphics_queue.family_index, 0, &graphics_queue.handle);
    vkGetDeviceQueue(*logical_device, present_queue.family_index, 0, &present_queue.handle);

    auto swapchain = Vulkan::create_swapchain(
        *selected_physical_device, *logical_device, presentation_surface, selected_queue_family_indices, window_width, window_height);
    if (!swapchain) {
        Journal::critical(Tags::Renderer, "Could not create swap chain!");
        return std::nullopt;
    }

    auto [image_available_semaphore, rendering_finished_semaphore, in_flight_fence]
        = Vulkan::create_synchronization_objects(*logical_device);
    if (image_available_semaphore == VK_NULL_HANDLE || rendering_finished_semaphore == VK_NULL_HANDLE
        || in_flight_fence == VK_NULL_HANDLE) {
        Journal::critical(Tags::Renderer, "Couldn't create synchronization objects!");
        return std::nullopt;
    }

    VkCommandPoolCreateInfo command_pool_create_info = {};
    command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    command_pool_create_info.queueFamilyIndex = present_queue.family_index;

    VkCommandPool present_queue_command_pool;
    if (vkCreateCommandPool(*logical_device, &command_pool_create_info, nullptr, &present_queue_command_pool) != VK_SUCCESS) {
        Journal::error(Tags::Vulkan, "Could not create a command pool!");
        return {};
    }

    VkCommandBufferAllocateInfo command_buffer_allocate_info = {};
    command_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_allocate_info.commandPool = present_queue_command_pool;
    command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_buffer_allocate_info.commandBufferCount = 1;

    VkCommandBuffer present_queue_command_buffer;
    if (vkAllocateCommandBuffers(*logical_device, &command_buffer_allocate_info, &present_queue_command_buffer) != VK_SUCCESS) {
        Journal::error(Tags::Vulkan, "Could not record command buffers!");
        return {};
    }

    // Create renderer passes
    VkAttachmentDescription color_attachment_desc = {};
    color_attachment_desc.format = swapchain->image_format.format;
    color_attachment_desc.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment_desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment_desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment_desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment_desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment_desc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref {};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    VkRenderPassCreateInfo render_pass_create_info {};
    render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_create_info.attachmentCount = 1;
    render_pass_create_info.pAttachments = &color_attachment_desc;
    render_pass_create_info.subpassCount = 1;
    render_pass_create_info.pSubpasses = &subpass;
    VkRenderPass render_pass = VK_NULL_HANDLE;
    if (vkCreateRenderPass(*logical_device, &render_pass_create_info, nullptr, &render_pass) != VK_SUCCESS) {
        Journal::error(Tags::Vulkan, "Could not create render pass!");
        return {};
    }

    auto frag_info = Storage::get_shader(storage, 4263377347285878457); // Base.frag.spv
    auto vert_info = Storage::get_shader(storage, 3027544629138209736); // Base.frag.spv

    Vulkan::ShaderInfo shaders[] = { { .shader_type = Vulkan::ShaderType::Vertex, .shader_binary = vert_info->shader_binary },
        { .shader_type = Vulkan::ShaderType::Fragmet, .shader_binary = frag_info->shader_binary } };

    auto graphics_pipeline
        = Vulkan::create_graphics_pipeline({ .device = *logical_device, .render_pass = render_pass, .shaders = shaders });

    Renderer renderer;
    renderer.debug = info.validate;
    renderer.instance = instance;
    renderer.presentation_surface = presentation_surface;
    renderer.physical_device = *selected_physical_device;
    renderer.device = *logical_device;
    renderer.graphics_queue = graphics_queue;
    renderer.present_queue = present_queue;
    renderer.queue_family_indices = selected_queue_family_indices;
    renderer.swapchain = *swapchain;
    renderer.image_available_semaphore = image_available_semaphore;
    renderer.rendering_finished_semaphore = rendering_finished_semaphore;
    renderer.in_flight_fence = in_flight_fence;
    renderer.present_queue_command_pool = present_queue_command_pool;
    renderer.present_queue_command_buffer = present_queue_command_buffer;
    renderer.render_pass = render_pass;
    renderer.graphics_pipeline = graphics_pipeline;

    return { renderer };
}

auto destroy_renderer(Renderer& renderer) -> void {
    Vulkan::destroy_graphics_pipeline(renderer.graphics_pipeline);

    vkDestroyRenderPass(renderer.device, renderer.render_pass, nullptr);

    vkDestroyCommandPool(renderer.device, renderer.present_queue_command_pool, nullptr);

    vkDestroySemaphore(renderer.device, renderer.image_available_semaphore, nullptr);
    vkDestroySemaphore(renderer.device, renderer.rendering_finished_semaphore, nullptr);
    vkDestroyFence(renderer.device, renderer.in_flight_fence, nullptr);

    renderer.graphics_queue = {};
    renderer.present_queue = {};
    renderer.queue_family_indices = {};

    Vulkan::destroy_swapchain(renderer.device, renderer.swapchain);
    Vulkan::destroy_logical_device(renderer.device);

    vkDestroySurfaceKHR(renderer.instance, renderer.presentation_surface, nullptr);

    if (renderer.debug) {
        Vulkan::Debugging::cleanup(renderer.instance);
    }

    Vulkan::destroy_instance(renderer.instance);
}

[[maybe_unused]] static auto begin_frame(Renderer& renderer) -> bool {
    vkWaitForFences(renderer.device, 1, &renderer.in_flight_fence, VK_TRUE, UINT64_MAX);
    vkResetFences(renderer.device, 1, &renderer.in_flight_fence);

    uint32_t image_index = 0;
    auto result = vkAcquireNextImageKHR(
        renderer.device, renderer.swapchain.handle, UINT64_MAX, renderer.image_available_semaphore, VK_NULL_HANDLE, &image_index);
    switch (result) {
    case VK_SUCCESS:
    case VK_SUBOPTIMAL_KHR:
        break;
    case VK_ERROR_OUT_OF_DATE_KHR:
    default:
        Journal::error(Tags::Vulkan, "Problem occurred during swap chain image acquisition!");
        return false;
    }

    vkResetCommandBuffer(renderer.present_queue_command_buffer, 0);
    // record_command_buffer(vk.present_queue_command_buffer, image_index);

    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &renderer.image_available_semaphore;
    submit_info.pWaitDstStageMask = wait_stages;

    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &renderer.present_queue_command_buffer;

    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &renderer.rendering_finished_semaphore;

    if (vkQueueSubmit(renderer.graphics_queue.handle, 1, &submit_info, renderer.in_flight_fence) != VK_SUCCESS) {
        return false;
    }

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &renderer.rendering_finished_semaphore;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &renderer.swapchain.handle;
    present_info.pImageIndices = &image_index;

    result = vkQueuePresentKHR(renderer.present_queue.handle, &present_info);
    switch (result) {
    case VK_SUCCESS:
        break;
    case VK_ERROR_OUT_OF_DATE_KHR:
    case VK_SUBOPTIMAL_KHR:
    default:
        Journal::error(Tags::Vulkan, "Problem occurred during image presentation!");
        return false;
    }

    return true;
}

[[maybe_unused]] static auto end_frame([[maybe_unused]] Renderer& renderer) -> void {
}

auto draw_frame([[maybe_unused]] Renderer& renderer) -> void {
    // begin_frame(renderer);
    // end_frame(renderer);
}

} // namespace Renderer