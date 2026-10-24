import std;

#include "core/bytes.hpp"
#include "core/fs.hpp"
#include "core/text.hpp"
#include "reader/ole.hpp"

namespace cppxls::reader {

namespace {

using core::u1_at;
using core::u2_at;
using core::u4_at;
using core::u8_at;

constexpr std::uint32_t ku4_end_of_chain = 0xFFFFFFFE;
constexpr std::uint32_t ku4_free_sect = 0xFFFFFFFF;
constexpr std::uint32_t ku4_fat_sect = 0xFFFFFFFD;
constexpr std::uint32_t ku4_dif_sect = 0xFFFFFFFC;
constexpr std::uint64_t ku8_cutoff = 4096;
constexpr std::size_t kStDirEntry = 128;

[[nodiscard]] auto sector_span(const OleFile& ole, std::uint32_t u4_id)
    -> std::span<const std::byte> {
  const std::uint64_t u8_off =
      (static_cast<std::uint64_t>(u4_id) + 1) * ole.st_sector;
  if (u8_off + ole.st_sector > ole.vec_data.size()) {
    return {};
  }
  return std::span<const std::byte>{ole.vec_data}
      .subspan(static_cast<std::size_t>(u8_off), ole.st_sector);
}

[[nodiscard]] auto fat_chain(const std::vector<std::uint32_t>& vec_table,
                             std::uint32_t u4_start)
    -> core::Result<std::vector<std::uint32_t>> {
  std::vector<std::uint32_t> vec_ids;
  std::uint32_t u4_cur{u4_start};
  while (u4_cur != ku4_end_of_chain && u4_cur != ku4_free_sect) {
    if (u4_cur == ku4_fat_sect || u4_cur == ku4_dif_sect ||
        u4_cur >= vec_table.size() ||
        vec_ids.size() >= vec_table.size() + 1) {
      return core::fail(core::ErrorCode::kParse, "broken fat chain");
    }
    vec_ids.push_back(u4_cur);
    u4_cur = vec_table[u4_cur];
  }
  return vec_ids;
}

[[nodiscard]] auto concat_sectors(const OleFile& ole,
                                  const std::vector<std::uint32_t>& vec_ids)
    -> std::vector<std::byte> {
  std::vector<std::byte> vec_out;
  vec_out.reserve(vec_ids.size() * ole.st_sector);
  for (const std::uint32_t u4_id : vec_ids) {
    const std::span<const std::byte> sp_sec{sector_span(ole, u4_id)};
    vec_out.insert(vec_out.end(), sp_sec.begin(), sp_sec.end());
  }
  return vec_out;
}

[[nodiscard]] auto fold_ascii(char ch) -> char {
  return (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch - 'A' + 'a') : ch;
}

[[nodiscard]] auto names_equal_ci(std::string_view str_a,
                                  std::string_view str_b) -> bool {
  return std::ranges::equal(str_a, str_b, {}, fold_ascii, fold_ascii);
}

[[nodiscard]] auto load_ole(OleFile& ole) -> core::Result<void> {
  const std::span<const std::byte> sp_data{ole.vec_data};
  if (sp_data.size() < 512) {
    return core::fail(core::ErrorCode::kParse, "ole header missing");
  }
  const std::array<std::uint8_t, 8> karr_sig{0xD0, 0xCF, 0x11, 0xE0,
                                             0xA1, 0xB1, 0x1A, 0xE1};
  for (std::size_t st_i{0}; st_i < karr_sig.size(); ++st_i) {
    if (u1_at(sp_data, st_i) != karr_sig[st_i]) {
      return core::fail(core::ErrorCode::kParse, "not an ole2 compound file");
    }
  }

  const std::uint16_t ku2_sector_shift{u2_at(sp_data, 30)};
  const std::uint16_t ku2_mini_shift{u2_at(sp_data, 32)};
  if (ku2_sector_shift < 7 || ku2_sector_shift > 20 ||
      ku2_mini_shift < 6 || ku2_mini_shift > ku2_sector_shift) {
    return core::fail(core::ErrorCode::kParse, "bad ole sector shift");
  }
  ole.st_sector = std::size_t{1} << ku2_sector_shift;
  ole.st_mini = std::size_t{1} << ku2_mini_shift;

  const std::uint32_t ku4_fat_count{u4_at(sp_data, 44)};
  const std::uint32_t ku4_dir_start{u4_at(sp_data, 48)};
  const std::uint32_t ku4_minifat_start{u4_at(sp_data, 60)};
  const std::uint32_t ku4_difat_start{u4_at(sp_data, 68)};
  const std::uint32_t ku4_difat_count{u4_at(sp_data, 72)};
  const std::uint64_t ku8_sectors{
      (static_cast<std::uint64_t>(sp_data.size()) - 512) / ole.st_sector};
  if (ku4_fat_count > ku8_sectors) {
    return core::fail(core::ErrorCode::kParse, "fat sector count out of range");
  }

  std::vector<std::uint32_t> vec_difat;
  vec_difat.reserve(ku4_fat_count);
  for (std::uint32_t u4_i{0}; u4_i < 109 && vec_difat.size() < ku4_fat_count;
       ++u4_i) {
    const std::uint32_t ku4_entry{u4_at(sp_data, 76 + u4_i * 4)};
    if (ku4_entry == ku4_free_sect) {
      continue;
    }
    vec_difat.push_back(ku4_entry);
  }

  std::uint32_t u4_dif_cur{ku4_difat_start};
  std::uint64_t u8_dif_seen{0};
  while (u4_dif_cur != ku4_end_of_chain && u4_dif_cur != ku4_free_sect &&
         vec_difat.size() < ku4_fat_count &&
         u8_dif_seen <= static_cast<std::uint64_t>(ku4_difat_count) + 1) {
    const std::span<const std::byte> sp_dif{sector_span(ole, u4_dif_cur)};
    if (sp_dif.empty()) {
      return core::fail(core::ErrorCode::kParse, "bad difat sector");
    }
    ++u8_dif_seen;
    const std::size_t kst_per{ole.st_sector / 4};
    for (std::size_t st_i{0}; st_i + 1 < kst_per && vec_difat.size() < ku4_fat_count;
         ++st_i) {
      const std::uint32_t ku4_entry{u4_at(sp_dif, st_i * 4)};
      if (ku4_entry == ku4_free_sect) {
        continue;
      }
      vec_difat.push_back(ku4_entry);
    }
    u4_dif_cur = u4_at(sp_dif, (kst_per - 1) * 4);
  }

  const std::size_t kst_fat_entries{ole.st_sector / 4};
  ole.vec_fat.assign(
      static_cast<std::size_t>(ku4_fat_count) * kst_fat_entries, ku4_free_sect);
  for (std::size_t st_s{0}; st_s < vec_difat.size(); ++st_s) {
    const std::span<const std::byte> sp_fat{
        sector_span(ole, vec_difat[st_s])};
    if (sp_fat.empty()) {
      return core::fail(core::ErrorCode::kParse, "bad fat sector");
    }
    const std::size_t kst_base{st_s * kst_fat_entries};
    const std::size_t kst_copy{std::min(kst_fat_entries, ole.vec_fat.size() - kst_base)};
    for (std::size_t st_i{0}; st_i < kst_copy; ++st_i) {
      ole.vec_fat[kst_base + st_i] = u4_at(sp_fat, st_i * 4);
    }
  }
  if (ole.vec_fat.size() > static_cast<std::size_t>(ku8_sectors)) {
    ole.vec_fat.resize(static_cast<std::size_t>(ku8_sectors));
  }

  auto res_dir{fat_chain(ole.vec_fat, ku4_dir_start)};
  if (!res_dir) {
    return std::unexpected{res_dir.error()};
  }
  const std::vector<std::byte> vec_dir{concat_sectors(ole, *res_dir)};
  for (std::size_t st_off{0}; st_off + kStDirEntry <= vec_dir.size();
       st_off += kStDirEntry) {
    const std::span<const std::byte> sp_ent{vec_dir.data() + st_off,
                                            kStDirEntry};
    const std::uint8_t ku1_type{u1_at(sp_ent, 66)};
    const std::uint16_t ku2_name_len{u2_at(sp_ent, 64)};
    if (ku1_type < 1 || ku1_type > 5) {
      continue;
    }
    if (ku1_type == 5) {
      ole.u4_root_start = u4_at(sp_ent, 116);
      ole.u8_root_size = u8_at(sp_ent, 120);
    }
    if (ku2_name_len < 4 || ku2_name_len > 64) {
      continue;
    }
    OleEntry entry;
    entry.str_name = core::decode_utf16_span(
        std::span<const std::byte>{sp_ent.data(),
                                   static_cast<std::size_t>(ku2_name_len - 2)});
    core::strip_trailing_nulls(entry.str_name);
    entry.u1_type = ku1_type;
    entry.u4_start = u4_at(sp_ent, 116);
    entry.u8_size = u8_at(sp_ent, 120);
    ole.vec_entries.push_back(std::move(entry));
  }

  auto res_mini{fat_chain(ole.vec_fat, ku4_minifat_start)};
  if (res_mini) {
    const std::vector<std::byte> vec_mini{concat_sectors(ole, *res_mini)};
    ole.vec_minifat.resize(vec_mini.size() / 4);
    for (std::size_t st_i{0}; st_i < ole.vec_minifat.size(); ++st_i) {
      ole.vec_minifat[st_i] = u4_at(
          std::span<const std::byte>{vec_mini}, st_i * 4);
    }
  }
  return {};
}

}

auto open_ole_file(const std::filesystem::path& pth_file) -> core::Result<OleFile> {
  auto res_data{core::read_file_bytes(pth_file)};
  if (!res_data) {
    return std::unexpected{res_data.error()};
  }
  return open_ole_owned(std::move(*res_data));
}

auto open_ole_buffer(std::span<const std::byte> sp_data)
    -> core::Result<OleFile> {
  return open_ole_owned(
      std::vector<std::byte>{sp_data.begin(), sp_data.end()});
}

auto open_ole_owned(std::vector<std::byte> vec_data) -> core::Result<OleFile> {
  OleFile ole;
  ole.vec_data = std::move(vec_data);
  auto res_load{load_ole(ole)};
  if (!res_load) {
    return std::unexpected{res_load.error()};
  }
  return std::move(ole);
}

auto ole_read_stream(const OleFile& ole, std::string_view str_name)
    -> core::Result<std::vector<std::byte>> {
  const OleEntry* p_entry{nullptr};
  for (const OleEntry& entry : ole.vec_entries) {
    if (entry.u1_type == 2 && names_equal_ci(entry.str_name, str_name)) {
      p_entry = &entry;
      break;
    }
  }
  if (p_entry == nullptr) {
    return core::fail(core::ErrorCode::kStreamNotFound, "ole stream not found");
  }

  if (p_entry->u8_size >= ku8_cutoff) {
    auto res_ids{fat_chain(ole.vec_fat, p_entry->u4_start)};
    if (!res_ids) {
      return std::unexpected{res_ids.error()};
    }
    const std::uint64_t ku8_need_sectors{
        (p_entry->u8_size + ole.st_sector - 1) / ole.st_sector};
    std::vector<std::byte> vec_out;
    vec_out.reserve(static_cast<std::size_t>(std::min<std::uint64_t>(
                        res_ids->size(), ku8_need_sectors)) *
                    ole.st_sector);
    for (const std::uint32_t u4_id : *res_ids) {
      const std::span<const std::byte> sp_sec{sector_span(ole, u4_id)};
      vec_out.insert(vec_out.end(), sp_sec.begin(), sp_sec.end());
      if (vec_out.size() >= p_entry->u8_size) {
        break;
      }
    }
    if (vec_out.size() < p_entry->u8_size) {
      return core::fail(core::ErrorCode::kParse, "stream truncated");
    }
    vec_out.resize(p_entry->u8_size);
    return vec_out;
  }

  auto res_root{fat_chain(ole.vec_fat, ole.u4_root_start)};
  if (!res_root) {
    return std::unexpected{res_root.error()};
  }
  const std::vector<std::byte> vec_root{concat_sectors(ole, *res_root)};
  auto res_ids{fat_chain(ole.vec_minifat, p_entry->u4_start)};
  if (!res_ids) {
    return std::unexpected{res_ids.error()};
  }
  const std::uint64_t ku8_need_sectors{
      (p_entry->u8_size + ole.st_mini - 1) / ole.st_mini};
  std::vector<std::byte> vec_out;
  vec_out.reserve(static_cast<std::size_t>(std::min<std::uint64_t>(
                      res_ids->size(), ku8_need_sectors)) *
                  ole.st_mini);
  for (const std::uint32_t u4_id : *res_ids) {
    const std::uint64_t u8_off{
        static_cast<std::uint64_t>(u4_id) * ole.st_mini};
    if (u8_off + ole.st_mini > vec_root.size()) {
      return core::fail(core::ErrorCode::kParse, "ministream truncated");
    }
    vec_out.insert(
        vec_out.end(), vec_root.begin() + static_cast<std::ptrdiff_t>(u8_off),
        vec_root.begin() + static_cast<std::ptrdiff_t>(u8_off + ole.st_mini));
    if (vec_out.size() >= p_entry->u8_size) {
      break;
    }
  }
  if (vec_out.size() < p_entry->u8_size) {
    return core::fail(core::ErrorCode::kParse, "stream truncated");
  }
  vec_out.resize(p_entry->u8_size);
  return vec_out;
}

auto ole_read_workbook_stream(const OleFile& ole)
    -> core::Result<std::vector<std::byte>> {
  auto res_main{ole_read_stream(ole, "Workbook")};
  if (res_main) {
    return res_main;
  }
  if (res_main.error().u1_code != core::ErrorCode::kStreamNotFound) {
    return std::unexpected{res_main.error()};
  }
  return ole_read_stream(ole, "Book");
}

}
