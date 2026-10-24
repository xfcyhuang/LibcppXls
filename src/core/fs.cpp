import std;

#include "core/fs.hpp"

namespace cppxls::core {

auto read_file_bytes(const std::filesystem::path& pth_file)
    -> Result<std::vector<std::byte>> {
  std::ifstream fs_in{pth_file, std::ios::binary};
  if (!fs_in) {
    return fail(ErrorCode::kOpen, "unable to open file");
  }
  fs_in.seekg(0, std::ios::end);
  const std::streamoff kst_len{fs_in.tellg()};
  fs_in.seekg(0, std::ios::beg);
  if (kst_len < 0) {
    return fail(ErrorCode::kRead, "unable to stat file");
  }
  std::vector<std::byte> vec_data(static_cast<std::size_t>(kst_len));
  if (kst_len > 0) {
    fs_in.read(reinterpret_cast<char*>(vec_data.data()), kst_len);
    if (fs_in.gcount() != kst_len) {
      return fail(ErrorCode::kRead, "short read");
    }
  }
  return vec_data;
}

}
