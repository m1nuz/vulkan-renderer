#include "Vulkan.hpp"
#include "Journal.hpp"
#include "Tags.hpp"
#include "VulkanDebug.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>

namespace Vulkan {

namespace Debugging {
#ifdef LUNAR_VALIDATION
    const std::vector<const char*> validation_layers = { "VK_LAYER_LUNARG_standard_validation" };
#else
    const std::vector<const char*> validation_layers = { "VK_LAYER_KHRONOS_validation" };
#endif
} // namespace Debugging

[[nodiscard]] static auto check_validation_layer_support(const std::vector<const char*>& validation_layers) {
    uint32_t layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

    std::vector<VkLayerProperties> available_layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, std::data(available_layers));

    for (const char* layer_name : validation_layers) {
        bool layer_found = false;

        for (const auto& layerProperties : available_layers) {
            if (strcmp(layer_name, layerProperties.layerName) == 0) {
                layer_found = true;
                break;
            }
        }

        if (!layer_found) {
            return false;
        }
    }

    return true;
}

[[nodiscard]] static auto required_extensions(bool validation_layers_enabled) -> std::vector<const char*> {
    uint32_t glfw_extension_count = 0;
    const char** glfw_extensions;
    glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    std::vector<const char*> extensions(glfw_extensions, glfw_extensions + glfw_extension_count);

    if (validation_layers_enabled) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

[[nodiscard]] auto create_device(const CreateDeviceInfo& info) -> Device {
    Device device;

    auto instance = Vulkan::create_vulkan_instance(
        { .app_name = info.app_name, .engine_name = info.engine_name, .validate = info.validate, .window = info.window });
    if (!instance) {
        Journal::error(Tags::Vulkan, "Couldn't create device! Invalid instance");
        return {};
    }

    if (info.validate) {
        auto [debug_messager, res] = Debugging::create_debug_messager(instance);
        if (debug_messager) {
            device.debug_messenger = debug_messager;
        }
    }

    VkSurfaceKHR presentation_surface = VK_NULL_HANDLE;
    if (const auto res = glfwCreateWindowSurface(instance, info.window, nullptr, &presentation_surface); res != VK_SUCCESS) {
        Journal::critical(Tags::Vulkan, "Could not create presentation surface!");
        return {};
    }

    int window_width, window_height;
    glfwGetFramebufferSize(info.window, &window_width, &window_height);

    QueueFamilyIndices selected_queue_family_indices;
    auto selected_physical_device = Vulkan::pick_physical_device(instance, presentation_surface,
        selected_queue_family_indices.graphics_queue_family_index, selected_queue_family_indices.present_queue_family_index);
    if (!selected_physical_device) {
        Journal::critical(Tags::Vulkan, "Could not select physical device based on the chosen properties!");
        return {};
    }

    auto logical_device = Vulkan::create_logical_device(*selected_physical_device,
        selected_queue_family_indices.graphics_queue_family_index, selected_queue_family_indices.present_queue_family_index);
    if (!logical_device) {
        Journal::critical(Tags::Vulkan, "Couldn't create logical device!");
        return {};
    }

    QueueParameters graphics_queue;
    QueueParameters present_queue;
    graphics_queue.family_index = selected_queue_family_indices.graphics_queue_family_index;
    present_queue.family_index = selected_queue_family_indices.present_queue_family_index;
    vkGetDeviceQueue(*logical_device, graphics_queue.family_index, 0, &graphics_queue.handle);
    vkGetDeviceQueue(*logical_device, present_queue.family_index, 0, &present_queue.handle);

    VkCommandPoolCreateInfo command_pool_create_info = {};
    command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    command_pool_create_info.queueFamilyIndex = present_queue.family_index;

    VkCommandPool present_queue_command_pool;
    if (vkCreateCommandPool(*logical_device, &command_pool_create_info, nullptr, &present_queue_command_pool) != VK_SUCCESS) {
        Journal::error(Tags::Vulkan, "Could not create a command pool!");
        return {};
    }

    std::vector<VkCommandBuffer> present_queue_command_buffers;
    present_queue_command_buffers.resize(info.max_frames_in_flight);

    for (size_t i = 0; i < info.max_frames_in_flight; i++) {
        VkCommandBufferAllocateInfo command_buffer_allocate_info = {};
        command_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        command_buffer_allocate_info.commandPool = present_queue_command_pool;
        command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        command_buffer_allocate_info.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(*logical_device, &command_buffer_allocate_info, &present_queue_command_buffers[i]) != VK_SUCCESS) {
            Journal::error(Tags::Vulkan, "Could not record command buffers!");
            return {};
        }
    }

    device.instance = instance;
    device.physical_device = *selected_physical_device;
    device.device = *logical_device;
    device.presentation_surface = presentation_surface;
    device.graphics_queue = graphics_queue;
    device.present_queue = present_queue;
    device.present_queue_command_pool = present_queue_command_pool;
    device.present_queue_command_buffers = present_queue_command_buffers;

    return device;
}

