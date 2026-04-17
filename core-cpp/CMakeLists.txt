# core-cpp — Phase 1: hand-written runtime + reference TRANSFORM
#
# Targets:
#   oigtl_runtime  static lib, hand-written runtime (crc, header, ...)
#   oigtl_messages static lib, message codecs (Phase 1: TRANSFORM only)
#   oigtl          INTERFACE umbrella for downstream consumers
#   oigtl_tests    test binary (Phase 1: upstream TRANSFORM round-trip)
#
# Phase-2+ work will replace src/messages/*.cpp with codegen output.

cmake_minimum_required(VERSION 3.16)
project(oigtl_core CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Strict warnings — we want codegen output to compile cleanly.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror
                        -Wno-unused-parameter)
elseif(MSVC)
    add_compile_options(/W4 /WX)
endif()

# ---------------------------------------------------------------------------
# Runtime library — stdlib only, no external deps
# ---------------------------------------------------------------------------
add_library(oigtl_runtime STATIC
    src/runtime/crc64.cpp
    src/runtime/dispatch.cpp
    src/runtime/extended_header.cpp
    src/runtime/header.cpp
    src/runtime/metadata.cpp
    src/runtime/oracle.cpp
)
target_include_directories(oigtl_runtime PUBLIC include)

# ---------------------------------------------------------------------------
# Message library
# ---------------------------------------------------------------------------
# Glob the generated message sources. CONFIGURE_DEPENDS makes CMake
# rerun globbing on each build, so adding a new schema and
# regenerating doesn't require editing this file.
file(GLOB OIGTL_MESSAGE_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/src/messages/*.cpp")
add_library(oigtl_messages STATIC ${OIGTL_MESSAGE_SOURCES})
target_link_libraries(oigtl_messages PUBLIC oigtl_runtime)

# ---------------------------------------------------------------------------
# Umbrella target for downstream consumers
# ---------------------------------------------------------------------------
add_library(oigtl INTERFACE)
target_link_libraries(oigtl INTERFACE oigtl_runtime oigtl_messages)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()

set(IGTL_UPSTREAM_FIXTURES
    "${CMAKE_CURRENT_SOURCE_DIR}/../corpus-tools/reference-libs/openigtlink-upstream/Testing/igtlutil"
    CACHE PATH "Path to upstream igtl test-data headers")

add_executable(oigtl_tests
    tests/upstream_fixtures_test.cpp
)
target_link_libraries(oigtl_tests PRIVATE oigtl)
target_include_directories(oigtl_tests PRIVATE
    "${IGTL_UPSTREAM_FIXTURES}")

add_test(NAME upstream_fixtures COMMAND oigtl_tests)

# Cross-language parity test — shells out to the Python oracle via
# `uv run oigtl-corpus oracle verify --fixture ...` for every
# upstream fixture and asserts agreement. Skips at runtime if `uv`
# isn't on PATH, so it doesn't fail in environments without the
# Python tooling installed.
add_executable(oigtl_oracle_parity_test
    tests/oracle_parity_test.cpp
)
target_link_libraries(oigtl_oracle_parity_test PRIVATE oigtl)
target_include_directories(oigtl_oracle_parity_test PRIVATE
    "${IGTL_UPSTREAM_FIXTURES}")
add_test(NAME oracle_parity COMMAND oigtl_oracle_parity_test)
# The parity test invokes the corpus-tools CLI from a fixed
# location relative to the source tree; force the working
# directory so the relative path is stable regardless of where
# ctest is invoked from.
set_tests_properties(oracle_parity PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../corpus-tools")

# Negative corpus — every must-reject entry must be rejected by
# the C++ oracle. Loads spec/corpus/negative/index.json at runtime.
add_executable(oigtl_negative_corpus_test
    tests/negative_corpus_test.cpp
)
target_link_libraries(oigtl_negative_corpus_test PRIVATE oigtl)
add_test(NAME negative_corpus COMMAND oigtl_negative_corpus_test)
set_tests_properties(negative_corpus PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

# Oracle CLI — persistent subprocess for `oigtl-corpus fuzz differential`.
# Reads hex-encoded wire bytes from stdin, emits JSON reports on stdout.
# Not a test target; built but never invoked by ctest.
add_executable(oigtl_oracle_cli
    src/oracle_cli.cpp
)
target_link_libraries(oigtl_oracle_cli PRIVATE oigtl)

# Benchmark binary — built but not run by ctest. Invoke manually:
#   cmake --build build --target oigtl_bench && ./build/oigtl_bench
# Use a Release build for representative numbers:
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
add_executable(oigtl_bench
    benches/bench_codec.cpp
)
target_link_libraries(oigtl_bench PRIVATE oigtl)
target_include_directories(oigtl_bench PRIVATE
    "${IGTL_UPSTREAM_FIXTURES}")
