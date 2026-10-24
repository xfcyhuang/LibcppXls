import std;

#include "core/bytes.hpp"

namespace cppxls::core {

auto u1_at(std::span<const std::byte> sp, std::size_t st_off)
    -> std::uint8_t {
  return static_cast<std::uint8_t>(sp[st_off]);
}

auto u2_at(std::span<const std::byte> sp, std::size_t st_off)
    -> std::uint16_t {
  return static_cast<std::uint16_t>(
      static_cast<std::uint16_t>(u1_at(sp, st_off)) |
      (static_cast<std::uint16_t>(u1_at(sp, st_off + 1)) << 8));
}

auto u4_at(std::span<const std::byte> sp, std::size_t st_off)
    -> std::uint32_t {
  return static_cast<std::uint32_t>(u2_at(sp, st_off)) |
         (static_cast<std::uint32_t>(u2_at(sp, st_off + 2)) << 16);
}

auto u8_at(std::span<const std::byte> sp, std::size_t st_off)
    -> std::uint64_t {
  return static_cast<std::uint64_t>(u4_at(sp, st_off)) |
         (static_cast<std::uint64_t>(u4_at(sp, st_off + 4)) << 32);
}

auto f8_at(std::span<const std::byte> sp, std::size_t st_off) -> double {
  std::uint64_t u8_bits{0};
  for (std::size_t st_i{0}; st_i < 8; ++st_i) {
    u8_bits |= static_cast<std::uint64_t>(u1_at(sp, st_off + st_i))
               << (8 * st_i);
  }
  return std::bit_cast<double>(u8_bits);
}

}
