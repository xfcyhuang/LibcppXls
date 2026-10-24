import std;

#include "core/bytes.hpp"
#include "core/text.hpp"

namespace cppxls::core {

auto utf8_push(std::string& str_out, std::uint32_t u4_cp) -> void {
  if (u4_cp < 0x80) [[likely]] {
    str_out.push_back(static_cast<char>(u4_cp));
  } else if (u4_cp < 0x800) {
    const char arr[2]{static_cast<char>(0xC0 | (u4_cp >> 6)),
                      static_cast<char>(0x80 | (u4_cp & 0x3F))};
    str_out.append(arr, 2);
  } else if (u4_cp < 0x10000) {
    const char arr[3]{static_cast<char>(0xE0 | (u4_cp >> 12)),
                      static_cast<char>(0x80 | ((u4_cp >> 6) & 0x3F)),
                      static_cast<char>(0x80 | (u4_cp & 0x3F))};
    str_out.append(arr, 3);
  } else {
    const char arr[4]{static_cast<char>(0xF0 | (u4_cp >> 18)),
                      static_cast<char>(0x80 | ((u4_cp >> 12) & 0x3F)),
                      static_cast<char>(0x80 | ((u4_cp >> 6) & 0x3F)),
                      static_cast<char>(0x80 | (u4_cp & 0x3F))};
    str_out.append(arr, 4);
  }
}

auto latin1_push(std::string& str_out, std::uint8_t u1_ch) -> void {
  utf8_push(str_out, u1_ch);
}

auto decode_utf16_span(std::span<const std::byte> sp_raw) -> std::string {
  std::string str_out;
  str_out.reserve(sp_raw.size() / 2 * 3);
  for (std::size_t st_i{0}; st_i + 1 < sp_raw.size(); st_i += 2) {
    const std::uint16_t ku2_hi{u2_at(sp_raw, st_i)};
    if (ku2_hi >= 0xD800 && ku2_hi <= 0xDBFF) {
      if (st_i + 3 < sp_raw.size()) {
        const std::uint16_t ku2_lo{u2_at(sp_raw, st_i + 2)};
        if (ku2_lo >= 0xDC00 && ku2_lo <= 0xDFFF) {
          utf8_push(str_out, 0x10000 +
                                 ((static_cast<std::uint32_t>(ku2_hi - 0xD800)
                                   << 10) |
                                  (ku2_lo - 0xDC00)));
          st_i += 2;
          continue;
        }
        utf8_push(str_out, 0xFFFD);
        utf8_push(str_out, ku2_lo < 0xD800 || ku2_lo > 0xDFFF
                               ? ku2_lo
                               : 0xFFFD);
        st_i += 2;
        continue;
      }
      utf8_push(str_out, 0xFFFD);
      continue;
    }
    if (ku2_hi >= 0xDC00 && ku2_hi <= 0xDFFF) {
      utf8_push(str_out, 0xFFFD);
      continue;
    }
    utf8_push(str_out, ku2_hi);
  }
  return str_out;
}

auto decode_latin1_span(std::span<const std::byte> sp_raw) -> std::string {
  std::string str_out;
  str_out.reserve(sp_raw.size() * 2);
  for (const std::byte b_ch : sp_raw) {
    latin1_push(str_out, static_cast<std::uint8_t>(b_ch));
  }
  return str_out;
}

auto strip_trailing_nulls(std::string& str_io) -> void {
  while (!str_io.empty() && str_io.back() == '\0') {
    str_io.pop_back();
  }
}

}
