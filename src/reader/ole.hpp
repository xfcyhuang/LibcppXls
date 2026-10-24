#pragma once

import std;

#include "core/error.hpp"

namespace cppxls::reader {

struct OleEntry {
  std::string str_name;
  std::uint8_t u1_type{0};
  std::uint32_t u4_start{0};
  std::uint64_t u8_size{0};
};

struct OleFile {
  std::vector<std::byte> vec_data;
  std::size_t st_sector{512};
  std::size_t st_mini{64};
  std::vector<std::uint32_t> vec_fat;
  std::vector<std::uint32_t> vec_minifat;
  std::vector<OleEntry> vec_entries;
  std::uint32_t u4_root_start{0xFFFFFFFE};
  std::uint64_t u8_root_size{0};
};

[[nodiscard]] auto open_ole_file(std::string_view str_path)
    -> core::Result<OleFile>;

[[nodiscard]] auto open_ole_buffer(std::span<const std::byte> sp_data)
    -> core::Result<OleFile>;

[[nodiscard]] auto ole_read_stream(const OleFile& ole,
                                   std::string_view str_name)
    -> core::Result<std::vector<std::byte>>;

[[nodiscard]] auto ole_read_workbook_stream(const OleFile& ole)
    -> core::Result<std::vector<std::byte>>;

}
