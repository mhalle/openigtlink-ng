# ----------------------------------------------------------------------
# core-c — minimal C codec for embedded consumers.
#
# Status: shared runtime + generated per-message TUs. No transport,
# no packaging, no `find_package` exports yet. See core-c/README.md
# for scope and plans.
#
# This CMakeLists is small by design — the point of the C target is
# to stay linkable as a handful of .c files, and we don't want the
# build system to accumulate options that would make that harder.
# ----------------------------------------------------------------------
cmake_minimum_required(VERSION 3.16)
project(oigtl_c C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Strict warnings for C; matches the core-cpp policy (but using
# C-appropriate flag names).
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror
                        # The generated per-message TUs intentionally
                        # assign to nested struct members in pack/unpack
                        # loops; the `off` cursor is written but not
                        # read on empty-body messages. Both are benign.
                        -Wno-unused-parameter
                        -Wno-unused-variable
                        -Wno-unused-but-set-variable)
elseif(MSVC)
    add_compile_options(/W4 /WX)
endif()

# ----------------------------------------------------------------------
# Shared runtime: hand-written, same on every target.
# ----------------------------------------------------------------------
add_library(oigtl_c_runtime STATIC
    src/crc64.c
    src/ascii.c
    src/header.c
)
target_include_directories(oigtl_c_runtime PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

# Placeholder — generated message TUs are added to oigtl_c_messages
# once the codegen has produced them. The target always exists so
# tests can link against a known name.
add_library(oigtl_c_messages STATIC)
target_link_libraries(oigtl_c_messages PUBLIC oigtl_c_runtime)
# CMake requires at least one source on a STATIC library; the codegen
# will push files here. Until then, give it an empty TU so configure
# succeeds on a fresh checkout.
if(NOT TARGET oigtl_c_messages_sources_populated)
    set(_oigtl_c_placeholder "${CMAKE_CURRENT_BINARY_DIR}/_oigtl_c_placeholder.c")
    # An exported (non-static) symbol satisfies the linker's "archive
    # must have at least one object" rule without tripping -Werror's
    # unused-function diagnostic. The dummy variable gets stripped
    # at link time on any real consumer.
    file(WRITE "${_oigtl_c_placeholder}"
        "/* Auto-generated placeholder. Removed when the first real\n"
        " * message TU lands via the codegen. */\n"
        "const int oigtl_c_messages_placeholder = 0;\n")
    target_sources(oigtl_c_messages PRIVATE "${_oigtl_c_placeholder}")
endif()

# ----------------------------------------------------------------------
# Tests
# ----------------------------------------------------------------------
enable_testing()

# Tiny runtime smoke — CRC parity against a known vector + header
# round-trip. Proves the hand-written runtime works independently of
# the codegen, so a generator bug doesn't mask a runtime bug.
add_executable(oigtl_c_runtime_test tests/runtime_test.c)
target_link_libraries(oigtl_c_runtime_test PRIVATE oigtl_c_runtime)
add_test(NAME oigtl_c_runtime COMMAND oigtl_c_runtime_test)
