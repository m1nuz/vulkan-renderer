#pragma once

#include <atomic>

#include "Asset.hpp"

namespace Storage {

struct Storage {
    std::vector<Asset> assets;
    std::vector<std::string> files;
    std::atomic_bool inited = false;
};

[[nodiscard]] auto open(Storage& storage, const std::vector<std::string_view>& names) -> bool;

auto close(Storage& storage) -> bool;

auto get_shader(Storage& storage, uint64_t resource_id) -> const ShaderProgramInfo*;

auto get_image(Storage& storage, uint64_t resource_id) -> const ImageInfo*;

auto get_model(Storage& storage, uint64_t resource_id) -> const ModelInfo*;

} // namespace Storage