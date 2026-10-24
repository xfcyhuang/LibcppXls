cmake_minimum_required(VERSION 3.24)

project(LibcppXls LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 21)
    message(FATAL_ERROR "LibcppXls requires Clang 21+, found "
            "${CMAKE_CXX_COMPILER_VERSION}")
  endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 16)
    message(FATAL_ERROR "LibcppXls requires GCC 16+, found "
            "${CMAKE_CXX_COMPILER_VERSION}")
  endif()
  message(WARNING "GCC toolchain: std.cppm discovery assumes llvm-mingw libc++ layout")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  message(FATAL_ERROR "LibcppXls does not support MSVC; "
          "use Clang 21+ (llvm-mingw) or GCC 16+")
endif()

option(CPPXLS_ASAN "address+undefined sanitizer orchestration" OFF)
option(CPPXLS_HARDENED "hardened flag-level orchestration" OFF)

set(CPPXLS_PRESET base)
set(CPPXLS_COMMON_FLAGS -fwrapv -fno-strict-aliasing -O2 -DNDEBUG
  -ffunction-sections -fdata-sections)
set(CPPXLS_WARN_FLAGS -Wall -Wextra -Wpedantic)
set(CPPXLS_LINK_FLAGS -Wl,--gc-sections)
set(CPPXLS_PCM_FLAGS -w ${CPPXLS_COMMON_FLAGS})
if(CPPXLS_ASAN)
  set(CPPXLS_PRESET asan)
  list(APPEND CPPXLS_COMMON_FLAGS -fsanitize=address,undefined
    -fno-omit-frame-pointer)
  list(APPEND CPPXLS_LINK_FLAGS -fsanitize=address,undefined)
  list(APPEND CPPXLS_PCM_FLAGS -fsanitize=address,undefined
    -fno-omit-frame-pointer)
elseif(CPPXLS_HARDENED)
  set(CPPXLS_PRESET hard)
  list(APPEND CPPXLS_COMMON_FLAGS -D_FORTIFY_SOURCE=3
    -fstack-protector-strong -fPIE)
  list(APPEND CPPXLS_PCM_FLAGS -fstack-protector-strong)
  list(APPEND CPPXLS_LINK_FLAGS -pie -Wl,--dynamicbase,--nxcompat)
endif()

get_filename_component(CPPXLS_BIN_DIR ${CMAKE_CXX_COMPILER} DIRECTORY)
find_file(CPPXLS_STD_CPPM std.cppm
  HINTS ${CPPXLS_BIN_DIR}/../share/libc++/v1
        ${CPPXLS_BIN_DIR}/../include/c++/v1
  REQUIRED)

set(CPPXLS_STD_PCM ${CMAKE_BINARY_DIR}/std-${CPPXLS_PRESET}.pcm)
add_custom_command(
  OUTPUT ${CPPXLS_STD_PCM}
  COMMAND ${CMAKE_CXX_COMPILER} -std=c++26 ${CPPXLS_PCM_FLAGS}
          --precompile ${CPPXLS_STD_CPPM} -o ${CPPXLS_STD_PCM}
  DEPENDS ${CPPXLS_STD_CPPM})
add_custom_target(cppxls_std_pcm DEPENDS ${CPPXLS_STD_PCM})

set(CPPXLS_COMPILE_FLAGS ${CPPXLS_WARN_FLAGS} ${CPPXLS_COMMON_FLAGS}
  -fmodule-file=std=${CPPXLS_STD_PCM})

add_library(cppxls STATIC
  src/core/bytes.cpp
  src/core/fs.cpp
  src/core/text.cpp
  src/reader/ole.cpp
  src/reader/biff.cpp)
add_dependencies(cppxls cppxls_std_pcm)
target_include_directories(cppxls PUBLIC src)
target_compile_options(cppxls PRIVATE ${CPPXLS_COMPILE_FLAGS})

add_executable(xls_csv_dump Test/csv_dump.cpp)
add_dependencies(xls_csv_dump cppxls_std_pcm)
target_link_libraries(xls_csv_dump PRIVATE cppxls)
target_compile_options(xls_csv_dump PRIVATE ${CPPXLS_COMPILE_FLAGS})
target_link_options(xls_csv_dump PRIVATE ${CPPXLS_LINK_FLAGS})

add_executable(xls_unit Test/unit.cpp)
add_dependencies(xls_unit cppxls_std_pcm)
target_link_libraries(xls_unit PRIVATE cppxls)
target_compile_options(xls_unit PRIVATE ${CPPXLS_COMPILE_FLAGS})
target_link_options(xls_unit PRIVATE ${CPPXLS_LINK_FLAGS})

enable_testing()

foreach(CPPXLS_CASE IN ITEMS label_oob font_oob boundsheet_oob format_oob
    sst_cont_flag dir_surrogate formula_nan masked_error dims_extent
    grid_sparse merged_underflow bof_check hostile_fat sst_continue_oob
    biff5_formula_string datemode)
  add_test(NAME ${CPPXLS_CASE} COMMAND xls_unit ${CPPXLS_CASE})
endforeach()

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(NAME cross_check_xlrd
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/Script/cross_check_xlrd.py)
  add_test(NAME cross_check_synth
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/Script/check_synth.py
            $<TARGET_FILE:xls_csv_dump>)
  set_tests_properties(cross_check_xlrd cross_check_synth PROPERTIES
    ENVIRONMENT PYTHONIOENCODING=utf-8
    SKIP_RETURN_CODE 77)
else()
  message(WARNING "Python3 not found, cross-check tests skipped")
endif()
