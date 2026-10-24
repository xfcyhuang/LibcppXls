import std;

#include "core/bytes.hpp"
#include "core/fs.hpp"
#include "core/text.hpp"
#include "reader/biff.hpp"
#include "reader/ole.hpp"

namespace cppxls::reader {

namespace {

using core::f8_at;
using core::u1_at;
using core::u2_at;
using core::u4_at;

constexpr std::size_t kStRecHeader = 4;

struct RecView {
  Record e_id{Record::kEof};
  std::span<const std::byte> sp_body{};
};

[[nodiscard]] auto walk_records(std::span<const std::byte> sp_stream,
                                std::size_t st_from, bool b_stop_at_eof)
    -> std::vector<RecView> {
  std::vector<RecView> vec_recs;
  std::size_t st_pos{st_from};
  while (st_pos + kStRecHeader <= sp_stream.size()) {
    const std::uint16_t ku2_id{u2_at(sp_stream, st_pos)};
    const std::uint16_t ku2_len{u2_at(sp_stream, st_pos + 2)};
    st_pos += kStRecHeader;
    if (st_pos + ku2_len > sp_stream.size()) {
      break;
    }
    vec_recs.push_back(RecView{
        static_cast<Record>(ku2_id),
        sp_stream.subspan(st_pos, ku2_len)});
    st_pos += ku2_len;
    if (b_stop_at_eof && ku2_id == static_cast<std::uint16_t>(Record::kEof)) {
      break;
    }
  }
  return vec_recs;
}

struct StrSource {
  const std::vector<RecView>* vec_recs{nullptr};
  std::size_t st_next{0};
  std::span<const std::byte> sp_cur{};
  std::size_t st_pos{0};
  bool b_unicode{false};
  bool b_bad{false};
};

auto src_advance(StrSource& src) -> void {
  while (src.st_next < src.vec_recs->size()) {
    const RecView& rec{(*src.vec_recs)[src.st_next]};
    src.st_next += 1;
    if (rec.e_id != Record::kContinue) {
      src.b_bad = true;
      return;
    }
    if (rec.sp_body.empty()) {
      continue;
    }
    src.sp_cur = rec.sp_body;
    src.st_pos = 1;
    src.b_unicode = (u1_at(rec.sp_body, 0) & 0x01) != 0;
    return;
  }
  src.b_bad = true;
}

auto src_get_u8(StrSource& src) -> std::optional<std::uint8_t> {
  if (src.st_pos >= src.sp_cur.size()) {
    src_advance(src);
    if (src.b_bad || src.st_pos >= src.sp_cur.size()) {
      src.b_bad = true;
      return std::nullopt;
    }
  }
  const std::uint8_t ku1_v{u1_at(src.sp_cur, src.st_pos)};
  src.st_pos += 1;
  return ku1_v;
}

auto src_get_u2(StrSource& src) -> std::optional<std::uint16_t> {
  const auto o_lo{src_get_u8(src)};
  const auto o_hi{src_get_u8(src)};
  if (!o_lo || !o_hi) {
    src.b_bad = true;
    return std::nullopt;
  }
  return static_cast<std::uint16_t>(static_cast<std::uint16_t>(*o_lo) |
                                    (static_cast<std::uint16_t>(*o_hi) << 8));
}

auto src_get_u4(StrSource& src) -> std::optional<std::uint32_t> {
  const auto o_lo{src_get_u2(src)};
  if (!o_lo) {
    return std::nullopt;
  }
  const auto o_hi{src_get_u2(src)};
  if (!o_hi) {
    return std::nullopt;
  }
  return static_cast<std::uint32_t>(*o_lo) |
         (static_cast<std::uint32_t>(*o_hi) << 16);
}

auto src_skip(StrSource& src, std::uint64_t u8_n) -> void {
  while (u8_n > 0 && !src.b_bad) {
    if (!src_get_u8(src)) {
      src.b_bad = true;
      return;
    }
    u8_n -= 1;
  }
}

auto read_xl_string(StrSource& src, std::uint16_t u2_cch,
                    std::uint8_t u1_flags) -> std::string {
  const std::uint16_t ku2_runs{
      (u1_flags & 0x08) != 0 ? src_get_u2(src).value_or(0) : std::uint16_t{0}};
  const std::uint32_t ku4_ext{
      (u1_flags & 0x04) != 0 ? src_get_u4(src).value_or(0)
                             : std::uint32_t{0}};
  std::string str_out;
  str_out.reserve(std::size_t{u2_cch} * 3);
  for (std::uint16_t u2_i{0}; u2_i < u2_cch; ++u2_i) {
    if (src.b_bad) {
      break;
    }
    if (!src.b_unicode) {
      const auto o_ch{src_get_u8(src)};
      if (!o_ch) {
        src.b_bad = true;
        break;
      }
      core::latin1_push(str_out, *o_ch);
    } else {
      const auto o_hi{src_get_u2(src)};
      if (!o_hi) {
        break;
      }
      const std::uint16_t ku2_hi{*o_hi};
      if (ku2_hi >= 0xD800 && ku2_hi <= 0xDBFF) {
        const auto o_lo{src_get_u2(src)};
        if (!o_lo) {
          break;
        }
        const std::uint16_t ku2_lo{*o_lo};
        if (ku2_lo >= 0xDC00 && ku2_lo <= 0xDFFF) {
          core::utf8_push(str_out, 0x10000 +
                                       ((static_cast<std::uint32_t>(
                                             ku2_hi - 0xD800)
                                         << 10) |
                                        (ku2_lo - 0xDC00)));
        } else {
          core::utf8_push(str_out, 0xFFFD);
          core::utf8_push(str_out, ku2_lo < 0xD800 || ku2_lo > 0xDFFF
                                       ? ku2_lo
                                       : 0xFFFD);
        }
      } else if (ku2_hi >= 0xDC00 && ku2_hi <= 0xDFFF) {
        core::utf8_push(str_out, 0xFFFD);
      } else {
        core::utf8_push(str_out, ku2_hi);
      }
    }
  }
  src_skip(src, static_cast<std::uint64_t>(ku2_runs) * 4);
  src_skip(src, ku4_ext);
  return str_out;
}

[[nodiscard]] auto rk_to_double(std::uint32_t u4_rk) -> double {
  double fp8_out{0.0};
  if ((u4_rk & 0x02) != 0) {
    fp8_out = static_cast<double>(
        static_cast<std::int32_t>(u4_rk) >> 2);
  } else {
    const std::uint64_t ku8_bits{
        static_cast<std::uint64_t>(u4_rk & 0xFFFFFFFC) << 32};
    fp8_out = std::bit_cast<double>(ku8_bits);
  }
  if ((u4_rk & 0x01) != 0) {
    fp8_out /= 100.0;
  }
  return fp8_out;
}

[[nodiscard]] auto is_cell_record(Record e_id) -> bool {
  switch (e_id) {
    case Record::kFormula:
    case Record::kFormulaAlt:
    case Record::kNumber:
    case Record::kRk:
    case Record::kMulRk:
    case Record::kMulBlank:
    case Record::kLabel:
    case Record::kLabelSst:
    case Record::kBlank:
    case Record::kBoolErr:
    case Record::kRString:
      return true;
    default:
      return false;
  }
}

auto parse_globals(Workbook& wb) -> core::Result<void> {
  const std::vector<RecView> vec_recs{
      walk_records(wb.vec_stream, 0, true)};
  for (std::size_t st_i{0}; st_i < vec_recs.size(); ++st_i) {
    const RecView& rec{vec_recs[st_i]};
    const std::span<const std::byte> sp{rec.sp_body};
    switch (rec.e_id) {
      case Record::kBof: {
        if (sp.size() >= 4) {
          const std::uint16_t ku2_ver{u2_at(sp, 0)};
          wb.b_biff5 = ku2_ver < 0x0600;
        }
        break;
      }
      case Record::kFilePass:
        return core::fail(core::ErrorCode::kUnsupportedEncryption,
                          "FILEPASS: encrypted workbook");
      case Record::kCodepage:
        if (sp.size() >= 2) {
          wb.u2_codepage = u2_at(sp, 0);
        }
        break;
      case Record::kDateMode:
        if (sp.size() >= 2) {
          wb.b_1904 = u2_at(sp, 0) != 0;
        }
        break;
      case Record::kWindow1:
        if (sp.size() >= 12) {
          wb.u2_active_sheet = u2_at(sp, 10);
        }
        break;
      case Record::kBoundsheet: {
        if (sp.size() < 8) {
          break;
        }
        SheetInfo sheet;
        sheet.u4_filepos = u4_at(sp, 0);
        sheet.u1_visibility = u1_at(sp, 4);
        sheet.u1_type = u1_at(sp, 5);
        if (wb.b_biff5) {
          const std::uint8_t ku1_cch{u1_at(sp, 6)};
          if (sp.size() >= 7 + std::size_t{ku1_cch}) {
            sheet.str_name = core::decode_latin1_span(
                sp.subspan(7, ku1_cch));
          }
        } else {
          const std::uint8_t ku1_cch{u1_at(sp, 6)};
          const std::uint8_t ku1_flags{u1_at(sp, 7)};
          const std::size_t kst_chars{
              (ku1_flags & 0x01) != 0
                  ? std::size_t{ku1_cch} * 2
                  : std::size_t{ku1_cch}};
          if (sp.size() >= 8 + kst_chars) {
            sheet.str_name =
                (ku1_flags & 0x01) != 0
                    ? core::decode_utf16_span(sp.subspan(8, kst_chars))
                    : core::decode_latin1_span(sp.subspan(8, kst_chars));
          }
        }
        wb.vec_sheets.push_back(std::move(sheet));
        break;
      }
      case Record::kSst: {
        if (sp.size() < 8) {
          break;
        }
        const std::uint32_t ku4_unique{u4_at(sp, 4)};
        StrSource src{.vec_recs = &vec_recs,
                      .st_next = st_i + 1,
                      .sp_cur = sp,
                      .st_pos = 8,
                      .b_unicode = false,
                      .b_bad = false};
        for (std::uint32_t u4_n{0}; u4_n < ku4_unique && !src.b_bad; ++u4_n) {
          const std::uint16_t ku2_cch{src_get_u2(src).value_or(0)};
          if (src.b_bad) {
            break;
          }
          const auto o_flags{src_get_u8(src)};
          if (!o_flags) {
            src.b_bad = true;
            break;
          }
          src.b_unicode = (*o_flags & 0x01) != 0;
          wb.vec_sst.push_back(
              read_xl_string(src, ku2_cch, *o_flags));
        }
        break;
      }
      case Record::kFont:
      case Record::kFontAlt: {
        if (sp.size() < 16) {
          break;
        }
        FontInfo font;
        font.u2_height = u2_at(sp, 0);
        font.u2_color = u2_at(sp, 4);
        font.u2_bold = u2_at(sp, 6);
        if (wb.b_biff5) {
          const std::uint8_t ku1_cch{u1_at(sp, 14)};
          if (sp.size() >= 15 + std::size_t{ku1_cch}) {
            font.str_name = core::decode_latin1_span(
                sp.subspan(15, ku1_cch));
          }
        } else {
          const std::uint8_t ku1_cch{u1_at(sp, 14)};
          const std::uint8_t ku1_flags{u1_at(sp, 15)};
          const std::size_t kst_chars{
              (ku1_flags & 0x01) != 0
                  ? std::size_t{ku1_cch} * 2
                  : std::size_t{ku1_cch}};
          if (sp.size() >= 16 + kst_chars) {
            font.str_name =
                (ku1_flags & 0x01) != 0
                    ? core::decode_utf16_span(sp.subspan(16, kst_chars))
                    : core::decode_latin1_span(sp.subspan(16, kst_chars));
          }
        }
        wb.vec_fonts.push_back(std::move(font));
        break;
      }
      case Record::kFormat: {
        if (sp.size() < 4) {
          break;
        }
        FormatInfo format;
        format.u2_index = u2_at(sp, 0);
        if (wb.b_biff5) {
          const std::uint8_t ku1_cch{u1_at(sp, 2)};
          if (sp.size() >= 3 + std::size_t{ku1_cch}) {
            format.str_value = core::decode_latin1_span(
                sp.subspan(3, ku1_cch));
          }
        } else {
          const std::uint16_t ku2_cch{u2_at(sp, 2)};
          if (sp.size() >= 5) {
            const std::uint8_t ku1_flags{u1_at(sp, 4)};
            const std::size_t kst_chars{
                (ku1_flags & 0x01) != 0
                    ? std::size_t{ku2_cch} * 2
                    : std::size_t{ku2_cch}};
            if (sp.size() >= 5 + kst_chars) {
              format.str_value =
                  (ku1_flags & 0x01) != 0
                      ? core::decode_utf16_span(sp.subspan(5, kst_chars))
                      : core::decode_latin1_span(sp.subspan(5, kst_chars));
            }
          }
        }
        wb.vec_formats.push_back(std::move(format));
        break;
      }
      case Record::kXf: {
        if (sp.size() < 4) {
          break;
        }
        XfInfo xf;
        xf.u2_font = u2_at(sp, 0);
        xf.u2_format = u2_at(sp, 2);
        wb.vec_xfs.push_back(xf);
        break;
      }
      default:
        break;
    }
  }
  return {};
}

}

auto open_workbook_file(std::string_view str_path) -> core::Result<Workbook> {
  auto res_data{core::read_file_bytes(str_path)};
  if (!res_data) {
    return std::unexpected{res_data.error()};
  }
  return open_workbook_buffer(*res_data);
}

auto open_workbook_buffer(std::span<const std::byte> sp_data)
    -> core::Result<Workbook> {
  auto res_ole{open_ole_buffer(sp_data)};
  if (!res_ole) {
    return std::unexpected{res_ole.error()};
  }
  auto res_stream{ole_read_workbook_stream(*res_ole)};
  if (!res_stream) {
    return std::unexpected{res_stream.error()};
  }
  Workbook wb;
  wb.vec_stream = std::move(*res_stream);
  auto res_globals{parse_globals(wb)};
  if (!res_globals) {
    return std::unexpected{res_globals.error()};
  }
  return std::move(wb);
}

auto parse_worksheet(const Workbook& wb, std::size_t st_idx)
    -> core::Result<Worksheet> {
  if (st_idx >= wb.vec_sheets.size()) {
    return core::fail(core::ErrorCode::kParse, "worksheet index out of range");
  }
  const std::uint32_t ku4_pos{wb.vec_sheets[st_idx].u4_filepos};
  if (ku4_pos >= wb.vec_stream.size()) {
    return core::fail(core::ErrorCode::kParse, "worksheet offset out of range");
  }
  const std::vector<RecView> vec_recs{
      walk_records(wb.vec_stream, ku4_pos, true)};
  if (vec_recs.empty() || vec_recs.front().e_id != Record::kBof) {
    return core::fail(core::ErrorCode::kParse, "worksheet bof missing");
  }

  std::uint16_t ku2_maxrow{0};
  std::uint16_t ku2_maxcol{0};
  for (const RecView& rec : vec_recs) {
    const std::span<const std::byte> sp{rec.sp_body};
    if (rec.e_id == Record::kRow) {
      if (sp.size() >= 2) {
        ku2_maxrow = std::max(ku2_maxrow, u2_at(sp, 0));
      }
    } else if (is_cell_record(rec.e_id) && sp.size() >= 4) {
      ku2_maxrow = std::max(ku2_maxrow, u2_at(sp, 0));
      if (rec.e_id == Record::kMulRk || rec.e_id == Record::kMulBlank) {
        if (sp.size() >= 6) {
          ku2_maxcol = std::max(ku2_maxcol, u2_at(sp, sp.size() - 2));
        }
      } else {
        ku2_maxcol = std::max(ku2_maxcol, u2_at(sp, 2));
      }
    } else if (rec.e_id == Record::kMergedCells && sp.size() >= 2) {
      const std::uint16_t ku2_count{u2_at(sp, 0)};
      for (std::uint16_t u2_n{0}; u2_n < ku2_count; ++u2_n) {
        const std::size_t kst_off{2 + std::size_t{u2_n} * 8};
        if (kst_off + 8 > sp.size()) {
          break;
        }
        const std::uint16_t ku2_r2{u2_at(sp, kst_off + 2)};
        const std::uint16_t ku2_c2{u2_at(sp, kst_off + 6)};
        const std::uint16_t ku2_r1{u2_at(sp, kst_off)};
        const std::uint16_t ku2_c1{u2_at(sp, kst_off + 4)};
        if (ku2_r2 >= ku2_r1 && ku2_c2 >= ku2_c1) {
          ku2_maxrow = std::max(ku2_maxrow, ku2_r2);
          ku2_maxcol = std::max(ku2_maxcol, ku2_c2);
        }
      }
    }
  }

  Worksheet ws;
  ws.u2_maxcol = ku2_maxcol;
  const auto fn_store{
      [&](std::uint16_t ku2_row, std::uint16_t ku2_col) -> Cell* {
        if (ku2_row > ku2_maxrow || ku2_col > ku2_maxcol) {
          return nullptr;
        }
        if (ku2_row >= ws.vec_rows.size()) {
          ws.vec_rows.resize(ku2_row + 1);
        }
        std::vector<Cell>& vec_cells{ws.vec_rows[ku2_row].vec_cells};
        if (vec_cells.empty() || vec_cells.back().u2_col < ku2_col) {
          Cell& cell{vec_cells.emplace_back()};
          cell.u2_row = ku2_row;
          cell.u2_col = ku2_col;
          return &cell;
        }
        const auto it_find{std::lower_bound(
            vec_cells.begin(), vec_cells.end(), ku2_col,
            [](const Cell& cell, std::uint16_t ku2_key) {
              return cell.u2_col < ku2_key;
            })};
        if (it_find != vec_cells.end() && it_find->u2_col == ku2_col) {
          return &*it_find;
        }
        Cell& cell{*vec_cells.insert(it_find, Cell{})};
        cell.u2_row = ku2_row;
        cell.u2_col = ku2_col;
        return &cell;
      }};

  const auto fn_find{[&](std::uint16_t ku2_row, std::uint16_t ku2_col) -> Cell* {
    if (ku2_row >= ws.vec_rows.size()) {
      return nullptr;
    }
    std::vector<Cell>& vec_cells{ws.vec_rows[ku2_row].vec_cells};
    const auto it_find{std::lower_bound(
        vec_cells.begin(), vec_cells.end(), ku2_col,
        [](const Cell& cell, std::uint16_t ku2_key) {
          return cell.u2_col < ku2_key;
        })};
    if (it_find != vec_cells.end() && it_find->u2_col == ku2_col) {
      return &*it_find;
    }
    return nullptr;
  }};

  bool b_pending_string{false};
  std::uint16_t ku2_pending_row{0};
  std::uint16_t ku2_pending_col{0};

  for (const RecView& rec : vec_recs) {
    const std::span<const std::byte> sp{rec.sp_body};
    switch (rec.e_id) {
      case Record::kNumber: {
        if (sp.size() < 14) {
          break;
        }
        if (Cell* p{fn_store(u2_at(sp, 0), u2_at(sp, 2))}) {
          p->e_id = Record::kNumber;
          p->u2_xf = u2_at(sp, 4);
          p->fp8_value = f8_at(sp, 6);
        }
        break;
      }
      case Record::kRk: {
        if (sp.size() < 10) {
          break;
        }
        if (Cell* p{fn_store(u2_at(sp, 0), u2_at(sp, 2))}) {
          p->e_id = Record::kRk;
          p->u2_xf = u2_at(sp, 4);
          p->fp8_value = rk_to_double(u4_at(sp, 6));
        }
        break;
      }
      case Record::kMulRk: {
        if (sp.size() < 6) {
          break;
        }
        const std::uint16_t ku2_row{u2_at(sp, 0)};
        const std::uint16_t ku2_first{u2_at(sp, 2)};
        const std::uint16_t ku2_last{u2_at(sp, sp.size() - 2)};
        for (std::uint16_t ku2_c{ku2_first}; ku2_c <= ku2_last; ++ku2_c) {
          const std::size_t kst_off{4 + (std::size_t{ku2_c} - ku2_first) * 6};
          if (kst_off + 6 > sp.size() - 2) {
            break;
          }
          if (Cell* p{fn_store(ku2_row, ku2_c)}) {
            p->e_id = Record::kRk;
            p->u2_xf = u2_at(sp, kst_off);
            p->fp8_value = rk_to_double(u4_at(sp, kst_off + 2));
          }
        }
        break;
      }
      case Record::kMulBlank: {
        if (sp.size() < 6) {
          break;
        }
        const std::uint16_t ku2_row{u2_at(sp, 0)};
        const std::uint16_t ku2_first{u2_at(sp, 2)};
        const std::uint16_t ku2_last{u2_at(sp, sp.size() - 2)};
        for (std::uint16_t ku2_c{ku2_first}; ku2_c <= ku2_last; ++ku2_c) {
          const std::size_t kst_off{4 + (std::size_t{ku2_c} - ku2_first) * 2};
          if (kst_off + 2 > sp.size() - 2) {
            break;
          }
          if (Cell* p{fn_store(ku2_row, ku2_c)}) {
            p->e_id = Record::kBlank;
            p->u2_xf = u2_at(sp, kst_off);
          }
        }
        break;
      }
      case Record::kLabelSst: {
        if (sp.size() < 10) {
          break;
        }
        const std::uint32_t ku4_idx{u4_at(sp, 6)};
        if (Cell* p{fn_store(u2_at(sp, 0), u2_at(sp, 2))}) {
          p->e_id = Record::kLabelSst;
          p->u2_xf = u2_at(sp, 4);
          p->u4_sst = ku4_idx;
        }
        break;
      }
      case Record::kLabel:
      case Record::kRString: {
        if (sp.size() < 8) {
          break;
        }
        if (Cell* p{fn_store(u2_at(sp, 0), u2_at(sp, 2))}) {
          p->e_id = Record::kLabel;
          p->u2_xf = u2_at(sp, 4);
          const std::uint16_t ku2_cch{u2_at(sp, 6)};
          if (wb.b_biff5) {
            if (sp.size() >= 8 + std::size_t{ku2_cch}) {
              p->opt_str = core::decode_latin1_span(
                  sp.subspan(8, ku2_cch));
            }
          } else if (sp.size() < 9) {
            if (ku2_cch == 0) {
              p->opt_str = std::string{};
            }
          } else {
            const std::uint8_t ku1_flags{u1_at(sp, 8)};
            const std::size_t kst_chars{
                (ku1_flags & 0x01) != 0
                    ? std::size_t{ku2_cch} * 2
                    : std::size_t{ku2_cch}};
            if (sp.size() >= 9 + kst_chars) {
              p->opt_str =
                  (ku1_flags & 0x01) != 0
                      ? core::decode_utf16_span(
                            sp.subspan(9, kst_chars))
                      : core::decode_latin1_span(
                            sp.subspan(9, kst_chars));
            }
          }
        }
        break;
      }
      case Record::kBoolErr: {
        if (sp.size() < 8) {
          break;
        }
        if (Cell* p{fn_store(u2_at(sp, 0), u2_at(sp, 2))}) {
          p->e_id = Record::kBoolErr;
          p->u2_xf = u2_at(sp, 4);
          p->b_err = u1_at(sp, 7) != 0;
          if (p->b_err) {
            p->u1_errcode = u1_at(sp, 6);
          } else {
            p->b_bool = true;
            p->fp8_value = u1_at(sp, 6) != 0 ? 1.0 : 0.0;
          }
        }
        break;
      }
      case Record::kFormula:
      case Record::kFormulaAlt: {
        if (sp.size() < 16) {
          break;
        }
        if (Cell* p{fn_store(u2_at(sp, 0), u2_at(sp, 2))}) {
          p->e_id = Record::kFormula;
          p->u2_xf = u2_at(sp, 4);
          if (u2_at(sp, 12) == 0xFFFF) {
            const std::uint8_t ku1_kind{u1_at(sp, 6)};
            if (ku1_kind == 0) {
              b_pending_string = true;
              ku2_pending_row = u2_at(sp, 0);
              ku2_pending_col = u2_at(sp, 2);
            } else if (ku1_kind == 1) {
              p->b_bool = true;
              p->fp8_value = (u1_at(sp, 7) & 0x01) != 0 ? 1.0 : 0.0;
            } else {
              p->b_err = true;
              p->u1_errcode = u1_at(sp, 7);
            }
          } else {
            p->fp8_value = f8_at(sp, 6);
          }
        }
        break;
      }
      case Record::kString: {
        if (b_pending_string && sp.size() >= 2) {
          const std::uint16_t ku2_cch{u2_at(sp, 0)};
          Cell* p_str{fn_find(ku2_pending_row, ku2_pending_col)};
          if (p_str != nullptr) {
            if (wb.b_biff5) {
              if (sp.size() >= 2 + std::size_t{ku2_cch}) {
                p_str->opt_str = core::decode_latin1_span(
                    sp.subspan(2, ku2_cch));
              }
            } else if (sp.size() < 3) {
              if (ku2_cch == 0) {
                p_str->opt_str = std::string{};
              }
            } else {
              const std::uint8_t ku1_flags{u1_at(sp, 2)};
              const std::size_t kst_chars{
                  (ku1_flags & 0x01) != 0
                      ? std::size_t{ku2_cch} * 2
                      : std::size_t{ku2_cch}};
              if (sp.size() >= 3 + kst_chars) {
                p_str->opt_str =
                    (ku1_flags & 0x01) != 0
                        ? core::decode_utf16_span(
                              sp.subspan(3, kst_chars))
                        : core::decode_latin1_span(
                              sp.subspan(3, kst_chars));
              }
            }
          }
        }
        b_pending_string = false;
        break;
      }
      case Record::kDefColWidth: {
        if (sp.size() >= 2) {
          ws.u2_defcolwidth = u2_at(sp, 0);
        }
        break;
      }
      case Record::kColInfo: {
        if (sp.size() >= 12) {
          ws.vec_colinfo.push_back(ColInfoRec{u2_at(sp, 0), u2_at(sp, 2),
                                              u2_at(sp, 4), u2_at(sp, 6),
                                              u2_at(sp, 8)});
        }
        break;
      }
      case Record::kMergedCells: {
        if (sp.size() < 2) {
          break;
        }
        const std::uint16_t ku2_count{u2_at(sp, 0)};
        for (std::uint16_t u2_n{0}; u2_n < ku2_count; ++u2_n) {
          const std::size_t kst_off{2 + std::size_t{u2_n} * 8};
          if (kst_off + 8 > sp.size()) {
            break;
          }
          const std::uint16_t ku2_r1{u2_at(sp, kst_off)};
          const std::uint16_t ku2_r2{u2_at(sp, kst_off + 2)};
          const std::uint16_t ku2_c1{u2_at(sp, kst_off + 4)};
          const std::uint16_t ku2_c2{u2_at(sp, kst_off + 6)};
          if (ku2_r2 < ku2_r1 || ku2_c2 < ku2_c1) {
            continue;
          }
          if (Cell* p{fn_store(ku2_r1, ku2_c1)}) {
            p->u2_rowspan = ku2_r2 - ku2_r1 + 1;
            p->u2_colspan = ku2_c2 - ku2_c1 + 1;
          }
          for (std::uint16_t ku2_r{ku2_r1}; ku2_r <= ku2_r2; ++ku2_r) {
            for (std::uint16_t ku2_c{ku2_c1}; ku2_c <= ku2_c2; ++ku2_c) {
              if (ku2_r == ku2_r1 && ku2_c == ku2_c1) {
                continue;
              }
              if (Cell* p{fn_store(ku2_r, ku2_c)}) {
                p->b_hidden = true;
              }
            }
          }
        }
        break;
      }
      default:
        break;
    }
  }

  if (ws.vec_rows.size() <= ku2_maxrow) {
    ws.vec_rows.resize(static_cast<std::size_t>(ku2_maxrow) + 1);
  }
  for (std::size_t st_r{0}; st_r < ws.vec_rows.size(); ++st_r) {
    ws.vec_rows[st_r].u2_index = static_cast<std::uint16_t>(st_r);
  }
  return ws;
}

}
