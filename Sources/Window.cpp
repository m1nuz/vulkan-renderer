#include "Window.hpp"
#include "Journal.hpp"
#include "Tags.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace Window {

static auto center_window(GLFWwindow* window, GLFWmonitor* monitor) {
    if (!monitor)
        return;

    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    if (!mode)
        return;

    int monitor_x = 0, monitor_y = 0;
    glfwGetMonitorPos(monitor, &monitor_x, &monitor_y);

    int width = 0, height = 0;
    glfwGetWindowSize(window, &width, &height);

    glfwSetWindowPos(window, monitor_x + (mode->width - width) / 2, monitor_y + (mode->height - height) / 2);
}

static auto is_key_pressed(GLFWwindow* window, int key) noexcept -> bool {
    return glfwGetKey(window, key) == GLFW_PRESS;
}

static auto is_mouse_pressed(GLFWwindow* window, int button) noexcept -> bool {
    return glfwGetMouseButton(window, button) == GLFW_PRESS;
}

auto destroy_window(GLFWwindow* window) -> void {
    glfwDestroyWindow(window);
}

auto process_window_events(GLFWwindow* window) -> bool {
    if (glfwWindowShouldClose(window)) {
        return false;
    }

    glfwPollEvents();
    return true;
}

auto process_window_input(GLFWwindow* window, Input& input) -> void {
    input.forward = is_key_pressed(window, GLFW_KEY_W);
    input.backward = is_key_pressed(window, GLFW_KEY_S);
    input.left = is_key_pressed(window, GLFW_KEY_D);
    input.right = is_key_pressed(window, GLFW_KEY_A);
    input.space = is_key_pressed(window, GLFW_KEY_SPACE);
    input.button_right = is_mouse_pressed(window, GLFW_MOUSE_BUTTON_RIGHT);
    input.button_left = is_mouse_pressed(window, GLFW_MOUSE_BUTTON_LEFT);
}

[[nodiscard]] auto create_window(const CreateWindowInfo& info) -> GLFWwindow* {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    auto window = glfwCreateWindow(info.width, info.height, std::data(info.title), nullptr, nullptr);
    if (!window) {
        const char* description = nullptr;
        const auto err = glfwGetError(&description);
        Journal::critical(Tags::App, "Error: {} {}", err, description);
        return nullptr;
    }

    center_window(window, glfwGetPrimaryMonitor());
    glfwSetCursorPos(window, info.width / 2.f, info.height / 2.f);

    glfwShowWindow(window);

    return window;
}

} // namespace Window