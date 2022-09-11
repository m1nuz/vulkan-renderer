#include "Storage.hpp"
#include "Content.hpp"
#include "Journal.hpp"
#include "Tags.hpp"

#include <xxhash.h>

#include <filesystem>
#include <thread>
#include <unordered_map>

namespace fs = std::filesystem;

namespace Storage {

template <typename T> inline auto get_resource_id(std::string_view name) -> uint64_t {
    std::string fullname;
    if constexpr (std::is_same_v<T, ImageInfo>) {
        fullname = "image:";
        fullname += name;
    } else if (std::is_same_v<T, FontInfo>) {
        fullname = "font:";
        fullname += name;
    } else if (std::is_same_v<T, ShaderProgramInfo>) {
        fullname = "shader:";
        fullname += name;
    }

    return XXH64(std::data(fullname), std::size(fullname), 123);
}

static auto read_asset([[maybe_unused]] Storage& storage, [[maybe_unused]] const fs::path& path) -> bool {
    return true;
}

static auto is_asset_file(Storage& storage, std::string_view name) -> bool {
    for (const auto& a : storage.assets) {
        for (const auto& r : a.resources) {
            if (r.name == name) {
                return true;
            }
        }
    }

    return false;
}

static auto is_shader(const fs::path& name) -> bool {
    const auto ext = name.extension().string();
    if (ext == ".spv") {
        return true;
    }

    return false;
}

[[nodiscard]] auto open(Storage& storage, const std::vector<std::string_view>& names) -> bool {

    bool any_read = false;

    // Read assets first
    for (const auto& name : names) {
        if (fs::exists(name)) {
            if (fs::is_directory(name)) {
                // find .asset file and load resources
                for (const auto& p : fs::directory_iterator(name)) {
                    if (p.path().extension() == ".asset") {
                        any_read |= read_asset(storage, p.path());
                    }
                }
            } else if (fs::is_regular_file(name)) {
                // TODO: read archive
            }
        } else {
            Journal::warning(Tags::Storage, "{} doesn't exists", name);
        }
    }

    // Read files that not in assets
    std::unordered_map<std::string, std::string> files;
    for (const auto& name : names) {
        for (auto const& entry : fs::recursive_directory_iterator(name)) {
            if (fs::is_regular_file(entry)) {
                const auto filename = entry.path().filename().string();
                if (is_asset_file(storage, filename)) {
                    // Journal::verbose(Tags::Storage, "filename - {}", filename, entry.path().string());
                } else {
                    // Journal::debug(Tags::Storage, "filename - {}", filename, entry.path().string());
                    files.emplace(filename, entry.path().string());
                }
            }
        }
    }

    if (!files.empty()) {
        Asset asset;
        asset.name = "General";
        asset.version = "1.0";

        for (const auto& [name, path] : files) {
            if (is_shader(name)) {
                ShaderProgramInfo resource_info;

                const auto resource_id = get_resource_id<ShaderProgramInfo>(name);

                Journal::debug(Tags::Storage, "{} - '{}'", resource_id, name);

                asset.resources.emplace_back(resource_id, name, path, resource_info);
            }
        }

        if (!asset.resources.empty()) {
            storage.assets.push_back(asset);
        }
    }

    return any_read || !storage.assets.empty();
}

auto close([[maybe_unused]] Storage& storage) -> bool {
    return true;
}

static auto read_shader(ShaderType st, std::string_view path) -> std::optional<ShaderProgramInfo> {

    auto content = Content::read<Buffer>(path);
    if (!content) {
        return {};
    }

    ShaderProgramInfo si;
    si.shader_type = st;
    si.shader_binary = *content;

    return { si };
}

static auto read_image([[maybe_unused]] std::string_view path) -> std::optional<ImageInfo> {
    return std::nullopt;
}

static auto read_model([[maybe_unused]] std::string_view path) -> std::optional<ModelInfo> {
    return std::nullopt;
}

static auto get_shader_type(std::string_view name) -> ShaderType {
    using namespace std::literals;

    if (name.find(".vert"sv) != std::string_view::npos) {
        return ShaderType::Vertex;
    } else if (name.find(".frag"sv) != std::string_view::npos) {
        return ShaderType::Fragmet;
    }

    return ShaderType::Unknown;
}

auto get_shader(Storage& storage, uint64_t resource_id) -> const ShaderProgramInfo* {

    for (auto& a : storage.assets) {
        for (auto& r : a.resources) {
            if (r.id == resource_id) {
                if (!r.in_memory) {
                    auto shader = read_shader(get_shader_type(r.name), r.path);
                    if (shader) {
                        r.resource = *shader;
                    }
                }

                if (const auto* p = std::get_if<ShaderProgramInfo>(&r.resource); p) {
                    return p;
                }
            }
        }
    }

    return nullptr;
}

auto get_image(Storage& storage, uint64_t resource_id) -> const ImageInfo* {
    for (auto& a : storage.assets) {
        for (auto& r : a.resources) {
            if (r.id == resource_id) {
                if (!r.in_memory) {
                    auto image = read_image(r.path);
                    if (image) {
                        r.resource = *image;
                    }
                }

                if (const auto* p = std::get_if<ImageInfo>(&r.resource); p) {
                    return p;
                }
            }
        }
    }

    return nullptr;
}

auto get_model(Storage& storage, uint64_t resource_id) -> const ModelInfo* {
    for (auto& a : storage.assets) {
        for (auto& r : a.resources) {
            if (r.id == resource_id) {
                if (!r.in_memory) {
                    auto image = read_model(r.path);
                    if (image) {
                        r.resource = *image;
                    }
                }

                if (const auto* p = std::get_if<ModelInfo>(&r.resource); p) {
                    return p;
                }
            }
        }
    }

    return nullptr;
}

} // namespace Storage