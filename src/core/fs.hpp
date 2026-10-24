#pragma once

import std;

#include "core/error.hpp"

namespace cppxls::core {

[[nodiscard]] auto read_file_bytes(const std::filesystem::path& pth_file)
    -> Result<std::vector<std::byte>>;

}
