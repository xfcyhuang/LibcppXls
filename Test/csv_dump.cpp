import std;

#include "reader/biff.hpp"

namespace {

auto quoted_text(std::string_view str_v) -> std::string {
  std::string str_out{'"'};
  str_out += str_v;
  str_out += '"';
  return str_out;
}

auto cell_text(const cppxls::reader::Workbook& wb,
               const cppxls::reader::Cell& cell) -> std::string {
  if (cell.e_id == cppxls::reader::Record::kLabelSst) {
    if (cell.u4_sst < wb.vec_sst.size()) {
      return quoted_text(wb.vec_sst[cell.u4_sst]);
    }
    return {};
  }
  if (cell.opt_str) {
    return quoted_text(*cell.opt_str);
  }
  if (cell.b_err) {
    return "#ERR";
  }
  if (cell.b_bool) {
    return cell.fp8_value != 0.0 ? "true" : "false";
  }
  if (cell.e_id == cppxls::reader::Record::kBlank) {
    return {};
  }
  std::array<char, 48> arr_buf{};
  const auto res_fmt{
      std::to_chars(arr_buf.data(), arr_buf.data() + arr_buf.size(),
                    cell.fp8_value)};
  if (res_fmt.ec != std::errc{}) {
    return "#FMT";
  }
  return std::string{arr_buf.data(),
                     static_cast<std::size_t>(res_fmt.ptr - arr_buf.data())};
}

}

auto main(int argc, char** argv) -> int {
  if (argc < 2) {
    std::println("usage: xls_csv_dump <file.xls>");
    return 1;
  }
  const auto res_wb{cppxls::reader::open_workbook_file(argv[1])};
  if (!res_wb) {
    std::println("error: {} ({})", res_wb.error().str_message,
                 cppxls::core::to_string(res_wb.error().u1_code));
    return 1;
  }
  const cppxls::reader::Workbook& wb{*res_wb};
  std::println("sheets: {} biff5: {} codepage: {}", wb.vec_sheets.size(),
               wb.b_biff5, wb.u2_codepage);
  for (std::size_t st_i{0}; st_i < wb.vec_sheets.size(); ++st_i) {
    const auto res_ws{cppxls::reader::parse_worksheet(wb, st_i)};
    if (!res_ws) {
      std::println("=== {} (parse failed: {})", wb.vec_sheets[st_i].str_name,
                   res_ws.error().str_message);
      continue;
    }
    std::println("=== {}", wb.vec_sheets[st_i].str_name);
    const std::size_t kst_cols{
        static_cast<std::size_t>(res_ws->u2_maxcol) + 1};
    for (const cppxls::reader::RowData& row : res_ws->vec_rows) {
      std::string str_line{};
      std::size_t st_c{0};
      for (const cppxls::reader::Cell& cell : row.vec_cells) {
        while (st_c < cell.u2_col) {
          if (st_c > 0) {
            str_line += ';';
          }
          ++st_c;
        }
        if (st_c > 0) {
          str_line += ';';
        }
        str_line += cell_text(wb, cell);
        ++st_c;
      }
      while (st_c < kst_cols) {
        if (st_c > 0) {
          str_line += ';';
        }
        ++st_c;
      }
      std::println("{}", str_line);
    }
  }
  return 0;
}
