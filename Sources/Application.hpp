#pragma once

#include <cstdint>
#include <string>

namespace Application {

struct Configuration {
    int32_t window_width = 1920;
    int32_t window_height = 1080;
    std::string_view title = "Vulkan Renderer";
    bool fullscreen = false;
    bool vsync = false;
    bool window_centered = true;
    bool debug_graphics = true;
};

struct Application { };

auto read_conf(Configuration& conf, std::string_view path) -> bool;
auto run(Configuration& conf, Application& app) -> int;

} // namespace Application