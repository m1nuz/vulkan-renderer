#pragma once

#include <string>

typedef struct GLFWwindow GLFWwindow;

namespace Window {

struct CreateWindowInfo {
    std::string_view title;
    int32_t width = 0;
    int32_t height = 0;
};

[[nodiscard]] auto create_window(const CreateWindowInfo& info) -> GLFWwindow*;

auto destroy_window(GLFWwindow* window) -> void;

[[nodiscard]] auto process_window_events(GLFWwindow* window) -> bool;

} // namespace Window