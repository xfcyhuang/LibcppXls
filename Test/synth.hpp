#pragma once

import std;

namespace cppxls::test {

struct ByteBuilder {
  std::vector<std::byte> vec;

  auto u1(std::uint8_t u1_v) -> ByteBuilder& {
    vec.push_back(static_cast<std::byte>(u1_v));
    return *this;
  }

  auto u2(std::uint16_t u2_v) -> ByteBuilder& {
    return u1(static_cast<std::uint8_t>(u2_v))
        .u1(static_cast<std::uint8_t>(u2_v >> 8));
  }

  auto u4(std::uint32_t u4_v) -> ByteBuilder& {
    return u2(static_cast<std::uint16_t>(u4_v))
        .u2(static_cast<std::uint16_t>(u4_v >> 16));
  }

  auto f8(double fp8_v) -> ByteBuilder& {
    const std::uint64_t ku8_bits{std::bit_cast<std::uint64_t>(fp8_v)};
    return u4(static_cast<std::uint32_t>(ku8_bits))
        .u4(static_cast<std::uint32_t>(ku8_bits >> 32));
  }

  auto raw(std::string_view str_v) -> ByteBuilder& {
    for (const char ch : str_v) {
      vec.push_back(static_cast<std::byte>(ch));
    }
    return *this;
  }

  auto utf16(std::u16string_view str16_v) -> ByteBuilder& {
    for (const char16_t ch : str16_v) {
      u2(static_cast<std::uint16_t>(ch));
    }
    return *this;
  }