auto destroy_device(Device& device) -> void {
    vkDestroyCommandPool(device.device, device.present_queue_command_pool, nullptr);

    device.graphics_queue = {};
    device.present_queue = {};

    destroy_logical_device(device.device);

    vkDestroySurfaceKHR(device.instance, device.presentation_surface, nullptr);

    if (device.debug_messenger) {
        Debugging::destroy_debug_messager(device.instance, device.debug_messenger);
    }

    destroy_instance(device.instance);
}

auto create_vulkan_instance(const CreateVulkanInstanceInfo& info) -> VkInstance {
    if (info.validate && !check_validation_layer_support(Debugging::validation_layers)) {
        Journal::critical(Tags::Vulkan, "Validation layers not supported!");
        return {};
    }

    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = std::data(info.app_name);
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = std::data(info.engine_name);
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_2;

    const auto extensions = required_extensions(info.validate);

    VkInstanceCreateInfo create_instance_info = {};
    create_instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_instance_info.pApplicationInfo = &app_info;
    create_instance_info.enabledExtensionCount = std::size(extensions);
    create_instance_info.ppEnabledExtensionNames = std::data(extensions);
    if (info.validate) {
        create_instance_info.enabledLayerCount = static_cast<uint32_t>(std::size(Debugging::validation_layers));
        create_instance_info.ppEnabledLayerNames = std::data(Debugging::validation_layers);
    } else {
        create_instance_info.enabledLayerCount = 0;
        create_instance_info.pNext = nullptr;
    }

    VkInstance instance;
    if (const auto res = vkCreateInstance(&create_instance_info, nullptr, &instance); res != VK_SUCCESS) {
        Journal::error(Tags::Vulkan, "{}", error_string(res));
        return nullptr;
    }

    uint32_t supported_extension_count = 0;
    if (const auto res = vkEnumerateInstanceExtensionProperties(nullptr, &supported_extension_count, nullptr); res != VK_SUCCESS) {
        Journal::error(Tags::Vulkan, "{}", error_string(res));
        return {};
    }

    std::vector<VkExtensionProperties> supported_extensions;
    supported_extensions.resize(supported_extension_count);
    if (const auto res = vkEnumerateInstanceExtensionProperties(nullptr, &supported_extension_count, std::data(supported_extensions));
        res != VK_SUCCESS) {
        Journal::error(Tags::Vulkan, "{}", error_string(res));
        return {};
    }

    // for (const auto& ext : supported_extensions) {
    //     Journal::verbose(Tags::Vulkan, "{} : {}", ext.extensionName, ext.specVersion);
    // }

    return instance;
}

auto destroy_instance(VkInstance instance) noexcept -> void {
    vkDestroyInstance(instance, nullptr);
}

[[nodiscard]] static auto enumerate_physical_devices(const VkInstance instance) -> std::vector<VkPhysicalDevice> {
    uint32_t gpu_count = 0;
    if (const auto res = vkEnumeratePhysicalDevices(instance, &gpu_count, nullptr); res != VK_SUCCESS) {
        Journal::error(Tags::Vulkan, "{}", error_string(res));
        return {};
    }

    std::vector<VkPhysicalDevice> devices;
    devices.resize(gpu_count);

    if (const auto res = vkEnumeratePhysicalDevices(instance, &gpu_count, std::data(devices)); res != VK_SUCCESS) {
        Journal::error(Tags::Vulkan, "{}", error_string(res));
        return {};
    }

    return devices;
}

