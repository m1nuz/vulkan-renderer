#pragma once

#include <fstream>
#include <optional>
#include <string>

namespace Content {

template <typename Buffer> inline auto read(std::string_view filepath) -> std::optional<Buffer> {
    using namespace std;

    ifstream fs(filepath.data(), ios::in | ios::binary);

    if (!fs.is_open())
        return {};

    Buffer buf;
    fs.seekg(0, ios::end);
    const auto sz = fs.tellg();
    buf.resize(static_cast<size_t>(sz));
    fs.seekg(0, ios::beg);
    fs.read(reinterpret_cast<char*>(std::data(buf)), static_cast<std::streamsize>(std::size(buf)));
    fs.close();

    return { buf };
}

inline auto write(std::string_view path, std::string_view buf) -> bool {
    std::ofstream fs(path.data());
    if (!fs.is_open()) {
        return false;
    }

    fs.write(buf.data(), static_cast<std::streamsize>(std::size(buf)));
    fs.close();

    return true;
}

} // namespace Content