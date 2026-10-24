#pragma once

import std;

#include "core/error.hpp"

namespace cppxls::core {

[[nodiscard]] auto read_file_bytes(std::string_view str_path)
    -> Result<std::vector<std::byte>>;

}
