import std;

#include "reader/biff.hpp"
#include "reader/ole.hpp"
#include "synth.hpp"

namespace {

using cppxls::test::ByteBuilder;
using cppxls::test::bof_globals;
using cppxls::test::bof_sheet;
using cppxls::test::boundsheet;
using cppxls::test::default_spec;
using cppxls::test::eof_rec;
using cppxls::test::join;
using cppxls::test::make_ole;
using cppxls::test::rec;
using cppxls::test::utf16le_bytes;

using cppxls::test::pad_rec;

constexpr std::size_t kStPad{4200};

auto globals_then(std::vector<std::byte> vec_tail) -> std::vector<std::byte> {
  const std::uint32_t ku4_pos{static_cast<std::uint32_t>(
      bof_globals().size() + 13 + 4 + (kStPad + 4))};
  return join({bof_globals(), boundsheet(ku4_pos), eof_rec(), pad_rec(kStPad),
               std::move(vec_tail)});
}

auto open_wb(const std::vector<std::byte>& vec_ole)
    -> cppxls::reader::Workbook {
  auto res{cppxls::reader::open_workbook_buffer(vec_ole)};
  if (!res) {
    std::println("open_workbook_buffer failed: {}", res.error().str_message);
    std::exit(1);
  }
  return std::move(*res);
}

auto sheet_cell(const cppxls::reader::Worksheet& ws, std::uint16_t u2_row,
                std::uint16_t u2_col) -> const cppxls::reader::Cell* {
  if (u2_row >= ws.vec_rows.size()) {
    return nullptr;
  }
  for (const cppxls::reader::Cell& cell : ws.vec_rows[u2_row].vec_cells) {
    if (cell.u2_col == u2_col) {
      return &cell;
    }
    if (cell.u2_col > u2_col) {
      return nullptr;
    }
  }
  return nullptr;
}

auto case_label_oob() -> int {
  const std::vector<std::byte> vec_stream{globals_then(join(
      {bof_sheet(), rec(0x0204, ByteBuilder().u2(0).u2(0).u2(0).u2(0).take())}))};
  const cppxls::reader::Workbook wb{open_wb(make_ole(vec_stream, default_spec()))};
  auto res{cppxls::reader::parse_worksheet(wb, 0)};
  if (!res) {
    std::println("parse failed: {}", res.error().str_message);
    return 1;
  }
  const cppxls::reader::Cell* p{sheet_cell(*res, 0, 0)};
  if (p == nullptr) {
    std::println("cell (0,0) missing");
    return 1;
  }
  return 0;
}

auto case_font_oob() -> int {
  ByteBuilder font;
  for (std::size_t st_i{0}; st_i < 7; ++st_i) {
    font.u2(0);
  }
  font.u1(100).u1(1);
  for (std::size_t st_i{0}; st_i < 100; ++st_i) {
    font.u1(0);
  }
  const std::vector<std::byte> vec_stream{
      join({bof_globals(), pad_rec(kStPad), rec(0x0231, font.take())})};
  const cppxls::reader::Workbook wb{open_wb(make_ole(vec_stream, default_spec()))};
  if (wb.vec_fonts.size() != 1 || !wb.vec_fonts[0].str_name.empty()) {
    std::println("font count {} name {}", wb.vec_fonts.size(),
                 wb.vec_fonts.empty() ? "?" : wb.vec_fonts[0].str_name);
    return 1;
  }
  return 0;
}

auto case_boundsheet_oob() -> int {
  ByteBuilder bs;
  bs.u4(0).u1(0).u1(0).u1(100).u1(1);
  for (std::size_t st_i{0}; st_i < 100; ++st_i) {
    bs.u1(0);
  }
  const std::vector<std::byte> vec_stream{
      join({bof_globals(), pad_rec(kStPad), rec(0x0085, bs.take())})};
  const cppxls::reader::Workbook wb{open_wb(make_ole(vec_stream, default_spec()))};
  if (wb.vec_sheets.size() != 1 || !wb.vec_sheets[0].str_name.empty()) {
    std::println("sheet count {}", wb.vec_sheets.size());
    return 1;
  }
  return 0;
}

auto case_format_oob() -> int {
  ByteBuilder fmt;
  fmt.u2(0).u2(100).u1(1);
  for (std::size_t st_i{0}; st_i < 100; ++st_i) {
    fmt.u1(0);
  }
  const std::vector<std::byte> vec_stream{
      join({bof_globals(), pad_rec(kStPad), rec(0x041E, fmt.take())})};
  const cppxls::reader::Workbook wb{open_wb(make_ole(vec_stream, default_spec()))};
  if (wb.vec_formats.size() != 1 || !wb.vec_formats[0].str_value.empty()) {
    std::println("format count {}", wb.vec_formats.size());
    return 1;
  }
  return 0;
}

auto case_sst_cont_flag() -> int {
  ByteBuilder sst;
  sst.u4(1).u4(1).u2(8).u1(0).raw("ABCD");
  ByteBuilder cont;
  cont.u1(0x08).raw("EFGH");
  const std::vector<std::byte> vec_stream{join(
      {bof_globals(), pad_rec(kStPad), boundsheet(4229), rec(0x00FC, sst.take()),
       rec(0x003C, cont.take()), eof_rec()})};
  const cppxls::reader::Workbook wb{open_wb(make_ole(vec_stream, default_spec()))};
  if (wb.vec_sst.size() != 1 || wb.vec_sst[0] != "ABCDEFGH") {
    std::println("sst size {} [0]={}", wb.vec_sst.size(),
                 wb.vec_sst.empty() ? "?" : wb.vec_sst[0]);
    return 1;
  }
  return 0;
}

auto case_dir_surrogate() -> int {
  cppxls::test::OleSpec spec{default_spec()};
  spec.vec_extra_name = ByteBuilder().u2(0xD801).u2(0xDC02).u2(0).take();
  const std::vector<std::byte> vec_ole{
      make_ole(bof_globals(), spec)};
  auto res{cppxls::reader::open_ole_buffer(vec_ole)};
  if (!res) {
    std::println("ole open failed: {}", res.error().str_message);
    return 1;
  }
  for (const cppxls::reader::OleEntry& entry : res->vec_entries) {
    if (entry.str_name == "\xF0\x90\x90\x82") {
      return 0;
    }
  }
  std::print("names:");
  for (const cppxls::reader::OleEntry& entry : res->vec_entries) {
    std::print(" [{}]", entry.str_name);
  }
  std::println("");
  return 1;
}

auto case_formula_nan() -> int {
  const double kFpNan{
      std::bit_cast<double>(std::uint64_t{0xFFFF8000DEADBEEFULL})};
  ByteBuilder formula;
  formula.u2(0).u2(0).u2(0).f8(kFpNan).u2(0).u4(0).u2(0);
  const std::vector<std::byte> vec_stream{globals_then(join(
      {bof_sheet(), rec(0x0006, formula.take()), eof_rec()}))};
  const cppxls::reader::Workbook wb{open_wb(make_ole(vec_stream, default_spec()))};
  auto res{cppxls::reader::parse_worksheet(wb, 0)};
  if (!res) {
    std::println("parse failed: {}", res.error().str_message);
    return 1;
  }
  const cppxls::reader::Cell* p{sheet_cell(*res, 0, 0)};
  if (p == nullptr || !p->b_err || p->u1_errcode != 0xBE) {
    std::println("cell err={} code={}", p == nullptr ? -1 : p->b_err,
                 p == nullptr ? 0 : p->u1_errcode);
    return 1;
  }
  return 0;
}

auto case_masked_error() -> int {
  cppxls::test::OleSpec spec{default_spec()};
  spec.u4_start = 0x00ABCDEF;
  spec.b_size_from_stream = false;
  spec.u8_size = 5000;
  const std::vector<std::byte> vec_ole{make_ole(bof_globals(), spec)};
  auto res_ole{cppxls::reader::open_ole_buffer(vec_ole)};
  if (!res_ole) {
    std::println("ole open failed: {}", res_ole.error().str_message);
    return 1;
  }
  auto res{cppxls::reader::ole_read_workbook_stream(*res_ole)};
  if (res || res.error().u1_code != cppxls::core::ErrorCode::kParse) {
    std::println("error code {}", res ? 0 : static_cast<int>(res.error().u1_code));
    return 1;
  }
  return 0;
}

auto case_dims_extent() -> int {
  ByteBuilder dims;
  dims.u4(0).u4(5).u2(0).u2(3).u2(0);
  ByteBuilder number;
  number.u2(0).u2(0).u2(0).f8(1.5);
  const std::vector<std::byte> vec_stream{globals_then(join(
      {bof_sheet(), rec(0x0200, dims.take()), rec(0x0203, number.take()),
       eof_rec()}))};
  const cppxls::reader::Workbook wb{open_wb(make_ole(vec_stream, default_spec()))};
  auto res{cppxls::reader::parse_worksheet(wb, 0)};
  if (!res) {
    std::println("parse failed: {}", res.error().str_message);
    return 1;
  }
  if (res->vec_rows.size() != 1 || res->u2_maxcol != 0) {
    std::println("rows {} maxcol {}", res->vec_rows.size(), res->u2_maxcol);
    return 1;
  }
  return 0;
}

auto case_grid_sparse() -> int {
  ByteBuilder mul;
  mul.u2(99).u2(0);
  for (std::size_t st_i{0}; st_i < 256; ++st_i) {
    mul.u2(0);
  }
  mul.u2(255);
  const std::vector<std::byte> vec_stream{
      globals_then(join({bof_sheet(), rec(0x00BE, mul.take()), eof_rec()}))};
  const cppxls::reader::Workbook wb{open_wb(make_ole(vec_stream, default_spec()))};
  auto res{cppxls::reader::parse_worksheet(wb, 0)};
  if (!res) {
    std::println("parse failed: {}", res.error().str_message);
    return 1;
  }
  if (res->vec_rows.size() != 100 || !res->vec_rows[0].vec_cells.empty()
      || res->vec_rows[99].vec_cells.size() != 256) {
    std::println("rows {} row0 {} row99 {}", res->vec_rows.size(),
                 res->vec_rows.empty() ? 0 : res->vec_rows[0].vec_cells.size(),
                 res->vec_rows.size() > 99 ? res->vec_rows[99].vec_cells.size()
                                           : 0);
    return 1;
  }
  return 0;
}

auto case_merged_underflow() -> int {
  ByteBuilder anchor;
  anchor.u2(2).u2(0).u2(0).f8(1.5);
  ByteBuilder merged;
  merged.u2(1).u2(2).u2(0).u2(0).u2(1);
  const std::vector<std::byte> vec_stream{globals_then(join(
      {bof_sheet(), rec(0x0203, anchor.take()), rec(0x00E5, merged.take()),
       eof_rec()}))};
  const cppxls::reader::Workbook wb{open_wb(make_ole(vec_stream, default_spec()))};
  auto res{cppxls::reader::parse_worksheet(wb, 0)};
  if (!res) {
    std::println("parse failed: {}", res.error().str_message);
    return 1;
  }
  for (const cppxls::reader::RowData& row : res->vec_rows) {
    for (const cppxls::reader::Cell& cell : row.vec_cells) {
      if (cell.u2_rowspan > 1 || cell.u2_colspan > 1) {
        std::println("span r{} c{} rowspan {} colspan {}", row.u2_index,
                     cell.u2_col, cell.u2_rowspan, cell.u2_colspan);
        return 1;
      }
    }
  }
  return 0;
}

auto case_bof_check() -> int {
  ByteBuilder number;
  number.u2(0).u2(0).u2(0).f8(1.5);
  const std::vector<std::byte> vec_stream{globals_then(
      join({rec(0x0203, number.take()), eof_rec()}))};
  const cppxls::reader::Workbook wb{open_wb(make_ole(vec_stream, default_spec()))};
  auto res{cppxls::reader::parse_worksheet(wb, 0)};
  if (res) {
    std::println("parse unexpectedly ok, rows {}", res->vec_rows.size());
    return 1;
  }
  return 0;
}

auto case_sst_continue_oob() -> int {
  ByteBuilder sst;
  sst.u4(1).u4(1).u2(2).u1(0).raw("A");
  ByteBuilder c1;
  c1.u1(0);
  ByteBuilder c2;
  c2.u1(0).raw("B");
  const std::vector<std::byte> vec_stream{join(
      {bof_globals(), boundsheet(0), rec(0x00FC, sst.take()),
       rec(0x003C, c1.take()), rec(0x003C, c2.take()), eof_rec(),
       pad_rec(kStPad)})};
  const cppxls::reader::Workbook wb{open_wb(make_ole(vec_stream, default_spec()))};
  if (wb.vec_sst.size() != 1 || wb.vec_sst[0] != "A") {
    std::println("sst size {} [0]={}", wb.vec_sst.size(),
                 wb.vec_sst.empty() ? "?" : wb.vec_sst[0]);
    return 1;
  }
  return 0;
}

auto case_biff5_formula_string() -> int {
  ByteBuilder bof5g;
  bof5g.u2(0x0500).u2(0x0005);
  ByteBuilder bof5s;
  bof5s.u2(0x0500).u2(0x0010);
  ByteBuilder formula;
  formula.u2(0).u2(0).u2(0);
  formula.u1(0).u1(0).u1(0).u1(0).u1(0).u1(0).u1(0xFF).u1(0xFF);
  formula.u2(0).u4(0).u2(0);
  ByteBuilder str;
  str.u2(2).raw("AB");
  const std::uint32_t ku4_pos{25};
  const std::vector<std::byte> vec_stream{join(
      {rec(0x0809, bof5g.take()), boundsheet(ku4_pos), eof_rec(),
       rec(0x0809, bof5s.take()), rec(0x0006, formula.take()),
       rec(0x0207, str.take()), eof_rec(), pad_rec(kStPad)})};
  const cppxls::reader::Workbook wb{open_wb(make_ole(vec_stream, default_spec()))};
  auto res{cppxls::reader::parse_worksheet(wb, 0)};
  if (!res) {
    std::println("parse failed: {}", res.error().str_message);
    return 1;
  }
  const cppxls::reader::Cell* p{sheet_cell(*res, 0, 0)};
  if (p == nullptr || !p->opt_str || *p->opt_str != "AB") {
    std::println("cell string {}",
                 p != nullptr && p->opt_str ? *p->opt_str
                                            : std::string{"<none>"});
    return 1;
  }
  return 0;
}

auto case_datemode() -> int {
  ByteBuilder dm;
  dm.u2(1);
  const std::vector<std::byte> vec_stream{join(
      {bof_globals(), rec(0x0022, dm.take()), boundsheet(0), eof_rec(),
       pad_rec(kStPad)})};
  const cppxls::reader::Workbook wb{open_wb(make_ole(vec_stream, default_spec()))};
  if (!wb.b_1904) {
    std::println("b_1904 false");
    return 1;
  }
  return 0;
}

auto case_hostile_fat() -> int {
  std::vector<std::byte> vec_ole(512, std::byte{0});
  const std::array<std::uint8_t, 8> karr_sig{0xD0, 0xCF, 0x11, 0xE0,
                                             0xA1, 0xB1, 0x1A, 0xE1};
  for (std::size_t st_i{0}; st_i < karr_sig.size(); ++st_i) {
    vec_ole[st_i] = static_cast<std::byte>(karr_sig[st_i]);
  }
  vec_ole[30] = static_cast<std::byte>(9);
  vec_ole[32] = static_cast<std::byte>(6);
  vec_ole[68] = static_cast<std::byte>(0xFE);
  vec_ole[69] = static_cast<std::byte>(0xFF);
  vec_ole[70] = static_cast<std::byte>(0xFF);
  vec_ole[71] = static_cast<std::byte>(0xFF);
  const std::uint32_t ku4_fat{0x40000000};
  vec_ole[44] = static_cast<std::byte>(ku4_fat);
  vec_ole[45] = static_cast<std::byte>(ku4_fat >> 8);
  vec_ole[46] = static_cast<std::byte>(ku4_fat >> 16);
  vec_ole[47] = static_cast<std::byte>(ku4_fat >> 24);
  auto res{cppxls::reader::open_ole_buffer(vec_ole)};
  if (res || res.error().u1_code != cppxls::core::ErrorCode::kParse) {
    std::println("error code {}", res ? 0 : static_cast<int>(res.error().u1_code));
    return 1;
  }
  return 0;
}

}

