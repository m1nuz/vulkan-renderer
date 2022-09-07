#include "Application.hpp"
#include "Journal.hpp"
#include "Renderer.hpp"
#include "Storage.hpp"
#include "Tags.hpp"
#include "Window.hpp"

#include <GLFW/glfw3.h>

namespace Application {

auto read_conf([[maybe_unused]] Configuration& conf, [[maybe_unused]] std::string_view path) -> bool {
    return true; // read default
}

auto run(Configuration& conf, [[maybe_unused]] Application& app) -> int {
    Journal::message(Tags::App, "Start");

    Storage::Storage storage;
    if (!Storage::open(storage, { "../Assets" })) {
        Journal::critical(Tags::App, "Couldn't open Storage!");
        return EXIT_FAILURE;
    }

    glfwInit();
    auto window = Window::create_window({ .title = conf.title, .width = conf.window_width, .height = conf.window_height });
    if (!window) {
        glfwTerminate();
        return EXIT_FAILURE;
    }

    auto renderer = Renderer::create_renderer(storage, { .app_name = conf.title, .validate = true, .window = window });
    if (!renderer) {
        Journal::critical(Tags::App, "Couldn't create Renderer!");
        glfwTerminate();
        return EXIT_FAILURE;
    }

    bool running = true;

    Journal::message(Tags::App, "Running {}", running);

    while (running) {

        Renderer::draw_frame(*renderer);

        running = Window::process_window_events(window);
    }

    Renderer::destroy_renderer(*renderer);

    Window::destroy_window(window);
    glfwTerminate();

    Journal::message(Tags::App, "Shutdown");

    return EXIT_SUCCESS;
}

} // namespace Application