[[nodiscard]] static auto is_device_suitable(VkPhysicalDevice physical_device, VkSurfaceKHR presentation_surface,
    uint32_t& selected_graphics_queue_family_index, uint32_t& selected_present_queue_family_index) {

    VkPhysicalDeviceProperties device_properties;
    VkPhysicalDeviceFeatures device_features;
    VkPhysicalDeviceMemoryProperties device_memory_properties;

    vkGetPhysicalDeviceProperties(physical_device, &device_properties);
    vkGetPhysicalDeviceFeatures(physical_device, &device_features);
    vkGetPhysicalDeviceMemoryProperties(physical_device, &device_memory_properties);

    const uint32_t api_major_version = VK_VERSION_MAJOR(device_properties.apiVersion);
    const uint32_t api_minor_version = VK_VERSION_MINOR(device_properties.apiVersion);
    const uint32_t api_patch_version = VK_VERSION_PATCH(device_properties.apiVersion);

    const uint32_t driver_major_version = VK_VERSION_MAJOR(device_properties.driverVersion);
    const uint32_t driver_minor_version = VK_VERSION_MINOR(device_properties.driverVersion);
    const uint32_t driver_patch_version = VK_VERSION_PATCH(device_properties.driverVersion);

    if ((api_major_version < 1) || (device_properties.limits.maxImageDimension2D < 4096)) {
        Journal::error(Tags::Vulkan, "Physical device {}:{} doesn't support required parameters!", device_properties.deviceID,
            device_properties.deviceName);
        return false;
    }

    if (device_properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU || !device_features.geometryShader) {
        Journal::error(Tags::Vulkan, "Physical device {}:{} doesn't support required parameters!", device_properties.deviceID,
            device_properties.deviceName);
        return false;
    }

    uint32_t queue_families_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_families_count, nullptr);
    if (queue_families_count == 0) {
        Journal::error(Tags::Vulkan, "Physical device {}:{} doesn't have any queue families! ", device_properties.deviceID,
            device_properties.deviceName);
        return false;
    }

    std::vector<VkQueueFamilyProperties> queue_family_properties;
    queue_family_properties.resize(queue_families_count);
    std::vector<VkBool32> queue_present_support;
    queue_present_support.resize(queue_families_count);

    auto graphics_queue_family_index = UINT32_MAX;
    auto present_queue_family_index = UINT32_MAX;

    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_families_count, std::data(queue_family_properties));

    auto idx = 0;
    for (const auto& property : queue_family_properties) {
        vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, idx, presentation_surface, &queue_present_support[idx]);

        // Select first queue that supports graphics
        if (property.queueCount > 0 && property.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            if (graphics_queue_family_index == UINT32_MAX) {
                graphics_queue_family_index = idx;
            }
        }

        // If there is queue that supports both graphics and present - prefer it
        if (queue_present_support[idx]) {
            Journal::message(Tags::Vulkan, "'{}' API: {}.{}.{} Driver: {}.{}.{}", device_properties.deviceName, api_major_version,
                api_minor_version, api_patch_version, driver_major_version, driver_minor_version, driver_patch_version);

            selected_graphics_queue_family_index = selected_present_queue_family_index = idx;
            return true;
        }

        idx++;
    }

    // We don't have queue that supports both graphics and present so we have to use separate queues
    for (uint32_t i = 0; i < queue_families_count; ++i) {
        if (queue_present_support[i]) {
            present_queue_family_index = i;
            break;
        }
    }

    if ((graphics_queue_family_index == UINT32_MAX) || (present_queue_family_index == UINT32_MAX)) {
        Journal::error(Tags::Vulkan, "Could not find queue families with required properties on physical device {}:{}! ",
            device_properties.deviceID, device_properties.deviceName);
        return false;
    }

    selected_graphics_queue_family_index = graphics_queue_family_index;
    selected_present_queue_family_index = present_queue_family_index;

    Journal::verbose(
        Tags::Vulkan, "{} API: {}.{}.{}", device_properties.deviceName, api_major_version, api_minor_version, api_patch_version);

    return true;
}

