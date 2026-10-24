#pragma once

import std;

namespace cppxls::core {

[[nodiscard]] auto u1_at(std::span<const std::byte> sp,
                         std::size_t st_off) -> std::uint8_t;

[[nodiscard]] auto u2_at(std::span<const std::byte> sp,
                         std::size_t st_off) -> std::uint16_t;

[[nodiscard]] auto u4_at(std::span<const std::byte> sp,
                         std::size_t st_off) -> std::uint32_t;

[[nodiscard]] auto u8_at(std::span<const std::byte> sp,
                         std::size_t st_off) -> std::uint64_t;

[[nodiscard]] auto f8_at(std::span<const std::byte> sp,
                         std::size_t st_off) -> double;

}