  auto take() -> std::vector<std::byte> { return std::move(vec); }
};

auto join(std::initializer_list<std::vector<std::byte>> vec_parts)
    -> std::vector<std::byte> {
  std::vector<std::byte> vec_out;
  for (const std::vector<std::byte>& vec_p : vec_parts) {
    vec_out.insert(vec_out.end(), vec_p.begin(), vec_p.end());
  }
  return vec_out;
}

auto rec(std::uint16_t u2_id, std::vector<std::byte> vec_body)
    -> std::vector<std::byte> {
  ByteBuilder bb;
  bb.u2(u2_id).u2(static_cast<std::uint16_t>(vec_body.size()));
  std::vector<std::byte> vec_out{bb.take()};
  vec_out.insert(vec_out.end(), vec_body.begin(), vec_body.end());
  return vec_out;
}

auto bof_globals() -> std::vector<std::byte> {
  ByteBuilder bb;
  bb.u2(0x0600).u2(0x0005);
  return rec(0x0809, bb.take());
}

auto bof_sheet() -> std::vector<std::byte> {
  ByteBuilder bb;
  bb.u2(0x0600).u2(0x0010);
  return rec(0x0809, bb.take());
}

auto eof_rec() -> std::vector<std::byte> { return rec(0x000A, {}); }

auto pad_rec(std::size_t st_len) -> std::vector<std::byte> {
  ByteBuilder pad;
  for (std::size_t st_i{0}; st_i < st_len; ++st_i) {
    pad.u1(0);
  }
  return rec(0x0001, pad.take());
}

auto boundsheet(std::uint32_t u4_pos) -> std::vector<std::byte> {
  ByteBuilder bb;
  bb.u4(u4_pos).u1(0).u1(0).u1(1).u1(0).raw("S");
  return rec(0x0085, bb.take());
}

struct OleSpec {
  std::vector<std::byte> vec_name{};
  std::uint32_t u4_start{2};
  std::uint64_t u8_size{0};
  bool b_size_from_stream{true};
  std::vector<std::byte> vec_extra_name{};
};

auto utf16le_bytes(std::u16string_view str16_v) -> std::vector<std::byte> {
  ByteBuilder bb;
  bb.utf16(str16_v).u2(0);
  return bb.take();
}

auto make_ole(std::span<const std::byte> sp_stream, const OleSpec& spec)
    -> std::vector<std::byte> {
  constexpr std::size_t kStSector{512};
  const std::size_t kst_data{
      (sp_stream.size() + kStSector - 1) / kStSector};
  ByteBuilder fat;
  fat.u4(0xFFFFFFFD).u4(0xFFFFFFFE);
  for (std::size_t st_i{0}; st_i < kst_data; ++st_i) {
    fat.u4(st_i + 1 < kst_data
               ? static_cast<std::uint32_t>(st_i + 3)
               : 0xFFFFFFFE);
  }
  while (fat.vec.size() < kStSector) {
    fat.u4(0xFFFFFFFF);
  }
  std::vector<std::byte> vec_fat{fat.take()};

  ByteBuilder dir;
  auto push_entry = [&dir](std::span<const std::byte> sp_name,
                           std::uint8_t u1_type, std::uint32_t u4_child,
                           std::uint32_t u4_start, std::uint64_t u8_size) {
    const std::size_t kst_base{dir.vec.size()};
    dir.vec.resize(kst_base + 128);
    std::byte* p_name{dir.vec.data() + kst_base};
    for (std::size_t st_i{0}; st_i < sp_name.size() && st_i < 62; ++st_i) {
      p_name[st_i] = sp_name[st_i];
    }
    ByteBuilder meta;
    meta.u2(static_cast<std::uint16_t>(sp_name.size()))
        .u1(u1_type)
        .u1(1)
        .u4(0xFFFFFFFF)
        .u4(0xFFFFFFFF)
        .u4(u4_child);
    for (std::size_t st_i{0}; st_i < 4; ++st_i) {
      meta.u4(0);
    }
    for (std::size_t st_i{0}; st_i < 5; ++st_i) {
      meta.u4(0);
    }
    meta.u4(u4_start)
        .u4(static_cast<std::uint32_t>(u8_size))
        .u4(static_cast<std::uint32_t>(u8_size >> 32));
    std::copy(meta.vec.begin(), meta.vec.end(),
              dir.vec.begin() + kst_base + 64);
  };
  push_entry(utf16le_bytes(u"Root Entry"), 5, 1, 0xFFFFFFFE, 0);
  push_entry(spec.vec_name, 2, 0xFFFFFFFF, spec.u4_start,
             spec.b_size_from_stream ? sp_stream.size() : spec.u8_size);
  if (!spec.vec_extra_name.empty()) {
    push_entry(spec.vec_extra_name, 2, 0xFFFFFFFF, 0xFFFFFFFE, 0);
  }
  while (dir.vec.size() < kStSector) {
    dir.u4(0);
  }
  std::vector<std::byte> vec_dir{dir.take()};

  ByteBuilder head;
  const std::array<std::uint8_t, 8> karr_sig{0xD0, 0xCF, 0x11, 0xE0,
                                             0xA1, 0xB1, 0x1A, 0xE1};
  for (const std::uint8_t u1_b : karr_sig) {
    head.u1(u1_b);
  }
  head.vec.resize(28);
  head.u1(0xFE).u1(0xFF);
  head.u2(9).u2(6);
  head.vec.resize(44);
  head.u4(1).u4(1);
  head.vec.resize(60);
  head.u4(0xFFFFFFFE);
  head.vec.resize(68);
  head.u4(0xFFFFFFFE).u4(0);
  head.vec.resize(76);
  head.u4(0);
  head.vec.resize(512);
  std::vector<std::byte> vec_head{head.take()};

  std::vector<std::byte> vec_out{std::move(vec_head)};
  vec_out.insert(vec_out.end(), vec_fat.begin(), vec_fat.end());
  vec_out.insert(vec_out.end(), vec_dir.begin(), vec_dir.end());
  vec_out.insert(vec_out.end(), sp_stream.begin(), sp_stream.end());
  vec_out.resize(vec_out.size() + (kst_data * kStSector - sp_stream.size()));
  return vec_out;
}

auto default_spec() -> OleSpec {
  OleSpec spec;
  spec.vec_name = utf16le_bytes(u"Workbook");
  return spec;
}

}