[[nodiscard]] auto pick_physical_device(const VkInstance instance, VkSurfaceKHR presentation_surface,
    uint32_t& selected_graphics_queue_family_index, uint32_t& selected_present_queue_family_index) -> std::optional<VkPhysicalDevice> {

    auto physical_devices = enumerate_physical_devices(instance);
    if (physical_devices.empty()) {
        return {};
    }

    auto selected_device_index = 0u;
    for (const auto& device : physical_devices) {
        if (is_device_suitable(device, presentation_surface, selected_graphics_queue_family_index, selected_present_queue_family_index))
            break;

        selected_device_index++;
    }

    if (selected_device_index >= physical_devices.size()) {
        Journal::error(Tags::Vulkan, "Invalid device index");
        return {};
    }

    const auto physical_device = physical_devices[selected_device_index];

    return { physical_device };
}

[[nodiscard]] auto create_logical_device(VkPhysicalDevice physical_device, const uint32_t selected_graphics_queue_family_index,
    const uint32_t selected_present_queue_family_index) -> std::optional<VkDevice> {
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    const float queue_priorities[] = { 1.0f };

    if (selected_graphics_queue_family_index != UINT32_MAX) {
        VkDeviceQueueCreateInfo queue_create_info = {};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = selected_graphics_queue_family_index;
        queue_create_info.queueCount = std::size(queue_priorities);
        queue_create_info.pQueuePriorities = std::data(queue_priorities);

        queue_create_infos.push_back(queue_create_info);
    }

    if (selected_graphics_queue_family_index != selected_present_queue_family_index) {
        VkDeviceQueueCreateInfo queue_create_info = {};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = selected_present_queue_family_index;
        queue_create_info.queueCount = std::size(queue_priorities);
        queue_create_info.pQueuePriorities = std::data(queue_priorities);

        queue_create_infos.push_back(queue_create_info);
    }

    const std::vector<const char*> extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceCreateInfo device_create_info = {};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.pQueueCreateInfos = std::data(queue_create_infos);
    device_create_info.queueCreateInfoCount = std::size(queue_create_infos);
    device_create_info.ppEnabledExtensionNames = std::data(extensions);
    device_create_info.enabledExtensionCount = std::size(extensions);

    VkDevice device;
    if (vkCreateDevice(physical_device, &device_create_info, nullptr, &device) != VK_SUCCESS) {
        Journal::error(Tags::Vulkan, "Could not create vulkan device!");
        return {};
    }

    return { device };
}

auto destroy_logical_device(VkDevice device) noexcept -> void {
    if (const auto res = vkDeviceWaitIdle(device); res != VK_SUCCESS) {
        Journal::error(Tags::Vulkan, "Couldn't wait device {}", error_string(res));
    }

    vkDestroyDevice(device, nullptr);
}

static auto query_swapchain_support(VkPhysicalDevice device, VkSurfaceKHR surface) -> std::optional<SwapChainSupportDetails> {

    VkSurfaceCapabilitiesKHR surface_capabilities;
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &surface_capabilities) != VK_SUCCESS) {
        Journal::error(Tags::Vulkan, "Could not check presentation surface capabilities!");
        return {};
    }

    uint32_t formats_count = 0;
    if ((vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formats_count, nullptr) != VK_SUCCESS) || (formats_count == 0)) {
        Journal::error(Tags::Vulkan, "Error occurred during presentation surface formats enumeration!");
        return {};
    }

    std::vector<VkSurfaceFormatKHR> surface_formats { formats_count };
    if (vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formats_count, &surface_formats[0]) != VK_SUCCESS) {
        Journal::error(Tags::Vulkan, "Error occurred during presentation surface formats enumeration!");
        return {};
    }

    uint32_t present_modes_count = 0;
    if ((vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_modes_count, nullptr) != VK_SUCCESS)
        || (present_modes_count == 0)) {
        Journal::error(Tags::Vulkan, "Error occurred during presentation surface present modes enumeration!");
        return {};
    }

    std::vector<VkPresentModeKHR> present_modes { present_modes_count };
    if (vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_modes_count, &present_modes[0]) != VK_SUCCESS) {
        Journal::error(Tags::Vulkan, "Error occurred during presentation surface present modes enumeration!");
        return {};
    }

    SwapChainSupportDetails details;
    details.capabilities = surface_capabilities;
    details.formats = surface_formats;
    details.present_modes = present_modes;

    return { details };
}

