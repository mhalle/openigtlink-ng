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

# Generated per-message translation units. CONFIGURE_DEPENDS makes
# CMake re-glob on each build so a `codegen c` run picks up new
# files without manual reconfigure. If the glob is empty (e.g. fresh
# clone before first codegen), we add a placeholder TU so the
# STATIC library still has at least one input.
file(GLOB OIGTL_C_MESSAGE_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/src/messages/*.c")
add_library(oigtl_c_messages STATIC)
target_link_libraries(oigtl_c_messages PUBLIC oigtl_c_runtime)
if(OIGTL_C_MESSAGE_SOURCES)
    target_sources(oigtl_c_messages PRIVATE ${OIGTL_C_MESSAGE_SOURCES})
else()
    set(_oigtl_c_placeholder "${CMAKE_CURRENT_BINARY_DIR}/_oigtl_c_placeholder.c")
    file(WRITE "${_oigtl_c_placeholder}"
        "/* Placeholder — no generated message TUs yet.\n"
        " * Run: uv run oigtl-corpus codegen c\n"
        " */\n"
        "const int oigtl_c_messages_placeholder = 0;\n")
    target_sources(oigtl_c_messages PRIVATE "${_oigtl_c_placeholder}")
endif()

# ----------------------------------------------------------------------
# Fuzzers (opt-in, Clang-only).
#
# Configure with `-DOIGTL_C_BUILD_FUZZERS=ON` on a Clang toolchain
# to enable libFuzzer + ASan + UBSan targets. See core-c/fuzz/ for
# per-target entry points and seed corpus generator. Not wired into
# the regular ctest matrix — runs via the existing fuzz-smoke CI
# job under clang-15 / Linux.
# ----------------------------------------------------------------------
option(OIGTL_C_BUILD_FUZZERS
    "Build libFuzzer fuzz targets under ASan+UBSan (requires Clang)."
    OFF)

if(OIGTL_C_BUILD_FUZZERS)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR
            "OIGTL_C_BUILD_FUZZERS=ON requires Clang; got "
            "${CMAKE_C_COMPILER_ID}.")
    endif()
    set(OIGTL_C_FUZZ_FLAGS
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -g)

    foreach(_target fuzz_transform fuzz_status fuzz_position fuzz_sensor
                    fuzz_point)
        add_executable(${_target} fuzz/${_target}.c)
        target_link_libraries(${_target} PRIVATE oigtl_c_messages)
        target_compile_options(${_target} PRIVATE ${OIGTL_C_FUZZ_FLAGS})
        target_link_options(${_target} PRIVATE ${OIGTL_C_FUZZ_FLAGS})
    endforeach()

    # Seed corpus — populated at build time by a stdlib Python
    # script. Stdlib-only so the CI runner doesn't need uv to
    # build fuzzers.
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_custom_target(fuzz_c_seed_corpus
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/fuzz/seeds/gen.py"
            --out "${CMAKE_CURRENT_BINARY_DIR}/seeds"
        COMMENT "Staging core-c fuzz seed corpus under build/seeds/"
        VERBATIM)
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

# Round-trip tests per supported message shape. Only enabled when
# the codegen has actually produced output — otherwise there are no
# message headers to include.
if(OIGTL_C_MESSAGE_SOURCES)
    add_executable(oigtl_c_messages_test tests/messages_test.c)
    target_link_libraries(oigtl_c_messages_test PRIVATE oigtl_c_messages)
    target_include_directories(oigtl_c_messages_test PRIVATE include)
    add_test(NAME oigtl_c_messages COMMAND oigtl_c_messages_test)

    # Cross-port parity. The C emitter produces canonical wire bytes
    # for a set of named cases; a Python emitter produces the same
    # cases using `struct.pack` reading only the spec. If the two
    # disagree, either the spec is ambiguous or one implementation
    # is wrong — either way the CI fails loudly.
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_executable(oigtl_c_parity_emitter tests/parity_emitter.c)
    target_link_libraries(oigtl_c_parity_emitter PRIVATE oigtl_c_messages)
    target_include_directories(oigtl_c_parity_emitter PRIVATE include)

    set(OIGTL_C_PARITY_WORKDIR "${CMAKE_CURRENT_BINARY_DIR}/_parity")
    foreach(_case transform status position_full position_only sensor string
                  point)
        add_test(NAME oigtl_c_parity_${_case}
            COMMAND ${CMAKE_COMMAND}
                -DSHIM=$<TARGET_FILE:oigtl_c_parity_emitter>
                -DUPSTREAM=${Python3_EXECUTABLE}
                -DUPSTREAM_ARG0=${CMAKE_CURRENT_SOURCE_DIR}/tests/parity_emitter_py.py
                -DCASE=${_case}
                -DWORKDIR=${OIGTL_C_PARITY_WORKDIR}
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_parity_compare.cmake")
    endforeach()
endif()
