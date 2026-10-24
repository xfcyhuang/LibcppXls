#pragma once

import std;

#include "core/error.hpp"

namespace cppxls::reader {

enum class Record : std::uint16_t {
  kFormula = 0x0006,
  kEof = 0x000A,
  kFilePass = 0x002F,
  kContinue = 0x003C,
  kWindow1 = 0x003D,
  kCodepage = 0x0042,
  kFont = 0x0031,
  kFontAlt = 0x0231,
  kDefColWidth = 0x0055,
  kColInfo = 0x007D,
  kBoundsheet = 0x0085,
  kMergedCells = 0x00E5,
  kMulRk = 0x00BD,
  kMulBlank = 0x00BE,
  kRString = 0x00D6,
  kXf = 0x00E0,
  kSst = 0x00FC,
  kLabelSst = 0x00FD,
  kBlank = 0x0201,
  kNumber = 0x0203,
  kLabel = 0x0204,
  kBoolErr = 0x0205,
  kString = 0x0207,
  kRow = 0x0208,
  kFormat = 0x041E,
  kRk = 0x027E,
  kFormulaAlt = 0x0406,
  kBof = 0x0809,
};

constexpr std::uint32_t kU4SstNone{0xFFFFFFFF};

struct SheetInfo {
  std::string str_name;
  std::uint32_t u4_filepos{0};
  std::uint8_t u1_type{0};
  std::uint8_t u1_visibility{0};
};

struct FontInfo {
  std::string str_name;
  std::uint16_t u2_height{0};
  std::uint16_t u2_bold{0};
  std::uint16_t u2_color{0};
};

struct FormatInfo {
  std::uint16_t u2_index{0};
  std::string str_value;
};

struct XfInfo {
  std::uint16_t u2_font{0};
  std::uint16_t u2_format{0};
};

struct Cell {
  Record e_id{Record::kBlank};
  std::uint16_t u2_row{0};
  std::uint16_t u2_col{0};
  std::uint16_t u2_xf{0};
  std::optional<std::string> opt_str;
  std::uint32_t u4_sst{kU4SstNone};
  double fp8_value{0.0};
  std::uint16_t u2_colspan{0};
  std::uint16_t u2_rowspan{0};
  bool b_hidden{false};
  bool b_bool{false};
  bool b_err{false};
  std::uint8_t u1_errcode{0};
};

struct RowData {
  std::uint16_t u2_index{0};
  std::vector<Cell> vec_cells;
};

struct ColInfoRec {
  std::uint16_t u2_first{0};
  std::uint16_t u2_last{0};
  std::uint16_t u2_width{0};
  std::uint16_t u2_xf{0};
  std::uint16_t u2_flags{0};
};

struct Worksheet {
  std::vector<RowData> vec_rows;
  std::vector<ColInfoRec> vec_colinfo;
  std::uint16_t u2_defcolwidth{0};
  std::uint16_t u2_maxcol{0};
};

struct Workbook {
  std::vector<std::byte> vec_stream;
  std::vector<SheetInfo> vec_sheets;
  std::vector<std::string> vec_sst;
  std::vector<FontInfo> vec_fonts;
  std::vector<FormatInfo> vec_formats;
  std::vector<XfInfo> vec_xfs;
  std::uint16_t u2_active_sheet{0};
  std::uint16_t u2_codepage{1252};
  bool b_1904{false};
  bool b_biff5{false};
};

[[nodiscard]] auto open_workbook_file(std::string_view str_path)
    -> core::Result<Workbook>;

[[nodiscard]] auto open_workbook_buffer(std::span<const std::byte> sp_data)
    -> core::Result<Workbook>;

[[nodiscard]] auto parse_worksheet(const Workbook& wb, std::size_t st_idx)
    -> core::Result<Worksheet>;

}