static auto choose_swap_surface_format(std::span<VkSurfaceFormatKHR> available_formats) -> VkSurfaceFormatKHR {
    for (const auto& available : available_formats) {
        if (available.format == VK_FORMAT_B8G8R8A8_SRGB && available.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return available;
        }
    }

    return available_formats[0];
}

static auto choose_swap_present_mode([[maybe_unused]] std::span<VkPresentModeKHR> available_present_modes) -> VkPresentModeKHR {
    for (const auto& available : available_present_modes) {
        if (available == VK_PRESENT_MODE_MAILBOX_KHR) {
            return available;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

static auto choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height) -> VkExtent2D {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    VkExtent2D actual_extent;
    actual_extent.width = std::clamp(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actual_extent.height = std::clamp(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    return actual_extent;
}

static auto get_swap_chain_num_images(const VkSurfaceCapabilitiesKHR& capabilities) -> uint32_t {
    uint32_t image_count = capabilities.minImageCount + 1;
    if ((capabilities.maxImageCount > 0) && (image_count > capabilities.maxImageCount)) {
        image_count = capabilities.maxImageCount;
    }

    return image_count;
}

static auto get_swap_chain_transform(const VkSurfaceCapabilitiesKHR& capabilities) -> VkSurfaceTransformFlagBitsKHR {
    // if (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
    //     return VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    // }

    return capabilities.currentTransform;
}

static auto get_swap_chain_usage_flags([[maybe_unused]] const VkSurfaceCapabilitiesKHR& capabilities) -> VkImageUsageFlags {
    // if (capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
    //     return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    // }

    // return static_cast<VkImageUsageFlags>(-1);
    return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
}

static auto create_image_views(VkDevice device, std::span<VkImage> images, VkFormat image_format) -> std::vector<VkImageView> {
    std::vector<VkImageView> image_views;
    image_views.resize(std::size(images));

    for (size_t i = 0; i < std::size(images); i++) {
        VkImageViewCreateInfo image_view_create_info = {};
        image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image_view_create_info.image = images[i];
        image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        image_view_create_info.format = image_format;
        image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_view_create_info.subresourceRange.baseMipLevel = 0;
        image_view_create_info.subresourceRange.levelCount = 1;
        image_view_create_info.subresourceRange.baseArrayLayer = 0;
        image_view_create_info.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &image_view_create_info, nullptr, &image_views[i]) != VK_SUCCESS) {
            Journal::error(Tags::Vulkan, "Could not create Image view!");
            return {};
        }
    }

    return image_views;
}

static auto create_framebuffers(VkDevice device, std::span<VkImageView> images, VkRenderPass render_pass, VkExtent2D extent)
    -> std::vector<VkFramebuffer> {
    std::vector<VkFramebuffer> framebuffers;
    framebuffers.resize(std::size(images));

    for (size_t i = 0; i < std::size(images); i++) {
        VkFramebufferCreateInfo framebuffer_create_info = {};
        framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_create_info.renderPass = render_pass;
        framebuffer_create_info.attachmentCount = 1;
        framebuffer_create_info.pAttachments = &images[i];
        framebuffer_create_info.width = extent.width;
        framebuffer_create_info.height = extent.height;
        framebuffer_create_info.layers = 1;

        if (vkCreateFramebuffer(device, &framebuffer_create_info, nullptr, &framebuffers[i]) != VK_SUCCESS) {
            Journal::error(Tags::Vulkan, "Could not create a framebuffer!");
            return {};
        }
    }

    return framebuffers;
}

static auto create_frame_sync_objects(VkDevice device, size_t frame_in_flights)
    -> std::tuple<std::vector<VkSemaphore>, std::vector<VkSemaphore>, std::vector<VkFence>> {
    std::vector<VkSemaphore> image_available_semaphores;
    std::vector<VkSemaphore> render_finished_semaphores;
    std::vector<VkFence> in_flight_fences;

    image_available_semaphores.resize(frame_in_flights);
    render_finished_semaphores.resize(frame_in_flights);
    in_flight_fences.resize(frame_in_flights);

    VkSemaphoreCreateInfo semaphore_create_info = {};
    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_create_info = {};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < frame_in_flights; i++) {
        if (vkCreateSemaphore(device, &semaphore_create_info, nullptr, &image_available_semaphores[i]) != VK_SUCCESS
            || vkCreateSemaphore(device, &semaphore_create_info, nullptr, &render_finished_semaphores[i]) != VK_SUCCESS
            || vkCreateFence(device, &fence_create_info, nullptr, &in_flight_fences[i]) != VK_SUCCESS) {
            Journal::error(Tags::Vulkan, "Could not create synchronization objects for a frame!");
            return {};
        }
    }

    return { image_available_semaphores, render_finished_semaphores, in_flight_fences };
}

auto create_swapchain(VkPhysicalDevice physical_device, VkDevice device, VkSurfaceKHR surface, const QueueFamilyIndices& indices,
    int32_t width, int32_t height, size_t frame_in_flights) -> std::optional<SwapChain> {

    auto swap_chain_support = query_swapchain_support(physical_device, surface);
    if (!swap_chain_support) {
        return {};
    }

    const auto desired_number_of_images = get_swap_chain_num_images(swap_chain_support->capabilities);
    const auto desired_format = choose_swap_surface_format(swap_chain_support->formats);
    const auto desired_extent = choose_swap_extent(swap_chain_support->capabilities, width, height);
    const auto desired_present_mode = choose_swap_present_mode(swap_chain_support->present_modes);
    const auto desired_transform = get_swap_chain_transform(swap_chain_support->capabilities);
    const auto desired_usage = get_swap_chain_usage_flags(swap_chain_support->capabilities);

    VkSwapchainCreateInfoKHR swap_chain_create_info = {};
    swap_chain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swap_chain_create_info.surface = surface;
    swap_chain_create_info.minImageCount = desired_number_of_images;
    swap_chain_create_info.imageFormat = desired_format.format;
    swap_chain_create_info.imageColorSpace = desired_format.colorSpace;
    swap_chain_create_info.imageExtent = desired_extent;
    swap_chain_create_info.imageArrayLayers = 1;
    swap_chain_create_info.imageUsage = desired_usage;
    swap_chain_create_info.preTransform = desired_transform;
    swap_chain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swap_chain_create_info.presentMode = desired_present_mode;
    swap_chain_create_info.clipped = VK_TRUE;
    swap_chain_create_info.oldSwapchain = VK_NULL_HANDLE;

    if (indices.graphics_queue_family_index != indices.present_queue_family_index) {
        uint32_t queueFamilyIndices[] = { indices.graphics_queue_family_index, indices.present_queue_family_index };
        swap_chain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swap_chain_create_info.queueFamilyIndexCount = 2;
        swap_chain_create_info.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        swap_chain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VkSwapchainKHR swap_chain = VK_NULL_HANDLE;
    if (vkCreateSwapchainKHR(device, &swap_chain_create_info, nullptr, &swap_chain) != VK_SUCCESS) {
        Journal::error(Tags::Vulkan, "Could not create swap chain!");
        return {};
    }

    uint32_t image_count = 0;
    vkGetSwapchainImagesKHR(device, swap_chain, &image_count, nullptr);

    std::vector<VkImage> swap_chain_images;
    swap_chain_images.resize(image_count);
    vkGetSwapchainImagesKHR(device, swap_chain, &image_count, &swap_chain_images[0]);

    auto swapchain_image_views = create_image_views(device, swap_chain_images, desired_format.format);
    if (swap_chain_images.empty()) {
        Journal::error(Tags::Vulkan, "Could not create swap chain image views!");
        return {};
    }

    // Create renderer passes
    VkAttachmentDescription color_attachment_desc = {};
    color_attachment_desc.format = desired_format.format;
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

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo render_pass_create_info {};
    render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_create_info.attachmentCount = 1;
    render_pass_create_info.pAttachments = &color_attachment_desc;
    render_pass_create_info.subpassCount = 1;
    render_pass_create_info.pSubpasses = &subpass;
    render_pass_create_info.dependencyCount = 1;
    render_pass_create_info.pDependencies = &dependency;

    VkRenderPass render_pass = VK_NULL_HANDLE;
    if (vkCreateRenderPass(device, &render_pass_create_info, nullptr, &render_pass) != VK_SUCCESS) {
        Journal::error(Tags::Vulkan, "Could not create render pass!");
        return {};
    }

    auto framebuffers = create_framebuffers(device, swapchain_image_views, render_pass,
        VkExtent2D { .width = static_cast<uint32_t>(width), .height = static_cast<uint32_t>(height) });

    auto [image_available_semaphores, render_finished_semaphores, in_flight_fences] = create_frame_sync_objects(device, frame_in_flights);

    SwapChain swapchain;
    swapchain.handle = swap_chain;
    swapchain.image_format = desired_format;
    swapchain.images = swap_chain_images;
    swapchain.extent = desired_extent;
    swapchain.views = swapchain_image_views;
    swapchain.render_pass = render_pass;
    swapchain.framebuffes = framebuffers;
    swapchain.image_available_semaphores = image_available_semaphores;
    swapchain.render_finished_semaphores = render_finished_semaphores;
    swapchain.in_flight_fences = in_flight_fences;
    swapchain.max_frames_in_flight = frame_in_flights;

    return { swapchain };
}

auto destroy_swapchain(VkDevice device, SwapChain& swapchain) -> void {

    for (auto s : swapchain.image_available_semaphores) {
        vkDestroySemaphore(device, s, nullptr);
    }
    for (auto s : swapchain.render_finished_semaphores) {
        vkDestroySemaphore(device, s, nullptr);
    }
    for (auto f : swapchain.in_flight_fences) {
        vkDestroyFence(device, f, nullptr);
    }

    vkDestroyRenderPass(device, swapchain.render_pass, nullptr);
    for (auto f : swapchain.framebuffes) {
        vkDestroyFramebuffer(device, f, nullptr);
    }

    for (auto& iv : swapchain.views) {
        vkDestroyImageView(device, iv, nullptr);
    }

    vkDestroySwapchainKHR(device, swapchain.handle, nullptr);
}

auto create_shader(VkDevice device, std::span<const uint8_t> shader_binary) -> VkShaderModule {
    VkShaderModuleCreateInfo shader_module_create_info = {};
    shader_module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_module_create_info.codeSize = std::size(shader_binary);
    shader_module_create_info.pCode = reinterpret_cast<const uint32_t*>(std::data(shader_binary));

    VkShaderModule shader_module;
    if (vkCreateShaderModule(device, &shader_module_create_info, nullptr, &shader_module) != VK_SUCCESS) {
        Journal::error(Tags::Vulkan, "Failed to create shader module!");
        return VK_NULL_HANDLE;
    }

    return shader_module;
}

auto destroy_shader(VkDevice device, VkShaderModule shader_module) noexcept -> void {
    vkDestroyShaderModule(device, shader_module, nullptr);
}

[[nodiscard]] auto create_graphics_pipeline(const CreatePipelineInfo& info) -> Pipeline {
    std::vector<VkShaderModule> shader_modules;
    std::vector<VkPipelineShaderStageCreateInfo> shader_stage_create_infos;
    for (const auto& s : info.shaders) {
        auto shader_module = create_shader(info.device, s.shader_binary);

        if (!shader_module) {
            continue;
        }

        shader_modules.push_back(shader_module);

        VkPipelineShaderStageCreateInfo shader_stage_create_info {};
        shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;

        if (s.shader_type == ShaderType::Vertex) {
            shader_stage_create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
        } else if (s.shader_type == ShaderType::Fragmet) {
            shader_stage_create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        } else if (s.shader_type == ShaderType::Geometry) {
            shader_stage_create_info.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
        }

        shader_stage_create_info.module = shader_module;
        shader_stage_create_info.pName = "main";

        shader_stage_create_infos.push_back(shader_stage_create_info);
    }

    VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info = {};
    vertex_input_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_state_create_info.vertexBindingDescriptionCount = 0;
    vertex_input_state_create_info.vertexAttributeDescriptionCount = 0;

    VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info = {};
    input_assembly_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_state_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly_state_create_info.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewport_state_create_info {};
    viewport_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state_create_info.viewportCount = 1;
    viewport_state_create_info.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterization_state_create_info = {};
    rasterization_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_state_create_info.depthClampEnable = VK_FALSE;
    rasterization_state_create_info.rasterizerDiscardEnable = VK_FALSE;
    rasterization_state_create_info.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization_state_create_info.lineWidth = 1.0f;
    rasterization_state_create_info.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization_state_create_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterization_state_create_info.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling_state_create_info = {};
    multisampling_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling_state_create_info.sampleShadingEnable = VK_FALSE;
    multisampling_state_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState color_blend_attachment_state = {};
    color_blend_attachment_state.colorWriteMask
        = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment_state.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo color_blend_state_create_info {};
    color_blend_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_state_create_info.logicOpEnable = VK_FALSE;
    color_blend_state_create_info.logicOp = VK_LOGIC_OP_COPY;
    color_blend_state_create_info.attachmentCount = 1;
    color_blend_state_create_info.pAttachments = &color_blend_attachment_state;
    color_blend_state_create_info.blendConstants[0] = 0.0f;
    color_blend_state_create_info.blendConstants[1] = 0.0f;
    color_blend_state_create_info.blendConstants[2] = 0.0f;
    color_blend_state_create_info.blendConstants[3] = 0.0f;

    std::vector<VkDynamicState> dynamic_states = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(std::size(dynamic_states));
    dynamicState.pDynamicStates = std::data(dynamic_states);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pushConstantRangeCount = 0;

    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    if (vkCreatePipelineLayout(info.device, &pipelineLayoutInfo, nullptr, &pipeline_layout) != VK_SUCCESS) {
        Journal::error(Tags::Vulkan, "Failed to create pipeline layout!");
        return {};
    }

    VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {};
    graphics_pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    graphics_pipeline_create_info.stageCount = std::size(shader_stage_create_infos);
    graphics_pipeline_create_info.pStages = std::data(shader_stage_create_infos);
    graphics_pipeline_create_info.pVertexInputState = &vertex_input_state_create_info;
    graphics_pipeline_create_info.pInputAssemblyState = &input_assembly_state_create_info;
    graphics_pipeline_create_info.pViewportState = &viewport_state_create_info;
    graphics_pipeline_create_info.pRasterizationState = &rasterization_state_create_info;
    graphics_pipeline_create_info.pMultisampleState = &multisampling_state_create_info;
    graphics_pipeline_create_info.pColorBlendState = &color_blend_state_create_info;
    graphics_pipeline_create_info.pDynamicState = &dynamicState;
    graphics_pipeline_create_info.layout = pipeline_layout;
    graphics_pipeline_create_info.renderPass = info.render_pass;
    graphics_pipeline_create_info.subpass = 0;
    graphics_pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;

    VkPipeline graphics_pipeline = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(info.device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr, &graphics_pipeline)
        != VK_SUCCESS) {
        Journal::error(Tags::Vulkan, "Failed to create graphics pipeline!");
        return {};
    }

    for (const auto& sh : shader_modules) {
        destroy_shader(info.device, sh);
    }

    Pipeline p;
    p.device = info.device;
    p.layout = pipeline_layout;
    p.pipeline = graphics_pipeline;

    return p;
}

auto destroy_graphics_pipeline(Pipeline& pipeline) -> void {
    vkDestroyPipeline(pipeline.device, pipeline.pipeline, nullptr);
    vkDestroyPipelineLayout(pipeline.device, pipeline.layout, nullptr);
}

} // namespace Vulkan