auto main(int argc, char** argv) -> int {
  if (argc != 2) {
    std::println("usage: xls_unit <case>");
    return 2;
  }
  const std::string_view str_case{argv[1]};
  if (str_case == "label_oob") {
    return case_label_oob();
  }
  if (str_case == "font_oob") {
    return case_font_oob();
  }
  if (str_case == "boundsheet_oob") {
    return case_boundsheet_oob();
  }
  if (str_case == "format_oob") {
    return case_format_oob();
  }
  if (str_case == "sst_cont_flag") {
    return case_sst_cont_flag();
  }
  if (str_case == "dir_surrogate") {
    return case_dir_surrogate();
  }
  if (str_case == "formula_nan") {
    return case_formula_nan();
  }
  if (str_case == "masked_error") {
    return case_masked_error();
  }
  if (str_case == "dims_extent") {
    return case_dims_extent();
  }
  if (str_case == "grid_sparse") {
    return case_grid_sparse();
  }
  if (str_case == "merged_underflow") {
    return case_merged_underflow();
  }
  if (str_case == "bof_check") {
    return case_bof_check();
  }
  if (str_case == "hostile_fat") {
    return case_hostile_fat();
  }
  if (str_case == "sst_continue_oob") {
    return case_sst_continue_oob();
  }
  if (str_case == "biff5_formula_string") {
    return case_biff5_formula_string();
  }
  if (str_case == "datemode") {
    return case_datemode();
  }
  std::println("unknown case: {}", str_case);
  return 2;
}
