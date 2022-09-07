#pragma once

#include <fmt/chrono.h>
#include <fmt/color.h>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <string>
#include <string_view>

namespace Journal {

namespace v3 {

    template <typename... Args> inline auto critical(const std::string_view tag, Args&&... args) -> void {
        const auto str = fmt::format(std::forward<Args>(args)...);
        const auto t = std::time(nullptr);
        const auto style = bg(fmt::terminal_color::red) | fmt::emphasis::bold;
        fmt::print(style, "{:%Y-%m-%d %H-%M-%S} C: [{}] {}\n", fmt::localtime(t), tag, str);
    }

    template <typename... Args> inline auto error(const std::string_view tag, Args&&... args) -> void {
        const auto str = fmt::format(std::forward<Args>(args)...);
        const auto t = std::time(nullptr);
        const auto style = fg(fmt::terminal_color::bright_red);
        fmt::print(style, "{:%Y-%m-%d %H-%M-%S} E: [{}] {}\n", fmt::localtime(t), tag, str);
    }

    template <typename... Args> inline auto warning(const std::string_view tag, Args&&... args) -> void {
        const auto str = fmt::format(std::forward<Args>(args)...);
        const auto t = std::time(nullptr);
        const auto style = fg(fmt::terminal_color::bright_yellow);
        fmt::print(style, "{:%Y-%m-%d %H-%M-%S} W: [{}] {}\n", fmt::localtime(t), tag, str);
    }

    template <typename... Args> inline auto message(const std::string_view tag, Args&&... args) -> void {
        const auto str = fmt::format(std::forward<Args>(args)...);
        const auto t = std::time(nullptr);
        const auto style = fg(fmt::terminal_color::white);
        fmt::print(style, "{:%Y-%m-%d %H-%M-%S} I: [{}] {}\n", fmt::localtime(t), tag, str);
    }

    template <typename... Args> inline auto debug(const std::string_view tag, Args&&... args) -> void {
        const auto str = fmt::format(std::forward<Args>(args)...);
        const auto t = std::time(nullptr);
        const auto style = fg(fmt::terminal_color::cyan);
        fmt::print(style, "{:%Y-%m-%d %H-%M-%S} D: [{}] {}\n", fmt::localtime(t), tag, str);
    }

    template <typename... Args> inline auto verbose(const std::string_view tag, Args&&... args) -> void {
        const auto str = fmt::format(std::forward<Args>(args)...);
        const auto t = std::time(nullptr);
        const auto style = fg(fmt::terminal_color::blue);
        fmt::print(style, "{:%Y-%m-%d %H-%M-%S} V: [{}] {}\n", fmt::localtime(t), tag, str);
    }

} // namespace v3

} // namespace Journal

namespace Journal {

using namespace v3;

} // namespace Journal
