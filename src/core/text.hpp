#pragma once

import std;

namespace cppxls::core {

auto utf8_push(std::string& str_out, std::uint32_t u4_cp) -> void;

auto latin1_push(std::string& str_out, std::uint8_t u1_ch) -> void;

[[nodiscard]] auto decode_utf16_span(std::span<const std::byte> sp_raw)
    -> std::string;

[[nodiscard]] auto decode_latin1_span(std::span<const std::byte> sp_raw)
    -> std::string;

auto strip_trailing_nulls(std::string& str_io) -> void;

}
