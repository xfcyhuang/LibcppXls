#pragma once

import std;

namespace cppxls::core {

enum class ErrorCode : std::uint8_t {
  kOk,
  kOpen,
  kRead,
  kParse,
  kOutOfMemory,
  kUnsupportedEncryption,
  kNullArgument,
  kStreamNotFound,
};

struct Error {
  ErrorCode u1_code{ErrorCode::kOk};
  std::string str_message;
};

template <typename T>
using Result = std::expected<T, Error>;

[[nodiscard]] inline auto fail(ErrorCode e_code, std::string_view str_msg)
    -> std::unexpected<Error> {
  return std::unexpected<Error>{Error{e_code, std::string{str_msg}}};
}

[[nodiscard]] constexpr auto to_string(ErrorCode e_code) -> std::string_view {
  using enum ErrorCode;
  switch (e_code) {
    case kOk: return "no error";
    case kOpen: return "unable to open file";
    case kRead: return "unable to read from file";
    case kParse: return "unable to parse file";
    case kOutOfMemory: return "out of memory";
    case kUnsupportedEncryption: return "unsupported encryption scheme";
    case kNullArgument: return "null argument";
    case kStreamNotFound: return "ole stream not found";
  }
  std::unreachable();
}

}
