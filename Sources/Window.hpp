#pragma once

#include <string>

typedef struct GLFWwindow GLFWwindow;

namespace Window {

struct CreateWindowInfo {
    std::string_view title;
    int32_t width = 0;
    int32_t height = 0;
};

struct Input {
    bool forward = false;
    bool backward = false;
    bool left = false;
    bool right = false;
    bool button_left = false;
    bool button_right = false;
    bool space = false;
};

[[nodiscard]] auto create_window(const CreateWindowInfo& info) -> GLFWwindow*;

auto destroy_window(GLFWwindow* window) -> void;

[[nodiscard]] auto process_window_events(GLFWwindow* window) -> bool;

[[nodiscard]] auto process_window_input(GLFWwindow* window, Input& input) -> bool;

} // namespace Window