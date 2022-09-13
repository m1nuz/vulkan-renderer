#include "Renderer.hpp"
#include "Journal.hpp"
#include "Storage.hpp"
#include "Tags.hpp"

#include <GLFW/glfw3.h>

namespace Renderer {

auto create_renderer([[maybe_unused]] Storage::Storage& storage, const CreateRendererInfo& info) -> std::optional<Renderer> {
    if (!info.window) {
        Journal::error(Tags::Renderer, "Couldn't create renderer! Invalid Window");
        return std::nullopt;
    }

    auto device = Vulkan::create_device({ .app_name = info.app_name,
        .engine_name = info.engine_name,
        .validate = info.validate,
        .window = info.window,
        .max_frames_in_flight = MaxFramesInFlight });
    if (!device) {
        Journal::error(Tags::Renderer, "Couldn't create renderer device!");
        return std::nullopt;
    }

    int window_width, window_height;
    glfwGetFramebufferSize(info.window, &window_width, &window_height);

    auto swapchain = Vulkan::create_swapchain({ .physical_device = device.physical_device,
        .device = device.device,
        .surface = device.presentation_surface,
        .indices = Vulkan::QueueFamilyIndices { device.graphics_queue.family_index, device.present_queue.family_index },
        .extent = VkExtent2D { static_cast<uint32_t>(window_width), static_cast<uint32_t>(window_height) },
        .frame_in_flights = MaxFramesInFlight });
    if (!swapchain) {
        Journal::critical(Tags::Renderer, "Could not create swap chain!");
        return std::nullopt;
    }

    auto frag_info = Storage::get_shader(storage, 4263377347285878457); // Base.frag.spv
    auto vert_info = Storage::get_shader(storage, 3027544629138209736); // Base.frag.spv

    Vulkan::ShaderInfo shaders[] = { { .shader_type = Vulkan::ShaderType::Vertex, .shader_binary = vert_info->shader_binary },
        { .shader_type = Vulkan::ShaderType::Fragmet, .shader_binary = frag_info->shader_binary } };

    auto graphics_pipeline
        = Vulkan::create_graphics_pipeline({ .device = device.device, .render_pass = swapchain->render_pass, .shaders = shaders });
    if (!graphics_pipeline) {
        Journal::critical(Tags::Renderer, "Couldn't create graphics pipeline!");
        return std::nullopt;
    }

    Renderer renderer;
    renderer.debug = info.validate;
    renderer.device = device;
    renderer.swapchain = *swapchain;
    renderer.graphics_pipeline = graphics_pipeline;

    return { renderer };
}

auto destroy_renderer(Renderer& renderer) -> void {
    vkDeviceWaitIdle(renderer.device.device);

    Vulkan::destroy_graphics_pipeline(renderer.graphics_pipeline);
    Vulkan::destroy_swapchain(renderer.device.device, renderer.swapchain);
    Vulkan::destroy_device(renderer.device);
}

[[maybe_unused]] static auto record_command_buffer(VkCommandBuffer command_buffer, VkRenderPass render_pass, VkFramebuffer framebuffer,
    VkPipeline graphics_pipeline, VkExtent2D extent) -> void {
    VkCommandBufferBeginInfo command_buffer_begin_info = {};
    command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    // command_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

    if (vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info) != VK_SUCCESS) {
        // return;
    }

    VkRenderPassBeginInfo render_pass_begin_info = {};
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.renderPass = render_pass;
    render_pass_begin_info.framebuffer = framebuffer;
    render_pass_begin_info.renderArea.offset = { 0, 0 };
    render_pass_begin_info.renderArea.extent = extent;

    VkClearValue clear_color = { { { 0.0f, 0.0f, 0.0f, 1.0f } } };
    render_pass_begin_info.clearValueCount = 1;
    render_pass_begin_info.pClearValues = &clear_color;

    vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);

    VkViewport viewport {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)extent.width;
    viewport.height = (float)extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D scissor {};
    scissor.offset = { 0, 0 };
    scissor.extent = extent;
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    vkCmdDraw(command_buffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(command_buffer);

    if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
        // return;
    }
}

[[maybe_unused]] static auto begin_frame(Renderer& renderer) -> bool {
    auto& swapchain = renderer.swapchain;

    vkWaitForFences(renderer.device.device, 1, &swapchain.in_flight_fences[swapchain.current_frame], VK_TRUE, UINT64_MAX);
    vkResetFences(renderer.device.device, 1, &swapchain.in_flight_fences[swapchain.current_frame]);

    uint32_t image_index = 0;
    auto result = vkAcquireNextImageKHR(renderer.device.device, swapchain.handle, UINT64_MAX,
        swapchain.image_available_semaphores[swapchain.current_frame], VK_NULL_HANDLE, &image_index);
    switch (result) {
    case VK_SUCCESS:
    case VK_SUBOPTIMAL_KHR:
        break;
    case VK_ERROR_OUT_OF_DATE_KHR:
    default:
        Journal::error(Tags::Vulkan, "Problem occurred during swap chain image acquisition!");
        // return false;
    }

    renderer.image_index = image_index;

    vkResetCommandBuffer(renderer.device.present_queue_command_buffers[swapchain.current_frame], 0);

    return true;
}

[[maybe_unused]] static auto end_frame([[maybe_unused]] Renderer& renderer) -> bool {
    auto& swapchain = renderer.swapchain;

    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &swapchain.image_available_semaphores[swapchain.current_frame];
    submit_info.pWaitDstStageMask = wait_stages;

    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &renderer.device.present_queue_command_buffers[swapchain.current_frame];

    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &swapchain.render_finished_semaphores[swapchain.current_frame];

    if (vkQueueSubmit(renderer.device.graphics_queue.handle, 1, &submit_info, swapchain.in_flight_fences[swapchain.current_frame])
        != VK_SUCCESS) {
        //    return false;
    }

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &swapchain.render_finished_semaphores[swapchain.current_frame];
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &swapchain.handle;
    present_info.pImageIndices = &renderer.image_index;

    auto result = vkQueuePresentKHR(renderer.device.present_queue.handle, &present_info);
    switch (result) {
    case VK_SUCCESS:
        break;
    case VK_ERROR_OUT_OF_DATE_KHR:
    case VK_SUBOPTIMAL_KHR:
    default:
        Journal::error(Tags::Vulkan, "Problem occurred during image presentation!");
        // return false;
    }

    renderer.swapchain.current_frame++;
    renderer.swapchain.current_frame %= swapchain.max_frames_in_flight;

    return true;
}

auto draw_frame([[maybe_unused]] Renderer& renderer) -> void {
    begin_frame(renderer);
    record_command_buffer(renderer.device.present_queue_command_buffers[renderer.swapchain.current_frame], renderer.swapchain.render_pass,
        renderer.swapchain.framebuffes[renderer.swapchain.current_frame], renderer.graphics_pipeline.pipeline, renderer.swapchain.extent);
    end_frame(renderer);
}

} // namespace Renderer