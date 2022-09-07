#pragma once

#include <atomic>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace Storage {

using Buffer = std::vector<uint8_t>;

struct EmptyInfo { };

struct FontInfo {
    std::string charset;
    size_t size = 0;
};

struct ImageInfo {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 0;
    uint32_t channels = 0;
    Buffer pixels;
};

enum class ShaderType {
    Unknown,
    Vertex, // Vertex Shader
    Fragmet, // Fragment Shader
    Geometry, // Geometry Shader
};

struct ShaderProgramInfo {
    ShaderType shader_type = ShaderType::Unknown;
    Buffer shader_binary;
};

using ResourceInfo = std::variant<EmptyInfo, ImageInfo, FontInfo, ShaderProgramInfo>;

struct ResourceDesc {
    ResourceDesc() = default;

    ResourceDesc(uint64_t rid, std::string_view n, std::string_view p, const ResourceInfo& r)
        : id { rid }
        , name { n }
        , path { p }
        , resource { r } {
    }

    uint64_t id = 0;
    bool in_memory = false;
    std::string name;
    std::string path;
    ResourceInfo resource;
};

struct Asset {
    std::string name;
    std::string version;
    std::string path;
    std::vector<ResourceDesc> resources;
};

struct Storage {
    std::vector<Asset> assets;
    std::vector<std::string> files;
    std::atomic_bool inited = false;
};

[[nodiscard]] auto open(Storage& storage, const std::vector<std::string_view>& names) -> bool;

auto close(Storage& storage) -> bool;

auto get_shader(Storage& storage, uint64_t resource_id) -> const ShaderProgramInfo*;

} // namespace Storage